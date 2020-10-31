# Perfetto UI

Quick Start
-----------
Run:

```bash
$ git clone https://android.googlesource.com/platform/external/perfetto/
$ cd perfetto
$ tools/install-build-deps --ui
$ tools/gn gen out/debug --args='is_debug=true'
$ tools/ninja -C out/debug ui
```

For more details on `gn` configs see
[Build Instructions](../docs/build-instructions.md).

To run the tests:
```bash
$ out/debug/ui_unittests
$ out/debug/ui_tests
```

To run the tests in watch mode:
```bash
$ out/debug/ui_unittests --watch
```

Finally run:

```bash
$ ./ui/run-dev-server out/debug
```

and navigate to `localhost:10000`.
