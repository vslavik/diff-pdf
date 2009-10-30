
bin_PROGRAMS = diff-pdf

diff_pdf_SOURCES = \
			diff-pdf.cpp \
			bmpviewer.cpp \
			bmpviewer.h \
			gutter.cpp \
			gutter.h

diff_pdf_CXXFLAGS = $(POPPLER_CFLAGS) $(WX_CXXFLAGS)
diff_pdf_LDADD = $(POPPLER_LIBS) $(WX_LIBS)

EXTRA_DIST = gtk-zoom-in.xpm gtk-zoom-out.xpm
