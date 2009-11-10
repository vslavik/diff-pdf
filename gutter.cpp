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


BEGIN_EVENT_TABLE(Gutter, wxWindow)
    EVT_PAINT(Gutter::OnPaint)
END_EVENT_TABLE()

Gutter::Gutter(wxWindow *parent)
    : wxWindow(parent, wxID_ANY,
               wxDefaultPosition, wxSize(WIDTH, WIDTH),
               wxFULL_REPAINT_ON_RESIZE | wxSUNKEN_BORDER)
{
}


void Gutter::SetThumbnail(const wxImage& image)
{
    m_background = wxBitmap(image);

    wxSize sz(image.GetWidth(), image.GetHeight());
    SetSize(sz);
    SetMinSize(sz);

    Update();
}


void Gutter::UpdateViewPos(wxScrolledWindow *win)
{
    int total_x, total_y;
    win->GetVirtualSize(&total_x, &total_y);

    float scale_x = float(GetSize().x) / float(total_x);
    float scale_y = float(GetSize().y) / float(total_y);

    win->GetViewStart(&m_viewPos.x, &m_viewPos.y);
    win->GetClientSize(&m_viewPos.width, &m_viewPos.height);

    m_viewPos.x = int(m_viewPos.x * scale_x);
    m_viewPos.y = int(m_viewPos.y * scale_y);
    m_viewPos.width = int(m_viewPos.width * scale_x);
    m_viewPos.height = int(m_viewPos.height * scale_y);

    Refresh();
}


void Gutter::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);

    if ( m_background.IsOk() )
        dc.DrawBitmap(m_background, 0, 0);

    // draw current position
    if ( m_viewPos.IsEmpty() )
        return;

    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(*wxBLUE));
    dc.DrawRectangle(m_viewPos);
}
