LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

GNONLIN_TOP := $(LOCAL_PATH)

GNL_BUILT_SOURCES := \
	gnl/Android.mk
GNL_BUILT_SOURCES := $(patsubst %, $(abspath $(GNONLIN_TOP))/%, $(GNL_BUILT_SOURCES))

.PHONY: gnonlin-configure gnonlin-configure-real
gnonlin-configure-real:
	cd $(GNONLIN_TOP) ; \
	CC="$(CONFIGURE_CC)" \
	CFLAGS="$(CONFIGURE_CFLAGS)" \
	LD=$(TARGET_LD) \
	LDFLAGS="$(CONFIGURE_LDFLAGS)" \
	CPP=$(CONFIGURE_CPP) \
	CPPFLAGS="$(CONFIGURE_CPPFLAGS)" \
	PKG_CONFIG_LIBDIR=$(CONFIGURE_PKG_CONFIG_LIBDIR) \
	PKG_CONFIG_TOP_BUILD_DIR=/ \
	$(abspath $(GNONLIN_TOP))/$(CONFIGURE) --host=arm-linux-androideabi \
	--disable-gtk-doc --disable-valgrind --prefix=/system && \
	for file in $(GNL_BUILT_SOURCES); do \
		rm -f $$file && \
		make -C $$(dirname $$file) $$(basename $$file) ; \
	done

gnonlin-configure: gnonlin-configure-real

CONFIGURE_TARGETS += gnonlin-configure

-include $(GNONLIN_TOP)/gnl/Android.mk
