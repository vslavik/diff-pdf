
#include "bmpviewer.h"

#include <stdio.h>
#include <assert.h>

#include <vector>

#include <glib.h>
#include <poppler.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>

// ------------------------------------------------------------------------
// PDF rendering functions
// ------------------------------------------------------------------------

bool g_verbose = true;

// Resolution to use for rasterization, in DPI
#define RESOLUTION  300

cairo_surface_t *render_page(PopplerPage *page)
{
    double w, h;
    poppler_page_get_size(page, &w, &h);

    const int w_px = int(RESOLUTION * w / 72.0);
    const int h_px = int(RESOLUTION * h / 72.0);

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, w_px, h_px);

    cairo_t *cr = cairo_create(surface);

    // clear the surface to white background:
    cairo_save(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, 0, 0, w_px, h_px);
    cairo_fill(cr);
    cairo_restore(cr);

    // Scale so that PDF output covers the whole surface. Image surface is
    // created with transformation set up so that 1 coordinate unit is 1 pixel;
    // Poppler assumes 1 unit = 1 point.
    cairo_scale(cr, RESOLUTION / 72.0, RESOLUTION / 72.0);

    poppler_page_render(page, cr);

    cairo_show_page(cr);

    cairo_destroy(cr);

    return surface;
}


cairo_surface_t *diff_images(cairo_surface_t *s1, cairo_surface_t *s2)
{
    assert( s1 || s2 );

    const int width = cairo_image_surface_get_width(s1 ? s1 : s2);
    const int height = cairo_image_surface_get_height(s1 ? s1 : s2);

    // FIXME: handle pages of different sizes
    assert( width == cairo_image_surface_get_width(s2) );
    assert( height == cairo_image_surface_get_height(s2) );


    // deal with pages present in only one document first, by creating a dummy
    // page for this case
    cairo_surface_t *dummy = NULL;
    if ( !s1 || !s2 )
    {
        dummy = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

        // clear the surface to white background:
        cairo_t *cr = cairo_create(dummy);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
        cairo_destroy(cr);

        if ( !s1 )
            s1 = dummy;
        if ( !s2 )
            s2 = dummy;
    }


    bool changes = false;

    cairo_surface_t *diff =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

    const int stride1 = cairo_image_surface_get_stride(s1);
    const int stride2 = cairo_image_surface_get_stride(s2);
    const int stridediff = cairo_image_surface_get_stride(diff);

    const unsigned char *data1 = cairo_image_surface_get_data(s1);
    const unsigned char *data2 = cairo_image_surface_get_data(s2);
    unsigned char *datadiff = cairo_image_surface_get_data(diff);

    for ( int y = 0;
          y < height;
          y++, data1 += stride1, data2 += stride2, datadiff += stridediff )
    {
        for ( int x = 0; x < width * 4; x += 4 )
        {
            unsigned char r1 = *(data1 + x + 0);
            unsigned char g1 = *(data1 + x + 1);
            unsigned char b1 = *(data1 + x + 2);

            unsigned char r2 = *(data2 + x + 0);
            unsigned char g2 = *(data2 + x + 1);
            unsigned char b2 = *(data2 + x + 2);

            if ( r1 != r2 || g1 != g2 || b1 != b2 )
                changes = true;


            // we visualize the differences by taking one channel from s1
            // and the other two channels from s2
            *(datadiff + x + 0) = r1;
            *(datadiff + x + 1) = g1;
            *(datadiff + x + 2) = b2;
        }
    }

    if ( dummy )
        cairo_surface_destroy(dummy);

    if ( changes )
    {
        return diff;
    }
    else
    {
        cairo_surface_destroy(diff);
        return NULL;
    }
}


