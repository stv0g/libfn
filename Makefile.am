pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fn.pc

install-data-hook:
	mkdir $(pkgdatadir)
	cp -r web $(pkgdatadir)

uninstall-hook:
	rm -r $(pkgdatadir)

SUBDIRS = src
