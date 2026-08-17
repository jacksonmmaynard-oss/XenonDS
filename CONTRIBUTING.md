# Contributing to XenonDS

Contributions are welcome through focused issues and pull requests.

## Development guidelines

- Keep platform-specific code behind the backend and platform boundaries.
- Include tests for portable core changes.
- Record performance results with the console model, build revision, test
  software, and measurement method.
- Keep commits limited to one clear change where practical.
- Do not commit ROMs, BIOS files, firmware, keys, Microsoft SDK material, or
  other proprietary console files.

## Validation

Run the host test suite before submitting a pull request:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests must use synthetic data or freely redistributable homebrew. Commercial
game images must not be attached to issues or pull requests.
