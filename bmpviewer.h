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

#ifndef _bmpviewer_h_
#define _bmpviewer_h_

#include <cairo/cairo.h>

#include <wx/image.h>
#include <wx/bitmap.h>
#include <wx/statbmp.h>
#include <wx/scrolwin.h>
#include <wx/event.h>

class Gutter;

// widget for comfortable viewing of a bitmap, with high-quality zooming
class BitmapViewer : public wxScrolledWindow
{
public:
    BitmapViewer(wxWindow *parent);

    // set the bitmap to be shown
    void Set(const wxImage& image);
    void Set(cairo_surface_t *surface);

    float GetZoom() const
    {
        return m_zoom_factor;
    }

    void SetZoom(float factor)
    {
        m_zoom_factor = factor;
        UpdateBitmap();
    }

    // sets the zoom value to "best fit" for current window size
    void SetBestFitZoom();

    // attaches a gutter that shows current scrolling position to the window
    void AttachGutter(Gutter *g);

private:
    // update the content after some change (bitmap, zoom factor, ...)
    void UpdateBitmap();

    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnScrolling(wxScrollWinEvent& event);
    void OnSizeChanged(wxSizeEvent& event);

private:
    wxStaticBitmap *m_content;
    wxImage m_orig_image;
    float m_zoom_factor;

    // is the user currently dragging the page around with the mouse?
    bool m_draggingPage;
    wxPoint m_draggingLastMousePos;

    Gutter *m_gutter;

    DECLARE_EVENT_TABLE()
};

#endif // _bmpviewer_h_
