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

#include "gutter.h"

#include <wx/dcclient.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>

#define EXTRA_ROOM_FOR_SCROLLBAR  20

Gutter::Gutter(wxWindow *parent, wxWindowID winid)
    : wxVListBox(parent, winid)
{
    m_fontHeight = -1;

    SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

    SetMinSize(wxSize(WIDTH + 2 * BORDER + EXTRA_ROOM_FOR_SCROLLBAR, -1));
}


void Gutter::AddPage(const wxString& label, const wxImage& thumbnail)
{
    m_labels.push_back(label);
    m_backgrounds.push_back(wxBitmap(thumbnail));
    SetItemCount(m_backgrounds.size());
    Refresh();
}

void Gutter::SetThumbnail(int page, const wxImage& thumbnail)
{
    m_backgrounds[page] = wxBitmap(thumbnail);
    Refresh();
}


void Gutter::UpdateViewPos(wxScrolledWindow *win)
{
    int sel = GetSelection();
    if ( sel == wxNOT_FOUND )
        return;

    int total_x, total_y;
    win->GetVirtualSize(&total_x, &total_y);

    float scale_x = float(m_backgrounds[sel].GetWidth()) / float(total_x);
    float scale_y = float(m_backgrounds[sel].GetHeight()) / float(total_y);

    win->GetViewStart(&m_viewPos.x, &m_viewPos.y);
    win->GetClientSize(&m_viewPos.width, &m_viewPos.height);

    m_viewPos.x = int(m_viewPos.x * scale_x);
    m_viewPos.y = int(m_viewPos.y * scale_y);
    m_viewPos.width = int(m_viewPos.width * scale_x);
    m_viewPos.height = int(m_viewPos.height * scale_y);

    Refresh();
}


wxCoord Gutter::OnMeasureItem(size_t n) const
{
    if ( m_fontHeight == -1 )
        wxConstCast(this, Gutter)->m_fontHeight = GetCharHeight();

    return m_backgrounds[n].GetHeight() + 3 * BORDER + m_fontHeight;
}


void Gutter::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
    const int xoffset = (GetClientSize().x - WIDTH) / 2;
    const int yoffset = BORDER;


    dc.DrawBitmap(m_backgrounds[n], rect.x + xoffset, rect.y + yoffset);

    const wxString label = m_labels[n];
    int tw;
    dc.GetTextExtent(label, &tw, NULL);
    dc.SetFont(GetFont());
    dc.DrawText
       (
           label,
           rect.x + xoffset + (WIDTH - tw) / 2,
           rect.y + yoffset + m_backgrounds[n].GetHeight() + BORDER
       );

    if ( GetSelection() == n )
    {
        // draw current position
        if ( m_viewPos.IsEmpty() )
            return;

        wxRect view(m_viewPos);
        view.Offset(rect.GetTopLeft());
        view.Offset(wxPoint(xoffset, yoffset));

        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxBLUE));
        dc.DrawRectangle(view);
    }
}
