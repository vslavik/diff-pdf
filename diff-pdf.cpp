/*
 * This file is part of diff-pdf.
 *
 * Copyright (C) 2009 TT-Solutions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bmpviewer.h"
#include "gutter.h"

#include <stdio.h>
#include <assert.h>

#include <vector>

#include <glib.h>
#include <poppler.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>

#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/progdlg.h>
#include <wx/filesys.h>

// ------------------------------------------------------------------------
// PDF rendering functions
// ------------------------------------------------------------------------

bool g_verbose = false;

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


// Creates image of differences between s1 and s2. If the offset is specified,
// then s2 is displaced by it. If thumbnail and thumbnail_width are specified,
// then a thumbnail with highlighted differences is created too.
cairo_surface_t *diff_images(cairo_surface_t *s1, cairo_surface_t *s2,
                             int offset_x = 0, int offset_y = 0,
                             wxImage *thumbnail = NULL, int thumbnail_width = -1)
{
    assert( s1 || s2 );

    wxRect r1, r2;

    if ( s1 )
    {
        r1 = wxRect(0, 0,
                    cairo_image_surface_get_width(s1),
                    cairo_image_surface_get_height(s1));
    }
    if ( s2 )
    {
        r2 = wxRect(offset_x, offset_y,
                    cairo_image_surface_get_width(s2),
                    cairo_image_surface_get_height(s2));
    }

    // compute union rectangle starting at [0,0] position
    wxRect rdiff(r1);
    rdiff.Union(r2);
    r1.Offset(-rdiff.x, -rdiff.y);
    r2.Offset(-rdiff.x, -rdiff.y);
    rdiff.Offset(-rdiff.x, -rdiff.y);

    bool changes = false;

    cairo_surface_t *diff =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, rdiff.width, rdiff.height);

    float thumbnail_scale;
    int thumbnail_height;

    if ( thumbnail )
    {
        thumbnail_scale = float(thumbnail_width) / float(rdiff.width);
        thumbnail_height = int(rdiff.height * thumbnail_scale);
        thumbnail->Create(thumbnail_width, thumbnail_height);
        thumbnail->SetRGB(wxRect(), 255, 255, 255);
    }

    // clear the surface to white background if the merged images don't fully
    // overlap:
    if ( r1 != r2 )
    {
        changes = true;

        cairo_t *cr = cairo_create(diff);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_rectangle(cr, 0, 0, rdiff.width, rdiff.height);
        cairo_fill(cr);
        cairo_destroy(cr);
    }

    const int stride1 = s1 ? cairo_image_surface_get_stride(s1) : 0;
    const int stride2 = s2 ? cairo_image_surface_get_stride(s2) : 0;
    const int stridediff = cairo_image_surface_get_stride(diff);

    const unsigned char *data1 = s1 ? cairo_image_surface_get_data(s1) : NULL;
    const unsigned char *data2 = s2 ? cairo_image_surface_get_data(s2) : NULL;
    unsigned char *datadiff = cairo_image_surface_get_data(diff);

    // we visualize the differences by taking one channel from s1
    // and the other two channels from s2:

    // first, copy s1 over:
    if ( s1 )
    {
        unsigned char *out = datadiff + r1.y * stridediff + r1.x * 4;
        for ( int y = 0;
              y < r1.height;
              y++, data1 += stride1, out += stridediff )
        {
            memcpy(out, data1, r1.width * 4);
        }
    }

    // then, copy B channel from s2 over it; also compare the two versions
    // to see if there are any differences:
    if ( s2 )
    {
        unsigned char *out = datadiff + r2.y * stridediff + r2.x * 4;
        for ( int y = 0;
              y < r2.height;
              y++, data2 += stride2, out += stridediff )
        {
            for ( int x = 0; x < r2.width * 4; x += 4 )
            {
                unsigned char cr1 = *(out + x + 0);
                unsigned char cg1 = *(out + x + 1);
                unsigned char cb1 = *(out + x + 2);

                unsigned char cr2 = *(data2 + x + 0);
                unsigned char cg2 = *(data2 + x + 1);
                unsigned char cb2 = *(data2 + x + 2);

                if ( cr1 != cr2 || cg1 != cg2 || cb1 != cb2 )
                {
                    changes = true;
                    if ( thumbnail )
                    {
                        // mark changes with red
                        thumbnail->SetRGB
                                  (
                                      int((r2.x + x/4) * thumbnail_scale),
                                      int((r2.y + y) * thumbnail_scale),
                                      255, 0, 0
                                  );
                    }
                }

                // change the B channel to be from s2; RG will be s1
                *(out + x + 2) = cb2;
            }
        }
    }

    // add background image of the page to the thumbnails
    if ( thumbnail )
    {
        // copy the 'diff' surface into wxImage:
        wxImage bg(rdiff.width, rdiff.height);
        unsigned char *in = datadiff;
        unsigned char *out = bg.GetData();
        for ( int y = 0; y < rdiff.height; y++, in += stridediff )
        {
            for ( int x = 0; x < rdiff.width * 4; x += 4 )
            {
                // cairo_surface_t uses BGR order, wxImage has RGB
                *(out++) = *(in + x + 2);
                *(out++) = *(in + x + 1);
                *(out++) = *(in + x + 0);
            }
        }

        // scale it to thumbnail size:
        bg.Rescale(thumbnail_width, thumbnail_height, wxIMAGE_QUALITY_HIGH);

        // and merge with the diff markers in *thumbnail, making it much
        // lighter in the process:
        in = bg.GetData();
        out = thumbnail->GetData();
        for ( int i = thumbnail_width * thumbnail_height; i > 0; i-- )
        {
            if ( out[1] == 0 ) // G=0 ==> not white
            {
                // marked with red color, as place with differences -- don't
                // paint background image here, make the red as visible as
                // possible
                out += 3;
                in += 3;
            }
            else
            {
                // merge in lighter background image
                *(out++) = 128 + *(in++) / 2;
                *(out++) = 128 + *(in++) / 2;
                *(out++) = 128 + *(in++) / 2;
            }
        }

        // If there were no changes, indicate it by using green
        // (170,230,130) color for the thumbnail in gutter control:
        if ( !changes )
        {
            out = thumbnail->GetData();
            for ( int i = thumbnail_width * thumbnail_height;
                  i > 0;
                  i--, out += 3 )
            {
                out[0] = 170/2 + out[0] / 2;
                out[1] = 230/2 + out[1] / 2;
                out[2] = 130/2 + out[2] / 2;
            }
        }
    }

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


// Compares given two pages. If cr_out is not NULL, then the diff image (either
// differences or unmodified page, if there are no diffs) is drawn to it.
// If thumbnail and thumbnail_width are specified, then a thumbnail with
// highlighted differences is created too.
bool page_compare(cairo_t *cr_out,
                  PopplerPage *page1, PopplerPage *page2,
                  wxImage *thumbnail = NULL, int thumbnail_width = -1)
{
    cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
    cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;

    cairo_surface_t *diff = diff_images(img1, img2, 0, 0,
                                        thumbnail, thumbnail_width);
    const bool has_diff = (diff != NULL);

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

    if ( diff )
        cairo_surface_destroy(diff);

    if ( img1 )
        cairo_surface_destroy(img1);
    if ( img2 )
        cairo_surface_destroy(img2);

    return !has_diff;
}


// Compares two documents, writing diff PDF into file named 'pdf_output' if
// not NULL. if 'differences' is not NULL, puts a map of which pages differ
// into it. If 'progress' is provided, it is updated to reflect comparison's
// progress. If 'gutter' is set, then all the pages are added to it, with
// their respective thumbnails (the gutter must be empty beforehand).
bool doc_compare(PopplerDocument *doc1, PopplerDocument *doc2,
                 const char *pdf_output,
                 std::vector<bool> *differences,
                 wxProgressDialog *progress = NULL,
                 Gutter *gutter = NULL)
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
        if ( progress )
        {
            progress->Update
                      (
                          page,
                          wxString::Format
                          (
                              wxT("Comparing page %d of %d..."),
                              page+1,
                              pages_total
                          )
                       );
        }

        poppler_page_get_size(poppler_document_get_page(doc1, page), &w, &h);
        if ( pdf_output )
        {
           cairo_pdf_surface_set_size (surface_out, w, h);
        }

        PopplerPage *page1 = page < pages1
                             ? poppler_document_get_page(doc1, page)
                             : NULL;
        PopplerPage *page2 = page < pages2
                             ? poppler_document_get_page(doc2, page)
                             : NULL;

        bool page_same;

        if ( gutter )
        {
            wxImage thumbnail;
            page_same = page_compare(cr_out, page1, page2,
                                     &thumbnail, Gutter::WIDTH);

            wxString label1(wxT("(null)"));
            wxString label2(wxT("(null)"));

            if ( page1 )
            {
                gchar *label;
                g_object_get(page1, "label", &label, NULL);
                label1 = wxString::FromUTF8(label);
                g_free(label);
            }
            if ( page2 )
            {
                gchar *label;
                g_object_get(page2, "label", &label, NULL);
                label2 = wxString::FromUTF8(label);
                g_free(label);
            }


            wxString label;
            if ( label1 == label2 )
                label = label1;
            else
                label = label1 + wxT(" / ") + label2;

            gutter->AddPage(label, thumbnail);
        }
        else
        {
            page_same = page_compare(cr_out, page1, page2);
        }

        if ( differences )
            differences->push_back(!page_same);

        if ( !page_same )
        {
            are_same = false;

            if ( g_verbose )
                printf("page %d differs\n", page);

            // If we don't need to output all different pages in any
            // form (including verbose report of differing pages!), then
            // we can stop comparing the PDFs as soon as we find the first
            // difference.
            if ( !g_verbose && !pdf_output && !differences && !gutter )
                break;
        }
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
const int ID_OFFSET_LEFT = wxNewId();
const int ID_OFFSET_RIGHT = wxNewId();
const int ID_OFFSET_UP = wxNewId();
const int ID_OFFSET_DOWN = wxNewId();
const int ID_GUTTER = wxNewId();

#define BMP_ARTPROV(id) wxArtProvider::GetBitmap(id, wxART_TOOLBAR)

#define BMP_PREV_PAGE      BMP_ARTPROV(wxART_GO_BACK)
#define BMP_NEXT_PAGE      BMP_ARTPROV(wxART_GO_FORWARD)

#define BMP_OFFSET_LEFT    BMP_ARTPROV(wxART_GO_BACK)
#define BMP_OFFSET_RIGHT   BMP_ARTPROV(wxART_GO_FORWARD)
#define BMP_OFFSET_UP      BMP_ARTPROV(wxART_GO_UP)
#define BMP_OFFSET_DOWN    BMP_ARTPROV(wxART_GO_DOWN)

#ifdef __WXGTK__
    #define BMP_ZOOM_IN    BMP_ARTPROV(wxT("gtk-zoom-in"))
    #define BMP_ZOOM_OUT   BMP_ARTPROV(wxT("gtk-zoom-out"))
#else
    #include "gtk-zoom-in.xpm"
    #include "gtk-zoom-out.xpm"
    #define BMP_ZOOM_IN    wxBitmap(gtk_zoom_in_xpm)
    #define BMP_ZOOM_OUT   wxBitmap(gtk_zoom_out_xpm)
#endif

static const float ZOOM_FACTOR_STEP = 1.2f;

class DiffFrame : public wxFrame
{
public:
    DiffFrame(const wxString& title)
        : wxFrame(NULL, wxID_ANY, title)
    {
        m_cur_page = -1;

        CreateStatusBar(2);
        SetStatusBarPane(0);
        const int stat_widths[] = { -1, 150 };
        SetStatusWidths(2, stat_widths);

        wxToolBar *toolbar =
            new wxToolBar
                (
                    this, wxID_ANY,
                    wxDefaultPosition, wxDefaultSize,
                    wxTB_HORIZONTAL | wxTB_FLAT | wxTB_HORZ_TEXT
                );

        toolbar->AddTool(ID_PREV_PAGE, wxT("Previous"), BMP_PREV_PAGE,
                         wxT("Go to previous page (PgUp)"));
        toolbar->AddTool(ID_NEXT_PAGE, wxT("Next"), BMP_NEXT_PAGE,
                         wxT("Go to next page (PgDown)"));
        toolbar->AddTool(ID_ZOOM_IN, wxT("Zoom in"), BMP_ZOOM_IN,
                         wxT("Make the page larger (Ctrl +)"));
        toolbar->AddTool(ID_ZOOM_OUT, wxT("Zoom out"), BMP_ZOOM_OUT,
                         wxT("Make the page smaller (Ctrl -)"));
        toolbar->AddTool(ID_OFFSET_LEFT, wxT(""), BMP_OFFSET_LEFT,
                         wxT("Offset one of the pages to the left (Ctrl left)"));
        toolbar->AddTool(ID_OFFSET_RIGHT, wxT(""), BMP_OFFSET_RIGHT,
                         wxT("Offset one of the pages to the right (Ctrl right)"));
        toolbar->AddTool(ID_OFFSET_UP, wxT(""), BMP_OFFSET_UP,
                         wxT("Offset one of the pages up (Ctrl up)"));
        toolbar->AddTool(ID_OFFSET_DOWN, wxT(""), BMP_OFFSET_DOWN,
                         wxT("Offset one of the pages down (Ctrl down)"));

        toolbar->Realize();
        SetToolBar(toolbar);

        wxAcceleratorEntry accels[8];
        accels[0].Set(wxACCEL_NORMAL, WXK_PAGEUP, ID_PREV_PAGE);
        accels[1].Set(wxACCEL_NORMAL, WXK_PAGEDOWN, ID_NEXT_PAGE);
        accels[2].Set(wxACCEL_CTRL, (int)'=', ID_ZOOM_IN);
        accels[3].Set(wxACCEL_CTRL, (int)'-', ID_ZOOM_OUT);
        accels[4].Set(wxACCEL_CTRL, WXK_LEFT, ID_OFFSET_LEFT);
        accels[5].Set(wxACCEL_CTRL, WXK_RIGHT, ID_OFFSET_RIGHT);
        accels[6].Set(wxACCEL_CTRL, WXK_UP, ID_OFFSET_UP);
        accels[7].Set(wxACCEL_CTRL, WXK_DOWN, ID_OFFSET_DOWN);

        wxAcceleratorTable accel_table(8, accels);
        SetAcceleratorTable(accel_table);

        m_gutter = new Gutter(this, ID_GUTTER);

        m_viewer = new BitmapViewer(this);
        m_viewer->AttachGutter(m_gutter);
        m_viewer->SetFocus();

        wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(m_gutter, wxSizerFlags(0).Expand().Border(wxALL, 2));
        sizer->Add(m_viewer, wxSizerFlags(1).Expand());
        SetSizer(sizer);
    }

    void SetDocs(PopplerDocument *doc1, PopplerDocument *doc2)
    {
        m_doc1 = doc1;
        m_doc2 = doc2;

        wxProgressDialog progress(wxT("Comparing documents"),
                                  wxT("Comparing documents..."),
                                  wxMax(poppler_document_get_n_pages(m_doc1),
                                        poppler_document_get_n_pages(m_doc2)),
                                  this,
                                  wxPD_SMOOTH | wxPD_REMAINING_TIME);


        doc_compare(m_doc1, m_doc2, NULL, &m_pages, &progress, m_gutter);

        progress.Pulse();

        m_diff_count = 0;
        for ( std::vector<bool>::const_iterator i = m_pages.begin();
              i != m_pages.end();
              ++i )
        {
            if ( *i )
                m_diff_count++;
        }

        GoToPage(0);

        progress.Pulse();

        m_viewer->SetBestFitZoom();
        UpdateStatus();

        progress.Hide();
    }

    void GoToPage(int n)
    {
        m_cur_page = n;
        m_gutter->SetSelection(n);
        DoUpdatePage();
    }

private:
    void DoUpdatePage()
    {
        wxBusyCursor wait;

        const int pages1 = poppler_document_get_n_pages(m_doc1);
        const int pages2 = poppler_document_get_n_pages(m_doc2);

        PopplerPage *page1 = m_cur_page < pages1
                             ? poppler_document_get_page(m_doc1, m_cur_page)
                             : NULL;
        PopplerPage *page2 = m_cur_page < pages2
                             ? poppler_document_get_page(m_doc2, m_cur_page)
                             : NULL;

        cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
        cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;

        wxImage thumbnail;
        cairo_surface_t *diff = diff_images
                                (
                                    img1, img2,
                                    m_offset.x, m_offset.y,
                                    &thumbnail, Gutter::WIDTH
                                );

        m_viewer->Set(diff ? diff : img1);

        // Always update the diff map. It will be all-white if there were
        // no differences.
        m_gutter->SetThumbnail(m_cur_page, thumbnail);

        if ( img1 )
            cairo_surface_destroy(img1);
        if ( img2 )
            cairo_surface_destroy(img2);
        if ( diff )
            cairo_surface_destroy(diff);

        UpdateStatus();
    }

    void UpdateStatus()
    {
        SetStatusText
        (
            wxString::Format
            (
                wxT("Page %d of %d; %d of them %s different, this page %s"),
                m_cur_page + 1 /* humans prefer 1-based counting*/,
                (int)m_pages.size(),
                m_diff_count,
                m_diff_count == 1 ? wxT("is") : wxT("are"),
                m_pages[m_cur_page] ? wxT("differs") : wxT("is unchanged")
            ),
            0
        );

        SetStatusText
        (
            wxString::Format
            (
                wxT("%.1f%% [offset %d,%d]"),
                m_viewer->GetZoom() * 100.0,
                m_offset.x, m_offset.y
            ),
            1
        );
    }

    void OnSetPage(wxCommandEvent& event)
    {
        GoToPage(event.GetSelection());
    }

    void OnPrevPage(wxCommandEvent&)
    {
        if ( m_cur_page > 0 )
            GoToPage(m_cur_page - 1);
    }

    void OnNextPage(wxCommandEvent&)
    {
        if ( m_cur_page < m_pages.size() - 1 )
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

    void DoOffset(int x, int y)
    {
        m_offset.x += x;
        m_offset.y += y;
        DoUpdatePage();
    }

    void OnOffsetLeft(wxCommandEvent&) { DoOffset(-1, 0); }
    void OnOffsetRight(wxCommandEvent&) { DoOffset(1, 0); }
    void OnOffsetUp(wxCommandEvent&) { DoOffset(0, -1); }
    void OnOffsetDown(wxCommandEvent&) { DoOffset(0, 1); }

    DECLARE_EVENT_TABLE()

private:
    BitmapViewer *m_viewer;
    Gutter *m_gutter;
    PopplerDocument *m_doc1, *m_doc2;
    std::vector<bool> m_pages;
    int m_diff_count;
    int m_cur_page;
    wxPoint m_offset;
};

