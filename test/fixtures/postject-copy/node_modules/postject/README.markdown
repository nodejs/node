# postject

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/postmanlabs/postject/tree/main.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/postmanlabs/postject/tree/main)
[![npm version](http://img.shields.io/npm/v/postject.svg)](https://npmjs.org/package/postject)

Easily inject arbitrary read-only resources into executable formats
(Mach-O, PE, ELF) and use it at runtime.

## Install

```sh
npm i -g postject
```

## Usage

### Command line utility

```sh
$ postject -h
Usage: postject [options] <filename> <resource_name> <resource>

Inject arbitrary read-only resources into an executable for use at runtime

Arguments:
  filename                             The executable to inject into
  resource_name                        The resource name to use (section name on Mach-O and ELF, resource name for PE)
  resource                             The resource to inject

Options:
  --macho-segment-name <segment_name>  Name for the Mach-O segment (default: "__POSTJECT")
  --output-api-header                  Output the API header to stdout
  --overwrite                          Overwrite the resource if it already exists
  -h, --help                           display help for command
```

### Using Programatically

```js
const { inject } = require('postject');

await inject('a.out', 'lol', Buffer.from('Hello, world!'));
```

## Building

### Prerequisites

* CMake
* Ninja
* [Emscripten (emsdk)](https://emscripten.org/docs/getting_started/downloads.html)

### Build Command

```sh
$ npm run build
```

The final output is placed in `dist/`, with `main.js` being the
entrypoint.

### Testing

```sh
$ npm test
```

## Design

To ensure maximum capatibility and head off unforeseen issues, the
implementation for each format tries to use that format's standard
practices for embedding binary data. As such, it should be possible
to embed the binary data at build-time as well. The CLI provides the
ability to inject the resources into pre-built executables, with the
goal that the end result should be as close as possible to what is
obtained by embedding them at build-time.

Note: Other runtime injection implementers should search the binary
compiled with `postject-api.h` for the
`POSTJECT_SENTINEL_fce680ab2cc467b6e072b8b5df1996b2:0` fuse and
flip the last character to `1` to indicate that a resource has been
injected. A different fuse can also be used by defining the
`POSTJECT_SENTINEL_FUSE` macro before including `postject-api.h` and
passing the same string to postject with
`--sentinel-fuse <sentinel_fuse>`.

### Windows

For PE executables, the resources are added into the `.rsrc` section,
with the `RT_RCDATA` (raw data) type.

The build-time equivalent is adding the binary data as a resource in
the usual manner, such as the Resource Compiler, and marking it as
`RT_RCDATA`.

The run-time lookup uses the `FindResource` and `LoadResource` APIs.

### macOS

For Mach-O executables, the resources are added as sections inside a
new segment.

The build-time equivalent of embedding binary data with this approach
uses a linker flag: `-sectcreate,__FOO,__foo,content.txt`

The run-time lookup uses APIs from `<mach-o/getsect.h>`.

### Linux

For ELF executables, the resources are added as notes.

The build-time equivalent is to use a linker script.