bool page_compare(cairo_t *cr_out,
                  int page_index, PopplerPage *page1, PopplerPage *page2)
{
    cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
    cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;

    cairo_surface_t *diff = diff_images(img1, img2);

    if ( diff && g_verbose )
        printf("page %d differs\n", page_index);

    if ( cr_out )
    {
        if ( diff )
        {
            // render the difference as high-resolution bitmap

            cairo_save(cr_out);
            cairo_scale(cr_out, 72.0 / RESOLUTION, 72.0 / RESOLUTION);

            cairo_set_source_surface(cr_out, diff ? diff : img1, 0, 0);
            cairo_paint(cr_out);

            cairo_restore(cr_out);

            cairo_surface_destroy(diff);
        }
        else
        {
            // save space (as well as improve rendering quality) in diff pdf
            // by writing unchanged pages in their original form rather than
            // a rasterized one

            poppler_page_render(page1, cr_out);
        }

        cairo_show_page(cr_out);
    }

    if ( img1 )
        cairo_surface_destroy(img1);
    if ( img2 )
        cairo_surface_destroy(img2);

    return diff == NULL;
}


// Compares two documents, writing diff PDF into file named 'pdf_output' if
// not NULL. if 'differences' is not NULL, puts a map of which pages differ
// into it.
bool doc_compare(PopplerDocument *doc1, PopplerDocument *doc2,
                 const char *pdf_output,
                 std::vector<bool> *differences)
{
    bool are_same = true;

    double w, h;
    poppler_page_get_size(poppler_document_get_page(doc1, 0), &w, &h);

    cairo_surface_t *surface_out = NULL;
    cairo_t *cr_out = NULL;

    if ( pdf_output )
    {
        surface_out = cairo_pdf_surface_create(pdf_output, w, h);
        cr_out = cairo_create(surface_out);
    }

    int pages1 = poppler_document_get_n_pages(doc1);
    int pages2 = poppler_document_get_n_pages(doc2);
    int pages_total = pages1 > pages2 ? pages1 : pages2;

    if ( pages1 != pages2 )
    {
        if ( g_verbose )
            printf("pages count differs: %d vs %d\n", pages1, pages2);
        are_same = false;
    }

    for ( int page = 0; page < pages_total; page++ )
    {
        PopplerPage *page1 = page < pages1
                             ? poppler_document_get_page(doc1, page)
                             : NULL;
        PopplerPage *page2 = page < pages2
                             ? poppler_document_get_page(doc2, page)
                             : NULL;

        const bool page_same = page_compare(cr_out, page, page1, page2);
        if ( differences )
            differences->push_back(!page_same);

        if ( !page_same )
            are_same = false;
    }

    if ( pdf_output )
    {
        cairo_destroy(cr_out);
        cairo_surface_destroy(surface_out);
    }

    return are_same;
}


// ------------------------------------------------------------------------
// wxWidgets GUI
// ------------------------------------------------------------------------

const int ID_PREV_PAGE = wxNewId();
const int ID_NEXT_PAGE = wxNewId();
const int ID_ZOOM_IN = wxNewId();
const int ID_ZOOM_OUT = wxNewId();

#define BMP_ARTPROV(id) wxArtProvider::GetBitmap(id, wxART_TOOLBAR)

#define BMP_PREV_PAGE     BMP_ARTPROV(wxART_GO_BACK)
#define BMP_NEXT_PAGE     BMP_ARTPROV(wxART_GO_FORWARD)

#ifdef __WXGTK__
    #define BMP_ZOOM_IN   BMP_ARTPROV(wxT("gtk-zoom-in"))
    #define BMP_ZOOM_OUT  BMP_ARTPROV(wxT("gtk-zoom-out"))
#else
#endif


class DiffFrame : public wxFrame
{
public:
    DiffFrame(const wxString& title)
        : wxFrame(NULL, wxID_ANY, title)
    {
        m_cur_page = -1;

        CreateStatusBar(2);
        SetStatusBarPane(0);
        const int stat_widths[] = { -1, 80 };
        SetStatusWidths(2, stat_widths);

        wxToolBar *toolbar =
            new wxToolBar
                (
                    this, wxID_ANY,
                    wxDefaultPosition, wxDefaultSize,
                    wxTB_HORIZONTAL | wxTB_FLAT | wxTB_HORZ_TEXT
                );
        toolbar->AddTool(ID_PREV_PAGE, wxT("Previous"), BMP_PREV_PAGE);
        toolbar->AddTool(ID_NEXT_PAGE, wxT("Next"), BMP_NEXT_PAGE);
        toolbar->AddTool(ID_ZOOM_IN, wxT("Zoom in"), BMP_ZOOM_IN);
        toolbar->AddTool(ID_ZOOM_OUT, wxT("Zoom out"), BMP_ZOOM_OUT);

        SetToolBar(toolbar);

        m_viewer = new BitmapViewer(this);

        wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(m_viewer, wxSizerFlags(1).Expand());
        SetSizer(sizer);
    }