BEGIN_EVENT_TABLE(DiffFrame, wxFrame)
    EVT_LISTBOX  (ID_GUTTER,       DiffFrame::OnSetPage)
    EVT_TOOL     (ID_PREV_PAGE,    DiffFrame::OnPrevPage)
    EVT_TOOL     (ID_NEXT_PAGE,    DiffFrame::OnNextPage)
    EVT_UPDATE_UI(ID_PREV_PAGE,    DiffFrame::OnUpdatePrevPage)
    EVT_UPDATE_UI(ID_NEXT_PAGE,    DiffFrame::OnUpdateNextPage)
    EVT_TOOL     (ID_ZOOM_IN,      DiffFrame::OnZoomIn)
    EVT_TOOL     (ID_ZOOM_OUT,     DiffFrame::OnZoomOut)
    EVT_TOOL     (ID_OFFSET_LEFT,  DiffFrame::OnOffsetLeft)
    EVT_TOOL     (ID_OFFSET_RIGHT, DiffFrame::OnOffsetRight)
    EVT_TOOL     (ID_OFFSET_UP,    DiffFrame::OnOffsetUp)
    EVT_TOOL     (ID_OFFSET_DOWN,  DiffFrame::OnOffsetDown)
END_EVENT_TABLE()


class DiffPdfApp : public wxApp
{
public:
    DiffPdfApp() : m_tlw(NULL) {}

