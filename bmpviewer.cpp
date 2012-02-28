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

#include <wx/sizer.h>

BEGIN_EVENT_TABLE(BitmapViewer, wxScrolledWindow)
    EVT_LEFT_DOWN(BitmapViewer::OnMouseDown)
    EVT_LEFT_UP(BitmapViewer::OnMouseUp)
    EVT_MOTION(BitmapViewer::OnMouseMove)
    EVT_MOUSE_CAPTURE_LOST(BitmapViewer::OnMouseCaptureLost)
    EVT_SCROLLWIN(BitmapViewer::OnScrolling)
    EVT_SIZE(BitmapViewer::OnSizeChanged)
END_EVENT_TABLE()

BitmapViewer::BitmapViewer(wxWindow *parent)
    : wxScrolledWindow(parent,
                       wxID_ANY,
                       wxDefaultPosition, wxDefaultSize,
                       wxFULL_REPAINT_ON_RESIZE)
{
    m_gutter = NULL;
    m_zoom_factor = 1.0;

    SetScrollRate(1, 1);

    wxBitmap dummyBitmap(16, 16);
    m_content = new wxStaticBitmap(this, wxID_ANY, dummyBitmap);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_content, wxSizerFlags(1).Expand());
    SetSizer(sizer);

    // we need to bind mouse-down event to m_content, as this scrolled window
    // will never see mouse events otherwise
    m_content->Connect
               (
                   wxEVT_LEFT_DOWN,
                   wxMouseEventHandler(BitmapViewer::OnMouseDown),
                   NULL,
                   this
               );
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
        wxImage scaled =
            m_orig_image.Scale
                         (
                             new_w,
                             new_h,
                             // we don't need HQ filtering when upscaling
                             m_zoom_factor < 1.0
                                ? wxIMAGE_QUALITY_HIGH
                                : wxIMAGE_QUALITY_NORMAL
                         );
        m_content->SetBitmap(wxBitmap(scaled));
    }
    else
    {
        m_content->SetBitmap(wxBitmap(m_orig_image));
    }

    GetSizer()->FitInside(this);

    if ( m_gutter )
        m_gutter->UpdateViewPos(this);
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


void BitmapViewer::AttachGutter(Gutter *g)
{
    m_gutter = g;
    if ( g )
        g->UpdateViewPos(this);
}


void BitmapViewer::OnMouseDown(wxMouseEvent& event)
{
    wxPoint view_origin;
    GetViewStart(&view_origin.x, &view_origin.y);

    const wxPoint pos = event.GetPosition();

    m_draggingPage = true;
    m_draggingLastMousePos = pos;
    CaptureMouse();
}


void BitmapViewer::OnMouseUp(wxMouseEvent&)
{
    m_draggingPage = false;
    ReleaseMouse();
}


void BitmapViewer::OnMouseMove(wxMouseEvent& event)
{
    event.Skip();

    if ( !m_draggingPage )
        return;

    wxPoint view_origin;
    GetViewStart(&view_origin.x, &view_origin.y);

    const wxPoint pos = event.GetPosition();
    wxPoint new_pos = view_origin + (m_draggingLastMousePos - pos);

    Scroll(new_pos.x, new_pos.y);
    if ( m_gutter )
        m_gutter->UpdateViewPos(this);

    m_draggingLastMousePos = pos;
}


void BitmapViewer::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    m_draggingPage = false;
    event.Skip();
}


void BitmapViewer::OnScrolling(wxScrollWinEvent& event)
{
    if ( m_gutter )
        m_gutter->UpdateViewPos(this);

    event.Skip();
}

void BitmapViewer::OnSizeChanged(wxSizeEvent& event)
{
    if ( m_gutter )
        m_gutter->UpdateViewPos(this);

    event.Skip();
}
