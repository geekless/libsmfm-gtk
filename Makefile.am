include $(top_srcdir)/Makefile.decl

NULL=

# a little trick against gtk-doc is here
SUBDIRS_DOCS = docs

EXTRA_DIST_DOCS = \
	gtk-doc.make \
	docs/Makefile.am \
	docs/Makefile.in \
	docs/reference/Makefile.am \
	docs/reference/Makefile.in \
	docs/reference/libfm/Makefile.am \
	docs/reference/libfm/Makefile.in \
	docs/reference/libfm/libfm-docs.xml \
	docs/reference/libfm/libfm-sections.txt \
	docs/reference/libfm/version.xml.in

DIST_SUBDIRS = src po data
SUBDIRS = $(DIST_SUBDIRS) $(SUBDIRS_DOCS)

EXTRA_DIST += \
	libsmfm-gtk2.pc.in \
	libsmfm-gtk3.pc.in \
	po/libfm.pot \
	$(NULL)

if ENABLE_GTK_DOC
DIST_SUBDIRS += $(SUBDIRS_DOCS)
else
EXTRA_DIST += $(EXTRA_DIST_DOCS)
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	libsmfm-gtk@GTK_VERSION_MAJOR@.pc \
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc

cppcheck cppcheck-inconclusive:
	make -C src $@