    virtual bool OnInit()
    {
        m_tlw = new DiffFrame(m_title);

        // like in LMI, maximize the window
        m_tlw->Maximize();
        m_tlw->Show();

        // yield so that size changes above take effect immediately (and so we
        // can query the window for its size)
        Yield();

#if !wxCHECK_VERSION(2,9,0)
        SetFrameDocs();
#endif

        return true;
    }

    void SetData(const wxString& file1, PopplerDocument *doc1,
                 const wxString& file2, PopplerDocument *doc2)
    {
        m_title = wxString::Format(wxT("Differences between %s and %s"), file1.c_str(), file2.c_str());
        m_doc1 = doc1;
        m_doc2 = doc2;
    }

protected:
#if wxCHECK_VERSION(2,9,0)
    virtual void OnEventLoopEnter(wxEventLoopBase *loop)
    {
        wxApp::OnEventLoopEnter(loop);

        if ( loop->IsMain() )
            SetFrameDocs();
    }
#endif // wx >= 2.9.0

private:
    void SetFrameDocs()
    {
        wxASSERT( m_tlw );
        wxASSERT( m_doc1 );
        wxASSERT( m_doc2 );

        m_tlw->SetDocs(m_doc1, m_doc2);
    }

private:
    DiffFrame *m_tlw;
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

#if !GLIB_CHECK_VERSION(2,36,0)
    g_type_init();
#endif

