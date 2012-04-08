LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LV_C_INCLUDES)
LOCAL_CFLAGS     += $(LV_CFLAGS)
LOCAL_CXXFLAGS   += $(LV_CXXFLAGS)
LOCAL_CPPFLAGS   += $(LV_CPPFLAGS)

LOCAL_MODULE           := actor_lv_analyzer
LOCAL_SRC_FILES        := \
	$(addprefix /, $(notdir $(wildcard $(LOCAL_PATH)/*.c) $(wildcard $(LOCAL_PATH)/*.cpp)))
LOCAL_LDLIBS           +=
LOCAL_SHARED_LIBRARIES += visual

include $(BUILD_SHARED_LIBRARY)