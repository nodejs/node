V8 includes a test framework that allows you to test the engine. The framework lets you run both our own test suites that are included with the source code and others, currently only the Mozilla tests.

## Running the V8 tests

Before you run the tests, you will have to build V8 with GYP using the instructions [here](http://code.google.com/p/v8-wiki/wiki/BuildingWithGYP)

You can append `.check` to any build target to have tests run for it, e.g.
```
make ia32.release.check
make ia32.check
make release.check
make check # builds and tests everything (no dot before "check"!)
```

Before submitting patches, you should always run the quickcheck target, which builds a fast debug build and runs only the most relevant tests:
```
make quickcheck
```

You can also run tests manually:
```
tools/run-tests.py --arch-and-mode=ia32.release [--outdir=foo]
```

Or you can run individual tests:
```
tools/run-tests.py --arch=ia32 cctest/test-heap/SymbolTable mjsunit/delete-in-eval
```

Run the script with `--help` to find out about its other options, `--outdir` defaults to `out`. Also note that using the `cctest` binary to run multiple tests in one process is not supported.

## Running the Mozilla and Test262 tests

The V8 test framework comes with support for running the Mozilla as well as the Test262 test suite. To download the test suites and then run them for the first time, do the following:

```
tools/run-tests.py --download-data mozilla
tools/run-tests.py --download-data test262
```

To run the tests subsequently, you may omit the flag that downloads the test suite:

```
tools/run-tests.py mozilla
tools/run-tests.py test262
```

Note that V8 fails a number of Mozilla tests because they require Firefox-specific extensions.

## Running the WebKit tests

Sometimes all of the above tests pass but WebKit build bots fail. To make sure WebKit tests pass run:

```
tools/run-tests.py --progress=verbose --outdir=out --arch=ia32 --mode=release webkit --timeout=200
```

Replace --arch and other parameters with values that match your build options.