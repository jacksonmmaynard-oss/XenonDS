# SPDX-License-Identifier: GPL-2.0-or-later
.SUFFIXES:
.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITXENON)),)
$(error "Set DEVKITXENON to the installed LibXenon toolchain path")
endif

include $(DEVKITXENON)/rules

WITH_DESMUME ?= 0
WITH_NOODS ?= 0
PLATFORM_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
PROJECT_ROOT := $(abspath $(PLATFORM_DIR)/../..)
LDSCRIPT := $(PLATFORM_DIR)/app.lds

ifeq ($(WITH_NOODS),1)
override AR := xenon-gcc-ar
export AR
TARGET := xenonds-noods
BUILD := build-noods
SOURCES := noods/source
CORE_CPPFILES := checksum.cpp controller_mapper.cpp nds_header.cpp
NOODS_ROOT := $(PROJECT_ROOT)/third_party/noods/src/core
include $(PLATFORM_DIR)/noods_sources.mk
NOODS_OFILES := $(addprefix noods/,$(NOODS_CPP_SOURCES:.cpp=.o))
NOODS_LIBRARY := noods/libnoods.a
NOODS_DEFINES := -DENDIAN_BIG=1 -DNO_FDOPEN=1 -DLOG_LEVEL=0 -DNOODS_XENON=1
NOODS_CXXFLAGS := -Ofast -flto -fno-fat-lto-objects -fno-rtti -fno-threadsafe-statics
NOODS_LDFLAGS := -flto
else ifeq ($(WITH_DESMUME),1)
TARGET := xenonds-desmume
BUILD := build-desmume-fasttiming-o2
SOURCES := desmume/source
CORE_CPPFILES := checksum.cpp controller_mapper.cpp nds_header.cpp session.cpp desmume_backend.cpp
DESMUME_ROOT := $(PROJECT_ROOT)/third_party/desmume/desmume/src
include $(PLATFORM_DIR)/desmume_sources.mk
DESMUME_CPP_OFILES := $(addprefix desmume/,$(DESMUME_CPP_SOURCES:.cpp=.o))
DESMUME_C_OFILES := $(addprefix desmume/,$(DESMUME_C_SOURCES:.c=.o))
DESMUME_OFILES := $(DESMUME_CPP_OFILES) $(DESMUME_C_OFILES)
DESMUME_LIBRARY := desmume/libdesmume.a
DESMUME_DEFINES := -DMSB_FIRST=1 -DHOST_32=1 -DDESMUME_XENON=1
DESMUME_CXXFLAGS :=
NOODS_DEFINES :=
NOODS_CXXFLAGS :=
NOODS_LDFLAGS :=
else
TARGET := xenonds-probe
BUILD := build
SOURCES := source
CORE_CPPFILES := checksum.cpp nds_header.cpp
DESMUME_DEFINES :=
DESMUME_CXXFLAGS :=
NOODS_DEFINES :=
NOODS_CXXFLAGS :=
NOODS_LDFLAGS :=
endif

DATA :=
INCLUDES := ../../include shared
SHARED_CPPFILES := rom_finder.cpp

ifeq ($(WITH_NOODS),1)
INCLUDES := \
	noods/compat \
	../../third_party/noods/src/core \
	../../third_party/noods/src/core/arm \
	../../third_party/noods/src/core/gpu \
	../../third_party/noods/src/core/hle \
	../../third_party/noods/src/core/io \
	../../third_party/noods/src/core/memory \
	../../include \
	shared
else ifeq ($(WITH_DESMUME),1)
INCLUDES += \
	../../third_party/desmume/desmume/src \
	../../third_party/desmume/desmume/src/frontend \
	../../third_party/desmume/desmume/src/libretro-common/include
endif

CFLAGS := -g -O2 -Wall $(MACHDEP) $(DESMUME_DEFINES) $(NOODS_DEFINES) $(INCLUDE)
CXXFLAGS := $(CFLAGS) $(DESMUME_CXXFLAGS) $(NOODS_CXXFLAGS) -std=gnu++11
LDFLAGS := -g $(MACHDEP) $(NOODS_LDFLAGS) -Wl,--gc-sections -Wl,-Map,$(TARGET).elf.map
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
export DESMUME_ROOT DESMUME_OFILES DESMUME_LIBRARY NOODS_ROOT NOODS_OFILES NOODS_LIBRARY
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
DEPENDS := $(OFILES:.o=.d) $(DESMUME_OFILES:.o=.d) $(NOODS_OFILES:.o=.d)

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

noods/%.o: $(NOODS_ROOT)/%.cpp
	@echo [NooDS $(notdir $<)]
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP -MF $(@:.o=.d) $(CXXFLAGS) -c $< -o $@.tmp
	@if [ ! -s "$@.tmp" ]; then \
		echo "[retrying empty object $(notdir $@)]"; \
		rm -f "$@.tmp"; \
		$(CXX) -MMD -MP -MF $(@:.o=.d) $(CXXFLAGS) -c $< -o $@.tmp; \
	fi
	@test -s "$@.tmp"
	@mv "$@.tmp" "$@"

# Xenon GCC 9.2 ICEs in gpu_2d.cpp's brightness math at -Ofast.
# Keep the rest of NooDS at -Ofast and compile this one translation unit at -O2.
noods/gpu/gpu_2d.o: CXXFLAGS := $(filter-out -Ofast,$(CXXFLAGS))

$(NOODS_LIBRARY): $(NOODS_OFILES)

$(OUTPUT).elf32: $(OUTPUT).elf
ifeq ($(WITH_NOODS),1)
$(OUTPUT).elf: $(OFILES) $(NOODS_LIBRARY)
else ifeq ($(WITH_DESMUME),1)
$(OUTPUT).elf: $(OFILES) $(DESMUME_LIBRARY)
else
$(OUTPUT).elf: $(OFILES)
endif
-include $(DEPENDS)
endif
