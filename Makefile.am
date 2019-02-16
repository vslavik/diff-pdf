
bin_PROGRAMS = diff-pdf

diff_pdf_SOURCES = \
			diff-pdf.cpp \
			bmpviewer.cpp \
			bmpviewer.h \
			gutter.cpp \
			gutter.h

diff_pdf_CXXFLAGS = $(POPPLER_CFLAGS) $(WX_CXXFLAGS)
diff_pdf_LDADD = $(POPPLER_LIBS) $(WX_LIBS)

EXTRA_DIST = bootstrap gtk-zoom-in.xpm gtk-zoom-out.xpm README.md win32/fonts.conf win32/collect-dlls.sh

windows-dist: diff-pdf-win-$(VERSION).zip

diff-pdf-win-$(VERSION).zip: all
	rm -rf $@ windist
	$(srcdir)/win32/collect-dlls.sh windist diff-pdf.exe
	mkdir -p windist/fonts && cp -a $(srcdir)/win32/fonts.conf windist/fonts/
	(cd windist && zip -9r ../$@ .)
	rm -rf windist

.PHONY: windows-dist
