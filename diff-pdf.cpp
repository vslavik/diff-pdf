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
bool g_skip_identical = false;
bool g_mark_differences = false;
long g_channel_tolerance = 0;
long g_per_page_pixel_tolerance = 0;
bool g_grayscale = false;
// Resolution to use for rasterization, in DPI
#define DEFAULT_RESOLUTION 300
long g_resolution = DEFAULT_RESOLUTION;

inline unsigned char to_grayscale(unsigned char r, unsigned char g, unsigned char b)
{
    return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

cairo_surface_t *render_page(PopplerPage *page)
{
    double w, h;
    poppler_page_get_size(page, &w, &h);

    const int w_px = int((int)g_resolution * w / 72.0);
    const int h_px = int((int)g_resolution * h / 72.0);

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
    cairo_scale(cr, (int)g_resolution / 72.0, (int)g_resolution / 72.0);

    poppler_page_render(page, cr);

    cairo_show_page(cr);

    cairo_destroy(cr);

    return surface;
}


// Creates image of differences between s1 and s2. If the offset is specified,
// then s2 is displaced by it. If thumbnail and thumbnail_width are specified,
// then a thumbnail with highlighted differences is created too.
cairo_surface_t *diff_images(int page, cairo_surface_t *s1, cairo_surface_t *s2,
                             int offset_x = 0, int offset_y = 0,
                             wxImage *thumbnail = NULL, int thumbnail_width = -1)
{
    assert( s1 || s2 );

    long pixel_diff_count = 0;
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
        // initalize the thumbnail with a white rectangle:
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
            bool linediff = false;

            for ( int x = 0; x < r2.width * 4; x += 4 )
            {
                unsigned char cr1 = *(out + x + 0);
                unsigned char cg1 = *(out + x + 1);
                unsigned char cb1 = *(out + x + 2);

                unsigned char cr2 = *(data2 + x + 0);
                unsigned char cg2 = *(data2 + x + 1);
                unsigned char cb2 = *(data2 + x + 2);

                if ( cr1 > (cr2+g_channel_tolerance) || cr1 < (cr2-g_channel_tolerance)
                  || cg1 > (cg2+g_channel_tolerance) || cg1 < (cg2-g_channel_tolerance)
                  || cb1 > (cb2+g_channel_tolerance) || cb1 < (cb2-g_channel_tolerance)
                   )
                {
                    pixel_diff_count++;
                    changes = true;
                    linediff = true;

                    if ( thumbnail )
                    {
                        // calculate the coordinates in the thumbnail
                        int tx = int((r2.x + x/4.0) * thumbnail_scale);
                        int ty = int((r2.y + y) * thumbnail_scale);

                        // Limit the coordinates to the thumbnail size (may be
                        // off slightly due to rounding errors).
                        // See https://github.com/vslavik/diff-pdf/pull/58
                        tx = std::min(tx, thumbnail_width - 1);
                        ty = std::min(ty, thumbnail_height - 1);

                        // mark changes with red
                        thumbnail->SetRGB(tx, ty, 255, 0, 0);
                    }
                }

                if (g_grayscale)
                {
                    // convert both images to grayscale, use blue for s1, red for s2
                    unsigned char gray1 = to_grayscale(cr1, cg1, cb1);
                    unsigned char gray2 = to_grayscale(cr2, cg2, cb2);
                    *(out + x + 0) = gray2;
                    *(out + x + 1) = (gray1 + gray2) / 2;
                    *(out + x + 2) = gray1;
                }
                else
                {
                    // change the B channel to be from s2; RG will be s1
                    *(out + x + 2) = cb2;
                }
            }

            if (g_mark_differences && linediff)
            {
                for (int x = 0; x < (10 < r2.width ? 10 : r2.width) * 4; x+=4)
                {
                   *(out + x + 0) = 0;
                   *(out + x + 1) = 0;
                   *(out + x + 2) = 255;
                }
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

    if ( g_verbose )
        printf("page %d has %ld pixels that differ\n", page, pixel_diff_count);

    // If we specified a tolerance, then return if we have exceeded that for this page
    if ( g_per_page_pixel_tolerance == 0 ? changes : pixel_diff_count > g_per_page_pixel_tolerance)
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
bool page_compare(int page, cairo_t *cr_out,
                  PopplerPage *page1, PopplerPage *page2,
                  wxImage *thumbnail = NULL, int thumbnail_width = -1)
{
    cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
    cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;

    cairo_surface_t *diff = diff_images(page, img1, img2, 0, 0,
                                        thumbnail, thumbnail_width);
    const bool has_diff = (diff != NULL);

    if ( cr_out )
    {
        if ( diff )
        {
            // render the difference as high-resolution bitmap

            cairo_save(cr_out);
            cairo_scale(cr_out, 72.0 / g_resolution, 72.0 / g_resolution);

            cairo_set_source_surface(cr_out, diff ? diff : img1, 0, 0);
            cairo_paint(cr_out);

            cairo_restore(cr_out);
        }
        else
        {
            // save space (as well as improve rendering quality) in diff pdf
            // by writing unchanged pages in their original form rather than
            // a rasterized one

            if (!g_skip_identical)
               poppler_page_render(page1, cr_out);
        }

        if (diff || !g_skip_identical)
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
    int pages_differ = 0;

    cairo_surface_t *surface_out = NULL;
    cairo_t *cr_out = NULL;

    if ( pdf_output )
    {
        double w, h;
        poppler_page_get_size(poppler_document_get_page(doc1, 0), &w, &h);
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
                              "Comparing page %d of %d...",
                              page+1,
                              pages_total
                          )
                       );
        }

        if ( pdf_output && page != 0 )
        {
            double w, h;
            poppler_page_get_size(poppler_document_get_page(doc1, page), &w, &h);
            cairo_pdf_surface_set_size(surface_out, w, h);
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
            page_same = page_compare(page, cr_out, page1, page2,
                                     &thumbnail, Gutter::WIDTH);

            wxString label1("(null)");
            wxString label2("(null)");

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
                label = label1 + " / " + label2;

            gutter->AddPage(label, thumbnail);
        }
        else
        {
            page_same = page_compare(page, cr_out, page1, page2);
        }

        if ( differences )
            differences->push_back(!page_same);

        if ( !page_same )
        {
	    pages_differ ++;

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

    if (g_verbose)
        printf("%d of %d pages differ.\n", pages_differ, pages_total);

    // are doc1 and doc1 the same?
    return (pages_differ == 0) && (pages1 == pages2);
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
    #define BMP_ZOOM_IN    BMP_ARTPROV("gtk-zoom-in")
    #define BMP_ZOOM_OUT   BMP_ARTPROV("gtk-zoom-out")
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

        toolbar->AddTool(ID_PREV_PAGE, "Previous", BMP_PREV_PAGE,
                         "Go to previous page (PgUp)");
        toolbar->AddTool(ID_NEXT_PAGE, "Next", BMP_NEXT_PAGE,
                         "Go to next page (PgDown)");
        toolbar->AddTool(ID_ZOOM_IN, "Zoom in", BMP_ZOOM_IN,
                         "Make the page larger (Ctrl +)");
        toolbar->AddTool(ID_ZOOM_OUT, "Zoom out", BMP_ZOOM_OUT,
                         "Make the page smaller (Ctrl -)");
        toolbar->AddTool(ID_OFFSET_LEFT, "", BMP_OFFSET_LEFT,
                         "Offset one of the pages to the left (Ctrl left)");
        toolbar->AddTool(ID_OFFSET_RIGHT, "", BMP_OFFSET_RIGHT,
                         "Offset one of the pages to the right (Ctrl right)");
        toolbar->AddTool(ID_OFFSET_UP, "", BMP_OFFSET_UP,
                         "Offset one of the pages up (Ctrl up)");
        toolbar->AddTool(ID_OFFSET_DOWN, "", BMP_OFFSET_DOWN,
                         "Offset one of the pages down (Ctrl down)");

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

        wxProgressDialog progress("Comparing documents",
                                  "Comparing documents...",
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
                                    m_cur_page,
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
                "Page %d of %d; %d of them %s different, this page %s",
                m_cur_page + 1 /* humans prefer 1-based counting*/,
                (int)m_pages.size(),
                m_diff_count,
                m_diff_count == 1 ? "is" : "are",
                m_pages[m_cur_page] ? "differs" : "is unchanged"
            ),
            0
        );

        SetStatusText
        (
            wxString::Format
            (
                "%.1f%% [offset %d,%d]",
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

        return true;
    }

    void SetData(const wxString& file1, PopplerDocument *doc1,
                 const wxString& file2, PopplerDocument *doc2)
    {
        m_title = wxString::Format("Differences between %s and %s", file1.c_str(), file2.c_str());
        m_doc1 = doc1;
        m_doc2 = doc2;
    }

protected:
    virtual void OnEventLoopEnter(wxEventLoopBase *loop)
    {
        wxApp::OnEventLoopEnter(loop);

        if ( loop->IsMain() )
            SetFrameDocs();
    }

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

    static const wxCmdLineEntryDesc cmd_line_desc[] =
    {
        { wxCMD_LINE_SWITCH,
                  "h", "help", "show this help message",
                  wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },

        { wxCMD_LINE_SWITCH,
                  "v", "verbose", "be verbose" },

        { wxCMD_LINE_SWITCH,
                  "s", "skip-identical", "only output pages with differences" },

        { wxCMD_LINE_SWITCH,
                  "m", "mark-differences", "additionally mark differences on left side" },

        { wxCMD_LINE_SWITCH,
                  "g", "grayscale", "only differences will be in color, unchanged parts will show as gray" },

        { wxCMD_LINE_OPTION,
                  NULL, "output-diff", "output differences to given PDF file",
                  wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_OPTION,
                  NULL, "channel-tolerance", "consider channel values to be equal if within specified tolerance",
                  wxCMD_LINE_VAL_NUMBER },

        { wxCMD_LINE_OPTION,
                  NULL, "per-page-pixel-tolerance", "total number of pixels allowed to be different per page before specifying the page is different",
                  wxCMD_LINE_VAL_NUMBER },

        { wxCMD_LINE_OPTION,
                  NULL, "dpi", "rasterization resolution (default: " wxSTRINGIZE(DEFAULT_RESOLUTION) " dpi)",
                  wxCMD_LINE_VAL_NUMBER },

        { wxCMD_LINE_SWITCH,
                  NULL, "view", "view the differences in a window" },

        { wxCMD_LINE_PARAM,
                  NULL, NULL, "file1.pdf", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_PARAM,
                  NULL, NULL, "file2.pdf", wxCMD_LINE_VAL_STRING },

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

    if ( parser.Found("verbose") )
        g_verbose = true;

    if ( parser.Found("skip-identical") )
        g_skip_identical = true;

    if ( parser.Found("mark-differences") )
        g_mark_differences = true;

    if ( parser.Found("grayscale") )
        g_grayscale = true;

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

    if ( parser.Found("per-page-pixel-tolerance", &g_per_page_pixel_tolerance) )
    {
        if (g_per_page_pixel_tolerance < 0) {
            fprintf(stderr, "Invalid per-page-pixel-tolerance: %ld. Must be 0 or more\n", g_per_page_pixel_tolerance);
            return 2;
        }
    }

    if ( parser.Found("channel-tolerance", &g_channel_tolerance) )
    {
        if (g_channel_tolerance < 0 || g_channel_tolerance > 255) {
            fprintf(stderr, "Invalid channel-tolerance: %ld. Valid range is 0(default, exact matching)-255\n", g_channel_tolerance);
            return 2;
	}
    }

	if ( parser.Found("dpi", &g_resolution) )
    {
        if (g_resolution < 1 || g_resolution > 2400) {
            fprintf(stderr, "Invalid dpi: %ld. Valid range is 1-2400 (default: %d)\n", g_resolution, DEFAULT_RESOLUTION);
            return 2;
	}
    }


    int retval = 0;

    wxString pdf_file;
    if ( parser.Found("output-diff", &pdf_file) )
    {
        retval = doc_compare(doc1, doc2, pdf_file.utf8_str(), NULL) ? 0 : 1;
    }
    else if ( parser.Found("view") )
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
