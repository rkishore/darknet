#
# Main Makefile
#
include config.mak

ifeq ($(MAX_CLASSIFY_CHILDREN),)
    DEFINES += \
        -DMAX_CLASSIFY_CHILDREN=1
else
    DEFINES += \
        -DMAX_CLASSIFY_CHILDREN=$(MAX_CLASSIFY_CHILDREN)
endif

# List of flags to pass to the compilation command
# CFLAGS=$(DEFINES) -I$(IDIR) -ggdb -Wall -O2 -D_REENTRANT -D_GNU_SOURCE -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE `pkg-config --cflags pocketsphinx sphinxbase libavfilter libavformat libavcodec libavutil libswscale libswresample libpostproc` #-DSELF_PULL 
CFLAGS=$(DEFINES) -I$(IDIR) -ggdb -Wall -O2 -D_REENTRANT -D_GNU_SOURCE -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE 

#LIBS=-lpthread -L/usr/local/lib -Wl,-Bsymbolic-functions -Wl,-z,relro `pkg-config --libs pocketsphinx sphinxbase openssl zlib libavfilter libavformat libavcodec libavutil libswscale libswresample libpostproc` 
LIBS=-lpthread -L/usr/local/lib -lcurl -Wl,-Bsymbolic-functions -Wl,-z,relro -lcrypto -lm

# Set of .h files the .c files depend on
_DEPS = config.h serialnum.h init.h puller.h fastpool.h threadqueue.h dfxp.h allocator.h pool.h cjson.h restful.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#_OBJS = fastpool.o threadqueue.o config.o serialnum.o init.o dfxp.o allocator.o pool.o comm.o udpcomm.o cjson.o restful.o classifyapp.o
#_OBJS = fastpool.o threadqueue.o config.o serialnum.o init.o allocator.o pool.o comm.o udpcomm.o cjson.o puller.o restful.o classifyapp.o
_OBJS = common.o fastpool.o threadqueue.o config.o serialnum.o init.o cjson.o puller.o restful.o classifyapp.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SRC)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/classifyapp: $(OBJS)
	$(CC) -rdynamic -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o core $(SRC)/*~ $(IDIR)/*~ $(ODIR)/classifyapp

install:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
	$(INSTALL) -m 755 $(ODIR)/classifyapp "$(DESTDIR)$(prefix)/bin"

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/classifyapp
