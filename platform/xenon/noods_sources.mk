# NooDS core source set. Paths are relative to third_party/noods/src/core.

NOODS_CPP_SOURCES := \
	arm/cp15.cpp \
	arm/interpreter.cpp \
	arm/interpreter_alu.cpp \
	arm/interpreter_branch.cpp \
	arm/interpreter_lookup.cpp \
	arm/interpreter_transfer.cpp \
	arm/timers.cpp \
	core.cpp \
	gpu/gpu.cpp \
	gpu/gpu_2d.cpp \
	gpu/gpu_3d.cpp \
	gpu/gpu_3d_renderer.cpp \
	hle/action_replay.cpp \
	hle/dldi.cpp \
	hle/hle_arm7.cpp \
	hle/hle_bios.cpp \
	io/aes.cpp \
	io/div_sqrt.cpp \
	io/i2c.cpp \
	io/input.cpp \
	io/ipc.cpp \
	io/rtc.cpp \
	io/spi.cpp \
	io/spu.cpp \
	io/wifi.cpp \
	memory/cartridge.cpp \
	memory/dma.cpp \
	memory/memory.cpp \
	memory/ndma.cpp \
	memory/sd_mmc.cpp \
	save_states.cpp \
	settings.cpp
