AM_CFLAGS = -Wall -g \
    -DARIM_FILESDIR='"$(datadir)/$(PACKAGE_NAME)"' \
    -DARIM_DOCDIR='"$(datarootdir)/doc/$(PACKAGE_NAME)"' \
    -DARIM_NAME='"$(PACKAGE_NAME)"' \
    -DARIM_VERSION='"$(PACKAGE_VERSION)"'
if PORTABLE_BIN
AM_CFLAGS += -DPORTABLE_BIN
endif
if NATIVE_LITTLE_ENDIAN
AM_CFLAGS += -DNATIVE_LITTLE_ENDIAN
endif

if PORTABLE_BIN
exedir = $(prefix)
endif
if PORTABLE_BIN
exe_PROGRAMS = arim
else
bin_PROGRAMS = arim
endif
if PORTABLE_BIN
topdir = $(prefix)
top_DATA = arim.ini in.mbox out.mbox sent.mbox arim-themes
filesdir = $(prefix)/files
files_DATA = files/test.txt
docsdir = $(prefix)/doc
docs_DATA = help/arim-help.pdf help/arim(1).pdf help/arim(5).pdf NEWS AUTHORS COPYING README
else
filesdir = $(datadir)/$(PACKAGE_NAME)
files_DATA = files/test.txt arim.ini in.mbox arim-themes
docsdir = $(datarootdir)/doc/$(PACKAGE_NAME)
docs_DATA = help/arim-help.pdf help/arim(1).pdf help/arim(5).pdf NEWS AUTHORS COPYING README
dist_man_MANS = arim.1 arim.5
endif

EXTRA_DIST = files/test.txt help/arim-help.pdf help/arim(1).pdf help/arim(5).pdf ChangeLog in.mbox out.mbox sent.mbox arim.ini arim-themes
if PORTABLE_BIN
EXTRA_DIST += $(PACKAGE_NAME)
endif

arim_SOURCES = \
    src/main.c src/main.h \
    src/arim.c src/arim.h \
    src/arim_arq.c src/arim_arq.h \
    src/arim_arq_msg.c src/arim_arq_msg.h \
    src/arim_arq_auth.c src/arim_arq_auth.h \
    src/arim_arq_files.c src/arim_arq_files.h \
    src/arim_beacon.c src/arim_beacon.h \
    src/arim_message.c src/arim_message.h \
    src/arim_ping.c  src/arim_ping.h \
    src/arim_proto.c src/arim_proto.h \
    src/arim_proto_idle.c src/arim_proto_idle.h \
    src/arim_proto_ping.c src/arim_proto_ping.h \
    src/arim_proto_msg.c src/arim_proto_msg.h \
    src/arim_proto_query.c src/arim_proto_query.h \
    src/arim_proto_beacon.c src/arim_proto_beacon.h \
    src/arim_proto_unproto.c src/arim_proto_unproto.h \
    src/arim_proto_frame.c src/arim_proto_frame.h \
    src/arim_proto_arq_conn.c src/arim_proto_arq_conn.h \
    src/arim_proto_arq_files.c src/arim_proto_arq_files.h \
    src/arim_proto_arq_msg.c src/arim_proto_arq_msg.h \
    src/arim_proto_arq_auth.c src/arim_proto_arq_auth.h \
    src/arim_query.c src/arim_query.h \
    src/bufq.c src/bufq.h \
    src/cmdproc.c src/cmdproc.h \
    src/ardop_cmds.c src/ardop_cmds.h \
    src/ardop_data.c src/ardop_data.h \
    src/tnc_attach.c src/tnc_attach.h \
    src/cmdthread.c src/cmdthread.h \
    src/datathread.c src/datathread.h \
    src/serialthread.c src/serialthread.h \
    src/ini.c src/ini.h \
    src/log.c src/log.h \
    src/mbox.c src/mbox.h \
    src/ui.c src/ui.h \
    src/ui_dialog.c src/ui_dialog.h \
    src/ui_fec_menu.c src/ui_fec_menu.h \
    src/ui_files.c src/ui_files.h \
    src/ui_recents.c src/ui_recents.h \
    src/ui_ping_hist.c src/ui_ping_hist.h \
    src/ui_conn_hist.c src/ui_conn_hist.h \
    src/ui_file_hist.c src/ui_file_hist.h \
    src/ui_heard_list.c src/ui_heard_list.h \
    src/ui_tnc_data_win.c src/ui_tnc_data_win.h \
    src/ui_tnc_cmd_win.c src/ui_tnc_cmd_win.h \
    src/ui_cmd_prompt_win.c src/ui_cmd_prompt_win.h \
    src/ui_help_menu.c src/ui_help_menu.h \
    src/ui_msg.c src/ui_msg.h \
    src/ui_themes.c src/ui_themes.h \
    src/util.c src/util.h \
    src/auth.c src/auth.h \
    src/blake2s-ref.c src/blake2.h src/blake2-impl.h

if PORTABLE_BIN
uninstall-hook:
	if test -d $(topdir); then rm -rf $(topdir); fi
dist-hook:
	rm -rf $(distdir)/src
	rm -f $(distdir)/compile
	rm -f $(distdir)/depcomp
	rm -f $(distdir)/install-sh
	rm -f $(distdir)/missing
	rm -f $(distdir)/config.*
	rm -f $(distdir)/configure*
	rm -f $(distdir)/aclocal.m4
	rm -f $(distdir)/Makefile*
	rm -f $(distdir)/INSTALL
	rm -f $(distdir)/ChangeLog
	mkdir $(distdir)/doc
	mv $(distdir)/help/arim-help.pdf $(distdir)/doc/arim-help-v$(PACKAGE_VERSION).pdf
	mv $(distdir)/help/arim\(1\).pdf $(distdir)/doc/arim\(1\)-v$(PACKAGE_VERSION).pdf
	mv $(distdir)/help/arim\(5\).pdf $(distdir)/doc/arim\(5\)-v$(PACKAGE_VERSION).pdf
	mv $(distdir)/AUTHORS $(distdir)/doc/AUTHORS
	mv $(distdir)/README $(distdir)/doc/README
	mv $(distdir)/COPYING $(distdir)/doc/COPYING
	mv $(distdir)/NEWS $(distdir)/doc/NEWS
	rm -rf $(distdir)/help
	tar czhof $(distdir)-bin-$(target_os)-$(target_cpu).tar.gz $(distdir) 
	echo "rm -f $(distdir).tar.gz" > rm-unwanted.sh
	echo "tar tzvf $(distdir)-bin-$(target_os)-$(target_cpu).tar.gz" > list-archive.sh
distcheck:
	@:
else
uninstall-hook:
	if test -d $(docsdir); then rm -rf $(docsdir); fi
endif

