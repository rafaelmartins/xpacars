## Autotools settings

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh \
	LICENSE \
	README.md \
	sdk/license.txt \
	xpacars.txt \
	$(NULL)

lib_LTLIBRARIES = \
	xpacars.la \
	$(NULL)

xpacars_la_SOURCES = \
	main.c \
	$(NULL)

xpacars_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBCURL_CFLAGS) \
	-I$(top_srcdir)/SDK/CHeaders \
	-fvisibility=hidden \
	-DLIN=1 \
	-DXPLM210=1 \
	$(NULL)

xpacars_la_LIBADD = \
	$(LIBCURL_LIBS) \
	$(NULL)

xpacars_la_LDFLAGS = \
	-avoid-version \
	-module \
	-shared \
	$(NULL)

xpacars.xpl: xpacars.la
	cp .libs/xpacars.so xpacars.xpl

plugin: xpacars.xpl
	-rm -f $(PACKAGE)-plugin-$(VERSION).zip
	zip $(PACKAGE)-plugin-$(VERSION).zip xpacars.xpl

.PHONY: plugin
