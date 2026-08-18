# SPDX-License-Identifier: GPL-2.0-only
.SUFFIXES:
.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITXENON)),)
$(error "Set DEVKITXENON to the installed LibXenon toolchain path")
endif

include $(DEVKITXENON)/rules
LOADER_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
LDSCRIPT := $(abspath $(LOADER_DIR)/../app.lds)

TARGET := xenonds-loader
BUILD := build
SOURCES := source
DATA :=
INCLUDES := source

CFLAGS := -g -O2 -Wall $(MACHDEP) $(INCLUDE)
ASFLAGS := $(MACHDEP) $(INCLUDE)
LDFLAGS := -g $(MACHDEP) -Wl,--gc-sections -Wl,-Map,$(TARGET).elf.map
LIBS := -lxenon -lm -lfat
LIBDIRS :=

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

export LD := $(CC)
export OFILES := $(CFILES:.c=.o) $(SFILES:.S=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	-I$(CURDIR)/$(BUILD) -I$(LIBXENON_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBXENON_LIB)

.PHONY: all $(BUILD) clean
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).elf32 $(OUTPUT).map
else
DEPENDS := $(OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).elf32

$(OUTPUT).elf32: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)
endif