    static const wxCmdLineEntryDesc cmd_line_desc[] =
    {
        #if wxCHECK_VERSION(2,9,0)
            #define wxT28(s) s
        #else
            #define wxT28(s) wxT(s)
        #endif

        { wxCMD_LINE_SWITCH,
                  wxT28("h"), wxT28("help"), wxT28("show this help message"),
                  wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },

        { wxCMD_LINE_SWITCH,
                  wxT28("v"), wxT28("verbose"), wxT28("be verbose") },

        { wxCMD_LINE_OPTION,
                  NULL, wxT28("output-diff"), wxT28("output differences to given PDF file"),
                  wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_SWITCH,
                  NULL, wxT28("view"), wxT28("view the differences in a window") },

        { wxCMD_LINE_PARAM,
                  NULL, NULL, wxT28("file1.pdf"), wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_PARAM,
                  NULL, NULL, wxT28("file2.pdf"), wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_NONE }
    };

    wxCmdLineParser parser(cmd_line_desc, argc, argv);

    // Set default output to stderr
    wxMessageOutput::Set(new wxMessageOutputStderr); 

    switch ( parser.Parse() )
    {
        case -1: // --help
            return 0;

        case 0: // everything is ok; proceed
            break;

        default: // syntax error
            return 2;
    }

