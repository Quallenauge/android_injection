define _add-sgx-vendor-lib
include $$(CLEAR_VARS)
$(if $(word 2,$1),$(error Invalid SGX module name $1))
LOCAL_MODULE := $(basename $(notdir $1))
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(suffix $1)
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/$(dir $1)
LOCAL_STRIP_MODULE := false
LOCAL_CFLAGS := -Wno-error=maybe-uninitialized
OVERRIDE_BUILT_MODULE_PATH := $$(TARGET_OUT_INTERMEDIATE_LIBRARIES)
LOCAL_PATH := $$(img-sgx.untarred_bin)
include $$(BUILD_SYSTEM)/base_rules.mk
$$(LOCAL_BUILT_MODULE) : PRIVATE_SRC := $$(img-sgx.untarred_bin)/$1
$$(LOCAL_BUILT_MODULE) : $$(img-sgx.untarred_timestamp) | $$(ACP)
	@echo "Copy $$@ <- $$(PRIVATE_SRC)"
	@mkdir -p $(dir $$@)
	$$(hide) $$(ACP) -fp $$(PRIVATE_SRC) $$@
endef

define _add-sgx-vendor-bin
include $$(CLEAR_VARS)
$(if $(word 2,$1),$(error Invalid SGX module name $1))
LOCAL_MODULE := $(basename $(notdir $1))
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(suffix $1)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/$(dir $1)
LOCAL_STRIP_MODULE := false
LOCAL_CFLAGS := -Wno-error=maybe-uninitialized
OVERRIDE_BUILT_MODULE_PATH := $$(TARGET_OUT_INTERMEDIATE_EXECUTABLES)
LOCAL_PATH := $$(img-sgx.untarred_bin)
include $$(BUILD_SYSTEM)/base_rules.mk
$$(LOCAL_BUILT_MODULE) : PRIVATE_SRC := $$(img-sgx.untarred_bin)/$1
$$(LOCAL_BUILT_MODULE) : $$(img-sgx.untarred_timestamp) | $$(ACP)
	@echo "Copy $$@ <- $$(PRIVATE_SRC)"
	@mkdir -p $$(dir $$@)
	$$(hide) $$(ACP) -fp $$(PRIVATE_SRC) $$@
endef

define _add-sgx-vendor-etc
include $$(CLEAR_VARS)
$(if $(word 2,$1),$(error Invalid SGX module name $1))
LOCAL_MODULE := $(basename $(notdir $1))
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(suffix $1)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $$(TARGET_OUT)/$(dir $1)
LOCAL_STRIP_MODULE := false
LOCAL_CFLAGS := -Wno-error=maybe-uninitialized
OVERRIDE_BUILT_MODULE_PATH := $$(TARGET_OUT_INTERMEDIATE_ETC)
LOCAL_PATH := $$(img-sgx.untarred_bin)
include $$(BUILD_SYSTEM)/base_rules.mk
$$(LOCAL_BUILT_MODULE) : PRIVATE_SRC := $$(img-sgx.untarred_bin)/$1
$$(LOCAL_BUILT_MODULE) : $$(img-sgx.untarred_timestamp) | $$(ACP)
	@echo "Copy $$@ <- $$(PRIVATE_SRC)"
	@mkdir -p $$(dir $$@)
	$$(hide) $$(ACP) -fp $$(PRIVATE_SRC) $$@
endef

define _add-sgx-vendor-km
include $$(CLEAR_VARS)
$(if $(word 2,$1),$(error Invalid SGX module name $1))
LOCAL_MODULE := $(basename $(notdir $1))
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(suffix $1)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $$(TARGET_OUT)/lib/$(dir $1)
LOCAL_STRIP_MODULE := false
LOCAL_CFLAGS := -Wno-error=maybe-uninitialized
OVERRIDE_BUILT_MODULE_PATH := $$(TARGET_OUT_INTERMEDIATE_ETC)
LOCAL_PATH := $$(img-sgx.untarred_bin)
include $$(BUILD_SYSTEM)/base_rules.mk
$$(LOCAL_BUILT_MODULE) : PRIVATE_SRC := $$(img-sgx.untarred_bin)/$1
$$(LOCAL_BUILT_MODULE) : $$(img-sgx.untarred_timestamp) | $$(ACP)
	@echo "Copy $$@ <- $$(PRIVATE_SRC)"
	@mkdir -p $$(dir $$@)
	$$(hide) $$(ACP) -fp $$(PRIVATE_SRC) $$@
