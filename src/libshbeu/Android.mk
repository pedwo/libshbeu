LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	external/libshbeu/include \

#LOCAL_CFLAGS := -DDEBUG

LOCAL_SRC_FILES := \
	beu.c

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_MODULE := libshbeu
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