    void SetDocs(PopplerDocument *doc1, PopplerDocument *doc2)
    {
        wxBusyCursor wait;

        m_doc1 = doc1;
        m_doc2 = doc2;

        doc_compare(m_doc1, m_doc2, NULL, &m_pages);

        m_diff_count = 0;
        for ( std::vector<bool>::const_iterator i = m_pages.begin();
              i != m_pages.end();
              ++i )
        {
            if ( *i )
                m_diff_count++;
        }

        GoToPage(0);

        m_viewer->SetBestFitZoom();
        UpdateStatus();
    }

    void GoToPage(int n)
    {
        wxBusyCursor wait;

        m_cur_page = n;

        const int pages1 = poppler_document_get_n_pages(m_doc1);
        const int pages2 = poppler_document_get_n_pages(m_doc2);

        PopplerPage *page1 = n < pages1
                             ? poppler_document_get_page(m_doc1, n)
                             : NULL;
        PopplerPage *page2 = n < pages2
                             ? poppler_document_get_page(m_doc2, n)
                             : NULL;

        cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
        cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;
        cairo_surface_t *diff = diff_images(img1, img2);

        if ( diff )
            m_viewer->Set(diff);
        else
            m_viewer->Set(img1);

        if ( img1 )
            cairo_surface_destroy(img1);
        if ( img2 )
            cairo_surface_destroy(img2);
        if ( diff )
            cairo_surface_destroy(diff);

        UpdateStatus();
    }

private:
    static const float ZOOM_FACTOR_STEP = 1.2;

    void UpdateStatus()
    {
        SetStatusText
        (
            wxString::Format
            (
                wxT("Page %d of %d; %d of them are different, this page %s"),
                m_cur_page + 1 /* humans prefer 1-based counting*/,
                m_pages.size(),
                m_diff_count,
                m_pages[m_cur_page] ? wxT("differs") : wxT("is unchanged")
            ),
            0
        );

        SetStatusText
        (
            wxString::Format(wxT("%.1f%%"), m_viewer->GetZoom() * 100.0),
            1
        );
    }

    void OnPrevPage(wxCommandEvent&)
    {
        GoToPage(m_cur_page - 1);
    }

    void OnNextPage(wxCommandEvent&)
    {
        GoToPage(m_cur_page + 1);
    }

    void OnUpdatePrevPage(wxUpdateUIEvent& event)
    {
        event.Enable(m_cur_page > 0);
    }

    void OnUpdateNextPage(wxUpdateUIEvent& event)
    {
        event.Enable(m_cur_page < m_pages.size() - 1);
    }

    void OnZoomIn(wxCommandEvent&)
    {
        wxBusyCursor wait;
        m_viewer->SetZoom(m_viewer->GetZoom() * ZOOM_FACTOR_STEP);
        UpdateStatus();
    }

    void OnZoomOut(wxCommandEvent&)
    {
        wxBusyCursor wait;
        m_viewer->SetZoom(m_viewer->GetZoom() / ZOOM_FACTOR_STEP);
        UpdateStatus();
    }

    DECLARE_EVENT_TABLE()

private:
    BitmapViewer *m_viewer;
    PopplerDocument *m_doc1, *m_doc2;
    std::vector<bool> m_pages;
    int m_diff_count;
    int m_cur_page;
};

BEGIN_EVENT_TABLE(DiffFrame, wxFrame)
    EVT_TOOL     (ID_PREV_PAGE, DiffFrame::OnPrevPage)
    EVT_TOOL     (ID_NEXT_PAGE, DiffFrame::OnNextPage)
    EVT_UPDATE_UI(ID_PREV_PAGE, DiffFrame::OnUpdatePrevPage)
    EVT_UPDATE_UI(ID_NEXT_PAGE, DiffFrame::OnUpdateNextPage)
    EVT_TOOL     (ID_ZOOM_IN, DiffFrame::OnZoomIn)
    EVT_TOOL     (ID_ZOOM_OUT, DiffFrame::OnZoomOut)
