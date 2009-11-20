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

#include <vector>

#include <wx/image.h>
#include <wx/bitmap.h>
#include <wx/vlbox.h>

class WXDLLIMPEXP_FWD_CORE wxScrolledWindow;

// widget showing places of differences as well as scroll window's position
class Gutter : public wxVListBox
{
public:
    // standard width of the gutter image in pixels
    static const int WIDTH = 100;

    // standard border
    static const int BORDER = 5;

    Gutter(wxWindow *parent, wxWindowID winid);

    // Add a new page to the gutter, with thumbnail's background to be shown
    void AddPage(const wxString& label, const wxImage& thumbnail);

    // Set the bitmap with thumbnail's background to be shown
    void SetThumbnail(int page, const wxImage& thumbnail);

    // Updates shown view position, i.e. the visible subset of scrolled window.
    // The gutter will indicate this area with a blue rectangle.
    void UpdateViewPos(wxScrolledWindow *win);

protected:
    virtual wxCoord OnMeasureItem(size_t n) const;
    virtual void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const;

private:
    std::vector<wxString> m_labels;
    std::vector<wxBitmap> m_backgrounds;
    wxRect m_viewPos;
    int m_fontHeight;
};

#endif // _gutter_h_
