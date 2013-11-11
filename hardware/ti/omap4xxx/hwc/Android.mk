LOCAL_PATH := $(call my-dir)

# HAL module implementation, not prelinked and stored in
# hw/<HWCOMPOSE_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/../vendor/lib/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libutils libhardware libhardware_legacy libz \
                          libedid libdsswbhal

ifeq ($(TARGET_BOARD_PLATFORM), $(filter $(TARGET_BOARD_PLATFORM), omap4 omap5))
LOCAL_SHARED_LIBRARIES += libion_ti
LOCAL_CFLAGS += -DUSE_LIBION_TI
else
LOCAL_SHARED_LIBRARIES += libion
LOCAL_CFLAGS += -DUSE_LIBION
LOCAL_C_INCLUDES += system/core/include/ion
endif

LOCAL_SRC_FILES := \
    blitter.c \
    color_fmt.c \
    display.c \
    dsscomp.c \
    dump.c \
    hwc.c \
    layer.c \
    rgz_2d.c \
    sw_vsync.c \
    utils.c

LOCAL_STATIC_LIBRARIES := libpng

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_CFLAGS += -DLOG_TAG=\"ti_hwc\"
LOCAL_C_INCLUDES += external/libpng external/zlib

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../edid/inc \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../libdsswb

# LOG_NDEBUG=0 means verbose logging enabled
LOCAL_CFLAGS += -DLOG_NDEBUG=0
include $(BUILD_SHARED_LIBRARY)
