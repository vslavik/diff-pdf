
#ifndef _bmpviewer_h_
#define _bmpviewer_h_

#include <cairo/cairo.h>

#include <wx/image.h>
#include <wx/bitmap.h>
#include <wx/statbmp.h>
#include <wx/scrolwin.h>
#include <wx/event.h>

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

private:
    // update the content after some change (bitmap, zoom factor, ...)
    void UpdateBitmap();

    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);

private:
    wxStaticBitmap *m_content;
    wxImage m_orig_image;
    float m_zoom_factor;

    // is the user currently dragging the page around with the mouse?
    bool m_draggingPage;
    wxPoint m_draggingLastMousePos;

    DECLARE_EVENT_TABLE()
};

#endif // _bmpviewer_h_
