FLOAT=hard
TOOLCHAIN	:=/usr/
LD			:= $(TOOLCHAIN)/bin/ld
CC			:= $(TOOLCHAIN)/bin/gcc-4.7
CXX       	:= $(TOOLCHAIN)/bin/g++-4.7
OBJDUMP		:= $(TOOLCHAIN)/bin/objdump
RANLIB		:= $(TOOLCHAIN)/bin/ranlib
STRIP		:= $(TOOLCHAIN)/bin/strip
AR			:= $(TOOLCHAIN)/bin/ar
CXXCP 		:= $(CXX) -E

CFLAGS +=  -mfloat-abi=hard \
			-mcpu=arm1176jzf-s \
			-fomit-frame-pointer \
			-mabi=aapcs-linux \
			-mtune=arm1176jzf-s \
			-mfpu=vfp \
			-Wno-psabi \
			-mno-apcs-stack-check \
			-O3 \
			-mstructure-size-boundary=32 \
			-mno-sched-prolog \
			-march=armv6zk \
			-I/usr/include/dbus-1.0 \
			-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include 

LDFLAGS		+= -L/opt/vc/lib -L/lib -L/usr/lib -lfreetype
INCLUDES	+= -I/opt/vc/include/interface/vcos/pthreads \
				-I/opt/vc/include \
				-I/opt/vc/include/interface/vmcs_host \
				-I/opt/vc/include/interface/vmcs_host/linux \
				-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include \
				-I/usr/include \
				-I/usr/include/freetype2


CFLAGS    +=-std=c++0x \
            -D__STDC_CONSTANT_MACROS \
            -D__STDC_LIMIT_MACROS \
            -DTARGET_POSIX \
            -DTARGET_LINUX \
            -fPIC \
            -DPIC \
            -D_REENTRANT \
            -D_LARGEFILE64_SOURCE \
            -D_FILE_OFFSET_BITS=64 \
            -DHAVE_CMAKE_CONFIG \
            -D__VIDEOCORE4__ \
            -U_FORTIFY_SOURCE \
            -Wall \
            -DHAVE_OMXLIB \
            -DUSE_EXTERNAL_FFMPEG \
            -DHAVE_LIBAVCODEC_AVCODEC_H \
            -DHAVE_LIBAVUTIL_OPT_H \
            -DHAVE_LIBAVUTIL_MEM_H \
            -DHAVE_LIBAVUTIL_AVUTIL_H \
            -DHAVE_LIBAVFORMAT_AVFORMAT_H \
            -DHAVE_LIBAVFILTER_AVFILTER_H \
            -DHAVE_LIBSWRESAMPLE_SWRESAMPLE_H \
            -DOMX \
            -DOMX_SKIP64BIT \
            -ftree-vectorize \
            -DUSE_EXTERNAL_OMX \
            -DTARGET_RASPBERRY_PI \
            -DUSE_EXTERNAL_LIBBCM_HOST

LDFLAGS+=-L./ -lc -lWFC -lGLESv2 -lEGL -lbcm_host -lopenmaxil -lfreetype -lz -lasound

INCLUDES+=-I./ \
			-Ilinux \
			-I/usr/include/dbus-1.0 \
			-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include

SRC=	linux/XMemUtils.cpp \
		linux/OMXAlsa.cpp \
		utils/log.cpp \
		utils/PCMRemap.cpp \
		utils/RegExp.cpp \
		BitstreamConverter.cpp \
		OMXThread.cpp \
		OMXReader.cpp \
		OMXStreamInfo.cpp \
		OMXCore.cpp \
		OMXVideo.cpp \
		OMXClock.cpp \
		File.cpp \
		OMXTranscoderVideo.cpp \
		OMXMuxer.cpp \
		Srt.cpp \
		omxtranscoder.cpp

OBJS+=$(filter %.o,$(SRC:.cpp=.o))

all: omxtranscoder

%.o: %.cpp
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@ -Wno-deprecated-declarations

omxtranscoder: $(OBJS)
	$(CXX) $(LDFLAGS) -o omxtranscoder $(OBJS) -lvchiq_arm -lvchostif -lvcos -ldbus-1 -lrt -lpthread -lavutil -lavcodec -lavformat -lswscale -lswresample -lpcre
	$(STRIP) omxtranscoder

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f omxplayer.old.log omxplayer.log
	@rm -f omxtranscoder
