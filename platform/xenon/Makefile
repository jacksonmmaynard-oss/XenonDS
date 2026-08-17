# SPDX-License-Identifier: GPL-2.0-only
.SUFFIXES:
.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITXENON)),)
$(error "Set DEVKITXENON to the installed LibXenon toolchain path")
endif

include $(DEVKITXENON)/rules

WITH_DESMUME ?= 0
PLATFORM_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
PROJECT_ROOT := $(abspath $(PLATFORM_DIR)/../..)
LDSCRIPT := $(PLATFORM_DIR)/app.lds

ifeq ($(WITH_DESMUME),1)
TARGET := xenonds-desmume
BUILD := build-desmume
SOURCES := desmume/source
CORE_CPPFILES := checksum.cpp controller_mapper.cpp nds_header.cpp session.cpp desmume_backend.cpp
DESMUME_ROOT := $(PROJECT_ROOT)/third_party/desmume/desmume/src
include $(PLATFORM_DIR)/desmume_sources.mk
DESMUME_CPP_OFILES := $(addprefix desmume/,$(DESMUME_CPP_SOURCES:.cpp=.o))
DESMUME_C_OFILES := $(addprefix desmume/,$(DESMUME_C_SOURCES:.c=.o))
DESMUME_OFILES := $(DESMUME_CPP_OFILES) $(DESMUME_C_OFILES)
DESMUME_LIBRARY := desmume/libdesmume.a
DESMUME_DEFINES := -DMSB_FIRST=1 -DHOST_32=1 -DTARGET_INTERFACE=1 -DDESMUME_XENON=1
DESMUME_CXXFLAGS :=
else
TARGET := xenonds-probe
BUILD := build
SOURCES := source
CORE_CPPFILES := checksum.cpp nds_header.cpp
DESMUME_DEFINES :=
DESMUME_CXXFLAGS :=
endif

DATA :=
INCLUDES := ../../include shared
SHARED_CPPFILES := rom_finder.cpp

ifeq ($(WITH_DESMUME),1)
INCLUDES += \
	../../third_party/desmume/desmume/src \
	../../third_party/desmume/desmume/src/frontend \
	../../third_party/desmume/desmume/src/libretro-common/include
endif

CFLAGS := -g -O2 -Wall $(MACHDEP) $(DESMUME_DEFINES) $(INCLUDE)
CXXFLAGS := $(CFLAGS) $(DESMUME_CXXFLAGS) -std=gnu++11
LDFLAGS := -g $(MACHDEP) -Wl,--gc-sections -Wl,-Map,$(notdir $@).map
LIBS := -lxenon -lm -lfat -lz
LIBDIRS :=

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(CURDIR)/shared $(CURDIR)/../../src $(CURDIR)/../../src/backends
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp))) $(CORE_CPPFILES) $(SHARED_CPPFILES)
export LD := $(if $(strip $(CPPFILES)),$(CXX),$(CC))
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export DESMUME_ROOT DESMUME_OFILES DESMUME_LIBRARY
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(CURDIR)/$(BUILD) -I$(LIBXENON_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBXENON_LIB)

.PHONY: all $(BUILD) clean
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).elf32 $(OUTPUT).map
else
DEPENDS := $(OFILES:.o=.d) $(DESMUME_OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).elf32

desmume/%.o: $(DESMUME_ROOT)/%.cpp
	@echo [DeSmuME $(notdir $<)]
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP -MF $(@:.o=.d) $(CXXFLAGS) -c $< -o $@.tmp
	@if [ ! -s "$@.tmp" ]; then \
		echo "[retrying empty object $(notdir $@)]"; \
		rm -f "$@.tmp"; \
		$(CXX) -MMD -MP -MF $(@:.o=.d) $(CXXFLAGS) -c $< -o $@.tmp; \
	fi
	@test -s "$@.tmp"
	@mv "$@.tmp" "$@"

desmume/%.o: $(DESMUME_ROOT)/%.c
	@echo [DeSmuME $(notdir $<)]
	@mkdir -p $(dir $@)
	@$(CC) -MMD -MP -MF $(@:.o=.d) $(CFLAGS) -c $< -o $@.tmp
	@if [ ! -s "$@.tmp" ]; then \
		echo "[retrying empty object $(notdir $@)]"; \
		rm -f "$@.tmp"; \
		$(CC) -MMD -MP -MF $(@:.o=.d) $(CFLAGS) -c $< -o $@.tmp; \
	fi
	@test -s "$@.tmp"
	@mv "$@.tmp" "$@"

$(DESMUME_LIBRARY): $(DESMUME_OFILES)

$(OUTPUT).elf32: $(OUTPUT).elf
ifeq ($(WITH_DESMUME),1)
$(OUTPUT).elf: $(OFILES) $(DESMUME_LIBRARY)
else
$(OUTPUT).elf: $(OFILES)
endif
-include $(DEPENDS)
endif
