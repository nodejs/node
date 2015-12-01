# ECMAScript 402

V8 optionally implements the [ECMAScript 402](http://www.ecma-international.org/ecma-402/1.0/) API. The API is enabled by default, but can be turned off at compile time.


## Prerequisites

The i18n implementation adds a dependency on ICU. If you run

```
make dependencies
```

a suitable version of ICU is checked out into `third_party/icu`.


### Alternative ICU checkout

You can check out the ICU sources at a different location and define the gyp variable `icu_gyp_path` to point at the `icu.gyp` file.


### System ICU

Last but not least, you can compile V8 against a version of ICU installed in your system. To do so, specify the gyp variable `use_system_icu=1`. If you also have `want_separate_host_toolset` enabled, the bundled ICU will still be compiled to generate the V8 snapshot. The system ICU will only be used for the target architecture.


## Embedding V8

If you embed V8 in your application, but your application itself doesn't use ICU, you will need to initialize ICU before calling into V8 by executing:

```
v8::V8::InitializeICU();
```

It is safe to invoke this method if ICU was not compiled in, then it does nothing.


## Compiling without i18n support

To build V8 without i18n support use

```
make i18nsupport=off native
```