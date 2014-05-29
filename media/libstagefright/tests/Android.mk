# Build the unit tests.
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(TARGET_SIMULATOR),true)

LOCAL_MODULE := SurfaceMediaSource_test

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    SurfaceMediaSource_test.cpp \
	DummyRecorder.cpp \

LOCAL_SHARED_LIBRARIES := \
	libEGL \
	libGLESv2 \
	libbinder \
	libcutils \
	libgui \
	libmedia \
	libstagefright \
	libstagefright_foundation \
	libstagefright_omx \
	libsync \
	libui \
	libutils \
	liblog

LOCAL_STATIC_LIBRARIES := \
	libgtest_libc++ \
	libgtest_main_libc++ \

LOCAL_C_INCLUDES := \
    external/gtest/include \
	frameworks/av/media/libstagefright \
	frameworks/av/media/libstagefright/include \
	$(TOP)/frameworks/native/include/media/openmax \

LOCAL_32_BIT_ONLY := true

include external/libcxx/libcxx.mk
include $(BUILD_EXECUTABLE)

endif

# Include subdirectory makefiles
# ============================================================

# If we're building with ONE_SHOT_MAKEFILE (mm, mmm), then what the framework
# team really wants is to build the stuff defined by this makefile.
ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
