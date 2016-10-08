# v2.0.0 (2015-08-28):
## BREAKING CHANGES
- Use a newer version of tap.
- Removed optional dependency on `buffertools` to make deeper easier to bundle. It'll still use `buffertools` if it's installed, but this way won't spook the horses.
- Removed monkeypatching shims. They're out of date and probably not worth bringing up to date. They belong in a separate library anyway.
- Convert the code base to use `standard`.
