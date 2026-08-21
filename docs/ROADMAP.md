# XenonDS roadmap

## Milestone 0: portable core foundation

- [x] Safe Nintendo DS header parser and CRC verification
- [x] ARM7/ARM9 ROM range validation
- [x] Backend-neutral lifecycle, input, framebuffer and audio contracts
- [x] Host tests and ROM information utility
- [x] Initial DeSmuME adapter source
- [x] LibXenon hardware probe source
- [x] Dual-screen layout engine
- [x] Xbox-position button map and right-stick touch cursor
- [x] Containerized probe build workflow
- [x] Portable BGR555 video compositor

## Milestone 1: first hardware frame

- [x] Boot it through XeLL on a real Xbox 360
- [x] Record video, USB, controller, ATA, and FAT initialization results
- [x] Discover and validate a Nintendo DS ROM header from FAT storage
- [x] Statically compile the required DeSmuME interpreter sources
- [x] Stage the large interpreter behind an on-screen progress loader
- [x] Display the first software-rendered DS frame

## Milestone 2: playable interpreter

- [x] Replace the DeSmuME performance checkpoint with a pinned NooDS core
- [x] Direct-boot Nintendo DS software without proprietary BIOS files
- [x] Dual-screen Xbox framebuffer output and controller/touch input
- [x] Cartridge `.sav` loading and periodic/exit-time flushing
- [x] Pace completed DS frames rather than internal scheduler slices
- [x] Reproducible core smoke tests and GitHub release artifacts
- [ ] LibXenon audio ring buffer
- [ ] ROM browser and clean error screen
- [x] Configurable picture correction and hybrid 2D/3D frame skip
- [ ] Configurable screen layouts
- [ ] Hardware compatibility testing beyond the initial target ROM
- [x] Performance counters for input, ARM execution, frame copy, and Xbox video

## Milestone 3: Pokémon Black/White target

- [ ] PowerPC code emitter and executable-code cache
- [ ] ARM and Thumb basic-block translation
- [ ] Interpreter fallback for unsupported operations
- [x] Multi-thread software 3D on contexts 2/3/4 with context 5 reserved for video
- [ ] Stable 59.826 FPS, synchronized audio and saving

## Milestone 4: public alpha

- [ ] Compatibility database based on user-owned dumps
- [ ] Save states
- [ ] Screen layouts and scaling options
- [x] Reproducible `.elf32` release builds
- [ ] GPL source release and contributor documentation
