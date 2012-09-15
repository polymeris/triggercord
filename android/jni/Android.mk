LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := pentax
LOCAL_SRC_FILES := 	\
	pentax_wrap.cpp \
	../../pentax.cpp 	\
	../../pslr.c 		\
	../../pslr_enum.c \
	../../pslr_lens.c \
	../../pslr_model.c \
	../../pslr_scsi.c 
LOCAL_CPPFLAGS  := -DANDROID -frtti -I.. -Istlport
LOCAL_LDLIBS	:= -llog -lstdc++

include $(BUILD_SHARED_LIBRARY)