endef

prebuilt_sgx_vendor_libs := \
	lib/libIMGegl_SGX540_120.so \
	lib/libglslcompiler_SGX540_120.so \
	lib/libusc_SGX540_120.so \
	lib/libPVRScopeServices_SGX540_120.so \
	lib/libsrv_um_SGX540_120.so \
	lib/hw/gralloc.omap4430.so \
	lib/hw/gralloc.omap4460.so \
	lib/libpvrANDROID_WSEGL_SGX540_120.so \
	lib/libpvr2d_SGX540_120.so \
	lib/libsrv_init_SGX540_120.so \
	lib/egl/libGLESv1_CM_POWERVR_SGX540_120.so \
	lib/egl/libGLESv2_POWERVR_SGX540_120.so \
	lib/egl/libEGL_POWERVR_SGX540_120.so \
	lib/libIMGegl_SGX544_112.so \
	lib/libglslcompiler_SGX544_112.so \
	lib/libusc_SGX544_112.so \
	lib/libPVRScopeServices_SGX544_112.so \
	lib/libsrv_um_SGX544_112.so \
	lib/hw/gralloc.omap4470.so \
	lib/libpvrANDROID_WSEGL_SGX544_112.so \
	lib/libpvr2d_SGX544_112.so \
	lib/libsrv_init_SGX544_112.so \
	lib/egl/libGLESv1_CM_POWERVR_SGX544_112.so \
	lib/egl/libGLESv2_POWERVR_SGX544_112.so \
	lib/egl/libEGL_POWERVR_SGX544_112.so

prebuilt_sgx_vendor_bins := \
	bin/pvrsrvinit \
	bin/pvrsrvctl_SGX540_120 \
	bin/pvrsrvctl_SGX544_112

prebuilt_sgx_vendor_etc := \
	etc/powervr.ini

prebuilt_sgx_vendor_km := \
	modules/pvrsrvkm_sgx540_120.ko \
	modules/pvrsrvkm_sgx544_112.ko

prebuilt_sgx_modules := \
  $(foreach _file,$(prebuilt_sgx_vendor_libs) $(prebuilt_sgx_vendor_bins) $(prebuilt_sgx_vendor_etc) $(prebuilt_sgx_vendor_km),\
    $(notdir $(basename $(_file))))

include $(CLEAR_VARS)
LOCAL_MODULE := ti_omap4_sgx_libs
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(prebuilt_sgx_modules)
include $(BUILD_PHONY_PACKAGE)

$(foreach _file,$(prebuilt_sgx_vendor_libs),\
  $(eval $(call _add-sgx-vendor-lib,$(_file))))

$(foreach _file,$(prebuilt_sgx_vendor_bins),\
  $(eval $(call _add-sgx-vendor-bin,$(_file))))

$(foreach _file,$(prebuilt_sgx_vendor_etc),\
  $(eval $(call _add-sgx-vendor-etc,$(_file))))

$(foreach _file,$(prebuilt_sgx_vendor_km),\
  $(eval $(call _add-sgx-vendor-km,$(_file))))

prebuilt_sgx_modules :=
prebuilt_sgx_vendor_libs :=
prebuilt_sgx_vendor_bins :=
prebuilt_sgx_vendor_etc :=
prebuilt_sgx_vendor_km :=
_add-sgx-vendor-lib :=
_add-sgx-vendor-bin :=
_add-sgx-vendor-etc :=
_add-sgx-vendor-km :=
