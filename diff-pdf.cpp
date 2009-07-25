
#include <stdio.h>
#include <assert.h>

#include <glib.h>
#include <poppler.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>


// Resolution to use for rasterization, in DPI
#define RESOLUTION  300


cairo_surface_t *render_page(PopplerPage *page)
{
    double w, h;
    poppler_page_get_size(page, &w, &h);

    const int w_px = RESOLUTION * w / 72.0;
    const int h_px = RESOLUTION * h / 72.0;

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


cairo_surface_t *diff_images(cairo_surface_t *s1, cairo_surface_t *s2)
{
    const int width = cairo_image_surface_get_width(s1);
    const int height = cairo_image_surface_get_height(s1);

    // FIXME: handle pages of different sizes
    assert( width == cairo_image_surface_get_width(s2) );
    assert( height == cairo_image_surface_get_height(s2) );

    bool changes = false;

    cairo_surface_t *diff =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

    const int stride1 = cairo_image_surface_get_stride(s1);
    const int stride2 = cairo_image_surface_get_stride(s2);
    const int stridediff = cairo_image_surface_get_stride(diff);

    const unsigned char *data1 = cairo_image_surface_get_data(s1);
    const unsigned char *data2 = cairo_image_surface_get_data(s2);
    unsigned char *datadiff = cairo_image_surface_get_data(diff);

    for ( int y = 0;
          y < height;
          y++, data1 += stride1, data2 += stride2, datadiff += stridediff )
    {
        for ( int x = 0; x < width * 4; x += 4 )
        {
            unsigned char r1 = *(data1 + x + 0);
            unsigned char g1 = *(data1 + x + 1);
            unsigned char b1 = *(data1 + x + 2);

            unsigned char r2 = *(data2 + x + 0);
            unsigned char g2 = *(data2 + x + 1);
            unsigned char b2 = *(data2 + x + 2);

            if ( r1 != r2 || g1 != g2 || b1 != b2 )
                changes = true;


            // we visualize the differences by taking one channel from s1
            // and the other two channels from s2
            *(datadiff + x + 0) = r1;
            *(datadiff + x + 1) = g1;
            *(datadiff + x + 2) = b2;
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


bool page_compare(cairo_t *cr_out,
                  PopplerPage *page1, PopplerPage *page2)
{
    // FIXME: handle missing pages correctly
    assert( page1 );
    assert( page2 );

    cairo_surface_t *img1 = render_page(page1);
    cairo_surface_t *img2 = render_page(page2);

    cairo_surface_t *diff = diff_images(img1, img2);

    if ( diff )
    {
        // render the difference as high-resolution bitmap

        cairo_save(cr_out);
        cairo_scale(cr_out, 72.0 / RESOLUTION, 72.0 / RESOLUTION);

        cairo_set_source_surface(cr_out, diff ? diff : img1, 0, 0);
        cairo_paint(cr_out);

        cairo_restore(cr_out);

        cairo_surface_destroy(diff);
    }
    else
    {
        // save space (as well as improve rendering quality) in diff pdf
        // by writing unchanged pages in their original form rather than
        // a rasterized one

        poppler_page_render(page1, cr_out);
    }

    cairo_show_page(cr_out);

    cairo_surface_destroy(img1);
    cairo_surface_destroy(img2);

    return diff != NULL;
}


bool doc_compare(PopplerDocument *doc1, PopplerDocument *doc2)
{
    bool are_same = true;

    double w, h;
    poppler_page_get_size(poppler_document_get_page(doc1, 0), &w, &h);

    cairo_surface_t *surface_out =
          // FIXME: specify output file
          cairo_pdf_surface_create("foo.pdf", w, h);
    cairo_t *cr_out = cairo_create(surface_out);

    int pages1 = poppler_document_get_n_pages(doc1);
    int pages2 = poppler_document_get_n_pages(doc2);
    int pages_total = pages1 > pages2 ? pages1 : pages2;

    if ( pages1 != pages2 )
        are_same = false;

    for ( int page = 0; page < pages_total; page++ )
    {
        PopplerPage *page1 = page < pages1
                             ? poppler_document_get_page(doc1, page)
                             : NULL;
        PopplerPage *page2 = page < pages2
                             ? poppler_document_get_page(doc2, page)
                             : NULL;

        if ( !page_compare(cr_out, page1, page2) )
            are_same = false;
    }

    cairo_destroy(cr_out);
    cairo_surface_destroy(surface_out);

    return are_same;
}


int main(int argc, char *argv[])
{
    if ( argc != 3 )
    {
        fprintf(stderr, "Usage: %s file1.pdf file2.pdf\n", argv[0]);
        return 2;
    }

    g_type_init();

    GError *err;

    PopplerDocument *doc1 = poppler_document_new_from_file(argv[1], NULL, &err);
    if ( !doc1 )
    {
        fprintf(stderr, "Error opening %s: %s\n", argv[1], err->message);
        g_object_unref(err);
        return 3;
    }

    PopplerDocument *doc2 = poppler_document_new_from_file(argv[2], NULL, &err);
    if ( !doc2 )
    {
        fprintf(stderr, "Error opening %s: %s\n", argv[2], err->message);
        g_object_unref(err);
        return 3;
    }

    bool are_same = doc_compare(doc1, doc2);

    g_object_unref(doc1);
    g_object_unref(doc2);

    return are_same ? 0 : 1;
}
