#
# Main Makefile
#
include config.mak

GPU=0
CUDNN=0
OPENCV=1
OPENMP=1
DEBUG=0

ARCH= -gencode arch=compute_30,code=sm_30 \
      -gencode arch=compute_35,code=sm_35 \
      -gencode arch=compute_50,code=[sm_50,compute_50] \
      -gencode arch=compute_52,code=[sm_52,compute_52]
#      -gencode arch=compute_20,code=[sm_20,sm_21] \ This one is deprecated?

# This is what I use, uncomment if you know your arch and want to specify
# ARCH= -gencode arch=compute_52,code=compute_52

ifeq ($(MAX_CLASSIFY_CHILDREN),)
    DEFINES += \
        -DMAX_CLASSIFY_CHILDREN=1
else
    DEFINES += \
        -DMAX_CLASSIFY_CHILDREN=$(MAX_CLASSIFY_CHILDREN)
endif

OPTS=-Ofast

ifeq ($(DEBUG), 1) 
OPTS=-O0 -g
endif

# List of flags to pass to the compilation command
#CFLAGS=$(DEFINES) -I$(IDIR) -ggdb -Wall -O2 -D_REENTRANT -D_GNU_SOURCE -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -fPIC -fno-strict-aliasing -DBASE_THREADSAFE 
CFLAGS=-I$(IDIR) -ggdb -Wall -O2 -D_REENTRANT -D_GNU_SOURCE -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -fPIC -DBASE_THREADSAFE

ifeq ($(OPENMP), 1) 
CFLAGS+= -fopenmp
endif

#LIBS=-lpthread -L/usr/local/lib -Wl,-Bsymbolic-functions -Wl,-z,relro `pkg-config --libs pocketsphinx sphinxbase openssl zlib libavfilter libavformat libavcodec libavutil libswscale libswresample libpostproc` 
LIBS=-lpthread -L/usr/local/lib -lcurl -Wl,-Bsymbolic-functions -Wl,-z,relro -lcrypto -lm

ifeq ($(OPENCV), 1) 
COMMON+= -DOPENCV
CFLAGS+= -DOPENCV
LIBS+= `pkg-config --libs opencv` -lstdc++
COMMON+= `pkg-config --cflags opencv` 
endif

CFLAGS+=$(OPTS)

# Set of .h files the .c files depend on
_DEPS = config.h serialnum.h init.h puller.h fastpool.h threadqueue.h dfxp.h allocator.h pool.h cjson.h restful.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#_OBJS = fastpool.o threadqueue.o config.o serialnum.o init.o dfxp.o allocator.o pool.o comm.o udpcomm.o cjson.o restful.o classifyapp.o
#_OBJS = fastpool.o threadqueue.o config.o serialnum.o init.o allocator.o pool.o comm.o udpcomm.o cjson.o puller.o restful.o classifyapp.o
_OBJS=gemm.o utils.o cuda.o deconvolutional_layer.o convolutional_layer.o list.o image.o activations.o im2col.o col2im.o blas.o crop_layer.o dropout_layer.o maxpool_layer.o softmax_layer.o data.o matrix.o network.o connected_layer.o cost_layer.o parser.o option_list.o detection_layer.o route_layer.o upsample_layer.o box.o normalization_layer.o avgpool_layer.o layer.o local_layer.o shortcut_layer.o logistic_layer.o activation_layer.o rnn_layer.o gru_layer.o crnn_layer.o demo.o batchnorm_layer.o region_layer.o reorg_layer.o tree.o  lstm_layer.o l2norm_layer.o yolo_layer.o iseg_layer.o image_opencv.o detector.o common.o fastpool.o threadqueue.o config.o serialnum.o init.o cjson.o puller.o restful.o classifyapp.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SRC)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: $(SRC)/%.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

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
