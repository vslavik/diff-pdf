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

#include <assert.h>
#include <stdio.h>

#include <filesystem>
#include <string>
#include <vector>

#include <cairo-pdf.h>
#include <cairo.h>
#include <glib.h>
#include <glib/poppler.h>

// ------------------------------------------------------------------------
// PDF rendering functions
// ------------------------------------------------------------------------

bool g_verbose = false;
bool g_skip_identical = false;
bool g_mark_differences = false;
long g_channel_tolerance = 0;

// Resolution to use for rasterization, in DPI
#define RESOLUTION 300

cairo_surface_t *render_page(PopplerPage *page) {
  double w, h;
  poppler_page_get_size(page, &w, &h);

  const int w_px = int(RESOLUTION * w / 72.0);
  const int h_px = int(RESOLUTION * h / 72.0);

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

// Creates image of differences between s1 and s2. If the offset is specified,
// then s2 is displaced by it. If thumbnail and thumbnail_width are specified,
// then a thumbnail with highlighted differences is created too.
cairo_surface_t *diff_images(cairo_surface_t *s1, cairo_surface_t *s2,
                             int offset_x = 0, int offset_y = 0) {
  assert(s1 || s2);

  struct Rect {
    int x, y, width, height;
  };
  Rect r1 = {0, 0, s1 ? cairo_image_surface_get_width(s1) : 0,
             s1 ? cairo_image_surface_get_height(s1) : 0};
  Rect r2 = {0, 0, s2 ? cairo_image_surface_get_width(s2) : 0,
             s2 ? cairo_image_surface_get_height(s2) : 0};

  // compute union rectangle starting at [0,0] position
  Rect rdiff = {std::min(r1.x, r2.x), std::min(r1.y, r2.y),
                std::max(r1.x + r1.width, r2.x + r2.width),
                std::max(r1.y + r1.height, r2.y + r2.height)};
  r1.x -= rdiff.x;
  r1.y -= rdiff.y;
  r2.x -= rdiff.x;
  r2.y -= rdiff.y;
  rdiff.x -= rdiff.x;
  rdiff.y -= rdiff.y;

  bool changes = false;

  cairo_surface_t *diff =
      cairo_image_surface_create(CAIRO_FORMAT_RGB24, rdiff.width, rdiff.height);

  float thumbnail_scale;
  int thumbnail_height;

  // clear the surface to white background if the merged images don't fully
  // overlap:
  if (r1.x != r2.x || r1.y != r2.y || r1.width != r2.width ||
      r1.height != r2.height) {
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
  if (s1) {
    unsigned char *out = datadiff + r1.y * stridediff + r1.x * 4;
    for (int y = 0; y < r1.height; y++, data1 += stride1, out += stridediff) {
      memcpy(out, data1, r1.width * 4);
    }
  }

  // then, copy B channel from s2 over it; also compare the two versions
  // to see if there are any differences:
  if (s2) {
    unsigned char *out = datadiff + r2.y * stridediff + r2.x * 4;
    for (int y = 0; y < r2.height; y++, data2 += stride2, out += stridediff) {
      bool linediff = false;

      for (int x = 0; x < r2.width * 4; x += 4) {
        unsigned char cr1 = *(out + x + 0);
        unsigned char cg1 = *(out + x + 1);
        unsigned char cb1 = *(out + x + 2);

        unsigned char cr2 = *(data2 + x + 0);
        unsigned char cg2 = *(data2 + x + 1);
        unsigned char cb2 = *(data2 + x + 2);

        if (cr1 > (cr2 + g_channel_tolerance) ||
            cr1 < (cr2 - g_channel_tolerance) ||
            cg1 > (cg2 + g_channel_tolerance) ||
            cg1 < (cg2 - g_channel_tolerance) ||
            cb1 > (cb2 + g_channel_tolerance) ||
            cb1 < (cb2 - g_channel_tolerance)) {
          changes = true;
          linediff = true;
        }

        // change the B channel to be from s2; RG will be s1
        *(out + x + 2) = cb2;
      }

      if (g_mark_differences && linediff) {
        for (int x = 0; x < (10 < r2.width ? 10 : r2.width) * 4; x += 4) {
          *(out + x + 0) = 0;
          *(out + x + 1) = 0;
          *(out + x + 2) = 255;
        }
      }
    }
  }

  if (changes) {
    return diff;
  } else {
    cairo_surface_destroy(diff);
    return NULL;
  }
}

// Compares given two pages. If cr_out is not NULL, then the diff image (either
// differences or unmodified page, if there are no diffs) is drawn to it.
// If thumbnail and thumbnail_width are specified, then a thumbnail with
// highlighted differences is created too.
bool page_compare(cairo_t *cr_out, PopplerPage *page1, PopplerPage *page2) {
  cairo_surface_t *img1 = page1 ? render_page(page1) : NULL;
  cairo_surface_t *img2 = page2 ? render_page(page2) : NULL;

  cairo_surface_t *diff = diff_images(img1, img2, 0, 0);
  const bool has_diff = (diff != NULL);

  if (cr_out) {
    if (diff) {
      // render the difference as high-resolution bitmap

      cairo_save(cr_out);
      cairo_scale(cr_out, 72.0 / RESOLUTION, 72.0 / RESOLUTION);

      cairo_set_source_surface(cr_out, diff ? diff : img1, 0, 0);
      cairo_paint(cr_out);

      cairo_restore(cr_out);
    } else {
      // save space (as well as improve rendering quality) in diff pdf
      // by writing unchanged pages in their original form rather than
      // a rasterized one

      if (!g_skip_identical)
        poppler_page_render(page1, cr_out);
    }

    if (diff || !g_skip_identical)
      cairo_show_page(cr_out);
  }

  if (diff)
    cairo_surface_destroy(diff);

  if (img1)
    cairo_surface_destroy(img1);
  if (img2)
    cairo_surface_destroy(img2);

  return !has_diff;
}

// Compares two documents, writing diff PDF into file named 'pdf_output' if
// not NULL. if 'differences' is not NULL, puts a map of which pages differ
// into it. If 'progress' is provided, it is updated to reflect comparison's
// progress. If 'gutter' is set, then all the pages are added to it, with
// their respective thumbnails (the gutter must be empty beforehand).
bool doc_compare(PopplerDocument *doc1, PopplerDocument *doc2,
                 const char *pdf_output, std::vector<bool> *differences) {
  int pages_differ = 0;

  cairo_surface_t *surface_out = NULL;
  cairo_t *cr_out = NULL;

  if (pdf_output) {
    double w, h;
    poppler_page_get_size(poppler_document_get_page(doc1, 0), &w, &h);
    surface_out = cairo_pdf_surface_create(pdf_output, w, h);
    cr_out = cairo_create(surface_out);
  }

  int pages1 = poppler_document_get_n_pages(doc1);
  int pages2 = poppler_document_get_n_pages(doc2);
  int pages_total = pages1 > pages2 ? pages1 : pages2;

  if (pages1 != pages2) {
    if (g_verbose)
      printf("pages count differs: %d vs %d\n", pages1, pages2);
  }

  for (int page = 0; page < pages_total; page++) {
    if (pdf_output && page != 0) {
      double w, h;
      poppler_page_get_size(poppler_document_get_page(pages1 > pages2 ? doc1 : doc2, page), &w, &h);
      cairo_pdf_surface_set_size(surface_out, w, h);
    }

    PopplerPage *page1 =
        page < pages1 ? poppler_document_get_page(doc1, page) : NULL;
    PopplerPage *page2 =
        page < pages2 ? poppler_document_get_page(doc2, page) : NULL;

    bool page_same = page_compare(cr_out, page1, page2);

    if (differences)
      differences->push_back(!page_same);

    if (!page_same) {
      pages_differ++;

      if (g_verbose)
        printf("page %d differs\n", page + 1);

      // If we don't need to output all different pages in any
      // form (including verbose report of differing pages!), then
      // we can stop comparing the PDFs as soon as we find the first
      // difference.
      if (!g_verbose && !pdf_output && !differences)
        break;
    }
  }

  if (pdf_output) {
    cairo_destroy(cr_out);
    cairo_surface_destroy(surface_out);
  }

  if (g_verbose)
    printf("%d of %d pages differ.\n", pages_differ, pages_total);

  // are doc1 and doc1 the same?
  return (pages_differ == 0) && (pages1 == pages2);
}

int usage() {
  fprintf(
      stdout,
      R"HELP(Usage: diff-pdf [-h] [-v] [-s] [-m] [--output-diff <str>] [--channel-tolerance <num>] file1.pdf file2.pdf
  -h, --help                show this help message
  -v, --verbose             be verbose
  -s, --skip-identical      only output pages with differences
  -m, --mark-differences    additionally mark differences on left side
  --output-diff=<str>       output differences to given PDF file
  --channel-tolerance=<num> consider channel values to be equal if within specified tolerance
)HELP");
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    usage();
    return 2;
  }
  std::string pdf_file;
  std::string file1 = std::filesystem::absolute(argv[argc - 2]);
  std::string file2 = std::filesystem::absolute(argv[argc - 1]);
  for (int i = 1; i < argc - 2; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      return usage();
    } else if (arg == "-v" || arg == "--verbose") {
      g_verbose = true;
    } else if (arg == "-s" || arg == "--skip-identical") {
      g_skip_identical = true;
    } else if (arg == "-m" || arg == "--mark-differences") {
      g_mark_differences = true;
    } else if (arg.size() > 14 && arg.find("--output-diff=") == 0) {
      pdf_file = std::filesystem::absolute(arg.substr(14));
    } else if (arg.size() > 20 && arg.find("--channel-tolerance=") == 0) {
      g_channel_tolerance = atol(arg.substr(20).c_str());
      if (g_channel_tolerance < 0 || g_channel_tolerance > 255) {
        fprintf(stderr,
                "Invalid channel-tolerance: %ld. Valid range is 0(default, "
                "exact matching)-255\n",
                g_channel_tolerance);
        return 2;
      }
    }
  }

  GError *err = NULL;
  gchar *uri1 = g_filename_to_uri(file1.c_str(), NULL, &err);
  PopplerDocument *doc1 = poppler_document_new_from_file(uri1, NULL, &err);
  if (!doc1) {
    fprintf(stderr, "Error opening %s: %s\n", (const char *)file1.c_str(),
            err->message);
    g_error_free(err);
    return 3;
  }
  g_free(uri1);

  gchar *uri2 = g_filename_to_uri(file2.c_str(), NULL, &err);
  PopplerDocument *doc2 = poppler_document_new_from_file(uri2, NULL, &err);
  if (!doc2) {
    fprintf(stderr, "Error opening %s: %s\n", (const char *)file2.c_str(),
            err->message);
    g_error_free(err);
    return 3;
  }
  g_free(uri2);

  int retval = doc_compare(doc1, doc2, pdf_file.empty() ? NULL : pdf_file.c_str(), NULL) ? 0 : 1;

  g_object_unref(doc1);
  g_object_unref(doc2);

  // MinGW doesn't reliably flush streams on exit, so flush them explicitly:
  fflush(stdout);
  fflush(stderr);

  return retval;
}
