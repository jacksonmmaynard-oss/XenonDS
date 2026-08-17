# Contributing to XenonDS

XenonDS is currently an engineering prototype. Small, measured changes are
more useful than broad rewrites.

## Before opening a change

1. Build the host utilities and run `ctest --test-dir build --output-on-failure`.
2. Keep platform-specific code behind the backend and platform boundaries.
3. Do not commit ROMs, BIOS files, firmware, keys, Microsoft SDK material, or
   other copyrighted console files.
4. Record performance measurements with the console model, build revision,
   test software, and measurement method.

## Near-term work

- Complete the minimal DeSmuME interpreter source list for LibXenon.
- Add an audio ring buffer behind a platform-neutral sink.
- Add a right-stick touch cursor and configurable screen layout.
- Profile the interpreter before designing the PowerPC dynamic recompiler.

Tests should use synthetic headers or freely redistributable homebrew. Please
do not upload commercial game images to issues or pull requests.