END_EVENT_TABLE()


class DiffPdfApp : public wxApp
{
public:
    virtual bool OnInit()
    {
        DiffFrame *tlw = new DiffFrame(m_title);

        // like in LMI, maximize the window
        tlw->Maximize();
        tlw->Show();

        // yield so that size changes above take effect immediately (and so we
        // can query the window for its size)
        Yield();

        tlw->SetDocs(m_doc1, m_doc2);

        return true;
    }

    void SetData(const wxString& file1, PopplerDocument *doc1,
                 const wxString& file2, PopplerDocument *doc2)
    {
        m_title = wxString::Format(wxT("Differences between %s and %s"), file1.c_str(), file2.c_str());
        m_doc1 = doc1;
        m_doc2 = doc2;
    }

private:
    wxString m_title;
    PopplerDocument *m_doc1, *m_doc2;
};

IMPLEMENT_APP_NO_MAIN(DiffPdfApp);


// ------------------------------------------------------------------------
// main()
// ------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    wxAppConsole::CheckBuildOptions(WX_BUILD_OPTIONS_SIGNATURE, "diff-pdf");
    wxInitializer wxinitializer(argc, argv);

    g_type_init();

    static const wxCmdLineEntryDesc cmd_line_desc[] =
    {
        { wxCMD_LINE_SWITCH,
                  wxT("h"), wxT("help"), wxT("show this help message"),
                  wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },

        { wxCMD_LINE_SWITCH,
                  wxT("v"), wxT("verbose"), wxT("be verbose") },

        { wxCMD_LINE_OPTION,
                  NULL, wxT("pdf"), wxT("output differences to given PDF file"),
                  wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_SWITCH,
                  NULL, wxT("view"), wxT("view the differences in a window") },

        { wxCMD_LINE_PARAM,
                  NULL, NULL, wxT("file1.pdf"), wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_PARAM,
                  NULL, NULL, wxT("file2.pdf"), wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_NONE }
    };

    wxCmdLineParser parser(cmd_line_desc, argc, argv);
    switch ( parser.Parse() )
    {
        case -1: // --help
            return 0;

        case 0: // everything is ok; proceed
            break;

        default: // syntax error
            return 2;
    }

    wxFileName file1(parser.GetParam(0));
    wxFileName file2(parser.GetParam(1));
    file1.MakeAbsolute();
    file2.MakeAbsolute();
    const wxString url1 = wxT("file://") + file1.GetFullPath(wxPATH_UNIX);
    const wxString url2 = wxT("file://") + file2.GetFullPath(wxPATH_UNIX);

    GError *err;

    PopplerDocument *doc1 = poppler_document_new_from_file(url1.utf8_str(), NULL, &err);
    if ( !doc1 )
    {
        fprintf(stderr, "Error opening %s: %s\n", argv[1], err->message);
        g_object_unref(err);
        return 3;
    }

    PopplerDocument *doc2 = poppler_document_new_from_file(url2.utf8_str(), NULL, &err);
    if ( !doc2 )
    {
        fprintf(stderr, "Error opening %s: %s\n", argv[2], err->message);
        g_object_unref(err);
        return 3;
    }

    int retval = 0;

    wxString pdf_file;
    if ( parser.Found(wxT("pdf"), &pdf_file) )
    {
        retval = doc_compare(doc1, doc2, pdf_file.utf8_str(), NULL) ? 0 : 1;
    }
    else if ( parser.Found(wxT("view")) )
    {
        wxGetApp().SetData(parser.GetParam(0), doc1,
                           parser.GetParam(1), doc2);
        retval = wxEntry(argc, argv);
    }
    else
    {
        retval = doc_compare(doc1, doc2, NULL, NULL) ? 0 : 1;
    }

    g_object_unref(doc1);
    g_object_unref(doc2);

    return retval;
}
