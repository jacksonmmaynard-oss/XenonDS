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

- [ ] Confirm the container-built probe on GitHub Actions
- [ ] Boot it through XeLL on a real Xbox 360
- [ ] Statically compile the required DeSmuME interpreter sources
- [ ] Display the first software-rendered DS frame
- [ ] Map Xbox buttons and right-stick touch cursor

## Milestone 2: playable interpreter

- [ ] LibXenon audio ring buffer
- [ ] Battery-backed `.dsv` saves
- [ ] ROM browser and clean error screen
- [ ] Frame pacing and configurable frame skip
- [ ] Performance counters per emulated subsystem

## Milestone 3: Pokémon Black/White target

- [ ] PowerPC code emitter and executable-code cache
- [ ] ARM and Thumb basic-block translation
- [ ] Interpreter fallback for unsupported operations
- [ ] Multi-thread software 3D experiments
- [ ] Stable 59.826 FPS, synchronized audio and saving

## Milestone 4: public alpha

- [ ] Compatibility database based on user-owned dumps
- [ ] Save states
- [ ] Screen layouts and scaling options
- [ ] Reproducible `.elf32` release builds
- [ ] GPL source release and contributor documentation
