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

#ifndef _gutter_h_
#define _gutter_h_

#include <wx/image.h>
#include <wx/window.h>
#include <wx/bitmap.h>

class wxScrolledWindow;

// widget showing places of differences as well as scroll window's position
class Gutter : public wxWindow
{
public:
    // standard width of the gutter in pixels
    static const int WIDTH = 100;

    Gutter(wxWindow *parent);

    // set the bitmap with differences to be shown
    void SetDiffMap(const wxImage& image);

    // Updates shown view position, i.e. the visible subset of scrolled window.
    // The gutter will indicate this area with a blue rectangle.
    void UpdateViewPos(wxScrolledWindow *win);

private:
    void OnPaint(wxPaintEvent& event);

private:
    wxBitmap m_background;
    wxRect m_viewPos;

    DECLARE_EVENT_TABLE()
};

#endif // _gutter_h_
