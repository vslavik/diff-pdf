
#include "bmpviewer.h"

#include <wx/sizer.h>

BitmapViewer::BitmapViewer(wxWindow *parent)
    : wxScrolledWindow(parent)
{
    m_zoom_factor = 1.0;

    SetScrollRate(10, 10);

    m_content = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_content, wxSizerFlags(1).Expand());
    SetSizer(sizer);
}


void BitmapViewer::SetBestFitZoom()
{
    // compute highest scale factor that still doesn't need scrollbars:

    float scale_x = float(GetSize().x) / float(m_orig_image.GetWidth());
    float scale_y = float(GetSize().y) / float(m_orig_image.GetHeight());

    SetZoom(std::min(scale_x, scale_y));
}


void BitmapViewer::UpdateBitmap()
{
    int new_w = int(m_orig_image.GetWidth() * m_zoom_factor);
    int new_h = int(m_orig_image.GetHeight() * m_zoom_factor);

    if ( new_w != m_orig_image.GetWidth() ||
         new_h != m_orig_image.GetHeight() )
    {
        wxImage scaled(m_orig_image.Scale(new_w, new_h, wxIMAGE_QUALITY_HIGH));
        m_content->SetBitmap(wxBitmap(scaled));
    }
    else
    {
        m_content->SetBitmap(wxBitmap(m_orig_image));
    }

    GetSizer()->FitInside(this);
}


void BitmapViewer::Set(const wxImage& image)
{
    m_orig_image = image;
    UpdateBitmap();
}


void BitmapViewer::Set(cairo_surface_t *surface)
{
    // Cairo's RGB24 surfaces use 32 bits per pixel, while wxImage uses
    // 24 bits per pixel, so we need to convert between the two representations
    // manually. Moreover, channels order is different too, RGB vs. BGR.

    const int w = cairo_image_surface_get_width(surface);
    const int h = cairo_image_surface_get_height(surface);

    wxImage img(w, h, false);

    unsigned char *p_out = img.GetData();
    const unsigned char *p_in = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);

    for ( int y = 0; y < h; y++, p_in += stride )
    {
        for ( int x = 0; x < w; x++ )
        {
            *(p_out++) = *(p_in + 4 * x + 2);
            *(p_out++) = *(p_in + 4 * x + 1);
            *(p_out++) = *(p_in + 4 * x + 0);
        }
    }

    Set(img);
}
