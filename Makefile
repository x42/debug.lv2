#!/usr/bin/make -f

CXXFLAGS ?= -Wall
LIB_EXT=.so

LV2NAME=dbg
BUNDLE=dbg.lv2
BUILDDIR=build/

LOADLIBES=-lm
UI_LIBS=-lX11

# check for build-dependencies
ifeq ($(shell pkg-config --exists lv2 || echo no), no)
  $(error "LV2 SDK was not found")
endif

override CXXFLAGS += `pkg-config --cflags lv2` -fPIC

all: $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl \
     $(BUILDDIR)$(LV2NAME)$(LIB_EXT) \
     $(BUILDDIR)$(LV2NAME)_gui$(LIB_EXT) \

$(BUILDDIR)manifest.ttl: lv2ttl/manifest.ttl.in
	@mkdir -p $(BUILDDIR)
	sed "s/@LV2NAME@/$(LV2NAME)/;s/@LIB_EXT@/$(LIB_EXT)/" \
	  lv2ttl/manifest.ttl.in > $(BUILDDIR)manifest.ttl

$(BUILDDIR)$(LV2NAME).ttl: lv2ttl/$(LV2NAME).ttl.in
	@mkdir -p $(BUILDDIR)
	sed "s/@LV2NAME@/$(LV2NAME)/;s/@VERSION@/lv2:microVersion 0 ;lv2:minorVersion 0 ;/g" \
		lv2ttl/$(LV2NAME).ttl.in > $(BUILDDIR)$(LV2NAME).ttl

$(BUILDDIR)$(LV2NAME)$(LIB_EXT): src/$(LV2NAME).cc
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
	  -o $(BUILDDIR)$(LV2NAME)$(LIB_EXT) src/$(LV2NAME).cc \
	  -shared -Wl,-Bstatic -Wl,-Bdynamic $(LDFLAGS) $(LOADLIBES)

$(BUILDDIR)$(LV2NAME)_gui$(LIB_EXT): gui/$(LV2NAME)_gui.cc
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
	  -o $(BUILDDIR)$(LV2NAME)_gui$(LIB_EXT) gui/$(LV2NAME)_gui.cc \
	  -shared -Wl,-Bstatic -Wl,-Bdynamic $(LDFLAGS) $(UI_LIBS)

clean:
	rm -f $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl
	rm -f $(BUILDDIR)$(LV2NAME)$(LIB_EXT)
	rm -f $(BUILDDIR)$(LV2NAME)_gui$(LIB_EXT)
	-test -d $(BUILDDIR) && rmdir $(BUILDDIR) || true

distclean: clean
	rm -f cscope.out cscope.files tags

.PHONY: clean all distclean
