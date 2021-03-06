# FOSSology Makefile - agent/nomos
# Copyright (C) 2006-2009 Hewlett-Packard Development Company, L.P.
TOP=../..
VARS=$(TOP)/Makefile.conf
DEPS=$(TOP)/Makefile.deps
include $(VARS)

SPEC=STRINGS.in
CHECK=CHECKSTR
PRE=PRECHECK
PDATA=_split_words
LICFIX=GENSEARCHDATA


OBJS=licenses.o list.o parse.o process.o nomos_regex.o util.o # sources.o DMalloc.o
GENOBJS=_precheck.o _autodata.o
HDRS=nomos.h $(OBJS:.o=.h) _autodefs.h

# FIXME: we inherited these defines, but we probably don't need them, but we
# should test before removing them. More LFS info at
# http://www.suse.de/~aj/linux_lfs.html
DEF=-D_FILE_OFFSET_BITS=64 -D__USE_LARGEFILE64

CFLAGS_LOCAL= $(DEF) $(CFLAGS_DB) $(CFLAGS_REPO) $(CFLAGS_AGENT) -lpq $(ALL_CFLAGS)
CFLAGS_LOCALO= $(DEF) $(CFLAGS_DBO) $(CFLAGS_REPOO) $(CFLAGS_AGENTO) $(ALL_CFLAGS)

all: encode nomos

debug: nomos-gl 

nomos: nomos.o $(OBJS) $(GENOBJS)
	$(CC) nomos.o $(OBJS) $(GENOBJS) $(CFLAGS_LOCAL) -o $@

nomos.o: nomos.c $(HDRS) $(DB) $(REPO) $(AGENTLIB) $(VARS)
	$(CC) -c $< $(CFLAGS_LOCALO)
	
nomos-gl: nomos-g.o $(OBJS) $(GENOBJS)
	$(CC) nomos.o $(OBJS) $(GENOBJS) $(CFLAGS_LOCAL) -o $@

nomos-g.o: nomos.c $(HDRS) $(DB) $(REPO) $(AGENTLIB) $(VARS)
	$(CC) -c  -l /usr/lib/libefence.a $< $(CFLAGS_LOCALO) 

$(OBJS) $(GENOBJS): %.o: %.c $(HDRS) $(DB) $(VARS)
	$(CC) -c $< $(DEF) $(CFLAGS_DBO) $(ALL_CFLAGS)

#
# Non "standard" preprocessing stuff starts here...
#

encode: encode.o
	$(CC) $(CFLAGS) -o $@ $@.c

_precheck.c:	_autodata.c $(PRE) $(CHECK)
#	@echo "NOTE: _autodata.c has changed --> regenerate _precheck.c"
	./$(PRE)
	./$(CHECK)
#	@$(MAKE) $(STRINGS) $(KEYS)

_autodefs.h _autodata.c:	$(SPEC) $(LICFIX)
	@echo "NOTE: GENSEARCHDATA takes 1-2 minutes to run"
	./$(LICFIX)

#
# Non "standard" preprocessing stuff ends here...
#

install: all
	$(INSTALL_PROGRAM) nomos $(DESTDIR)$(AGENTDIR)/nomos
#	$(INSTALL_PROGRAM) encode  $(DESTDIR)$(AGENTDIR)/encode

uninstall:
#	rm -f $(DESTDIR)$(AGENTDIR)/encode
	rm -f $(DESTDIR)$(AGENTDIR)/nomos

test: all
	@echo "*** No tests available for agent/$(EXE) ***"

coverage:
	@echo "NOTICE: no coverage available for nomos"

clean:
	rm -f encode nomos  *.o core \
           _autodata.c _autodefs.c _autodefs.h _precheck.c \
           _strings.data _STRFILTER strings.HISTOGRAM words.HISTOGRAM \
           split.OTHER checkstr.OK

include $(DEPS)

.PHONY: all install uninstall clean test