    if ( parser.Found(wxT("verbose")) )
        g_verbose = true;

    wxFileName file1(parser.GetParam(0));
    wxFileName file2(parser.GetParam(1));
    file1.MakeAbsolute();
    file2.MakeAbsolute();
    const wxString url1 = wxFileSystem::FileNameToURL(file1);
    const wxString url2 = wxFileSystem::FileNameToURL(file2);

    GError *err = NULL;

    PopplerDocument *doc1 = poppler_document_new_from_file(url1.utf8_str(), NULL, &err);
    if ( !doc1 )
    {
        fprintf(stderr, "Error opening %s: %s\n", (const char*) parser.GetParam(0).c_str(), err->message);
        g_error_free(err);
        return 3;
    }

    PopplerDocument *doc2 = poppler_document_new_from_file(url2.utf8_str(), NULL, &err);
    if ( !doc2 )
    {
        fprintf(stderr, "Error opening %s: %s\n", (const char*) parser.GetParam(1).c_str(), err->message);
        g_error_free(err);
        return 3;
    }

    int retval = 0;

    wxString pdf_file;
    if ( parser.Found(wxT("output-diff"), &pdf_file) )
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

    // MinGW doesn't reliably flush streams on exit, so flush them explicitly:
    fflush(stdout);
    fflush(stderr);

    return retval;
}
