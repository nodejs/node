# Maintaining Single Executable Applications support

Support for [single executable applications][] is one of the key technical
priorities identified for the success of Node.js.

## High level strategy

From the [Next-10 discussions][] there are 2 approaches the project believes are
important to support:

### Compile with Node.js into executable

This is the approach followed by [boxednode][].

No additional code within the Node.js project is needed to support the
option of compiling a bundled application along with Node.js into a single
executable application.

### Bundle into existing Node.js executable

This is the approach followed by [pkg][].

The project does not plan to provide the complete solution but instead the key
elements which are required in the Node.js executable in order to enable
bundling with the pre-built Node.js binaries. This includes:

* Looking for a segment within the executable that holds bundled code.
* Running the bundled code when such a segment is found.

It is left up to external tools/solutions to:

* Bundle code into a single script.
* Generate a command line with appropriate options.
* Add a segment to an existing Node.js executable which contains
  the command line and appropriate headers.
* Re-generate or removing signatures on the resulting executable
* Provide a virtual file system, and hooking it in if needed to
  support native modules or reading file contents.

However, the project also maintains a separate tool, [postject][], for injecting
arbitrary read-only resources into the binary such as those needed for bundling
the application into the runtime.

## Planning

Planning for this feature takes place in the [single-executable repository][].

## Upcoming features

Currently, only running a single embedded CommonJS file is supported but support
for the following features are in the list of work we'd like to get to:

* Running an embedded ESM file.
* Running an archive of multiple files.
* Embedding [Node.js CLI options][] into the binary.
* [XCOFF][] executable format.
* Run tests on Alpine Linux.
* Run tests on s390x Linux.
* Run tests on ppc64 Linux.

## Disabling single executable application support

To disable single executable application support, build Node.js with the
`--disable-single-executable-application` configuration option.

## Implementation

When built with single executable application support, the Node.js process uses
[`postject-api.h`][] to check if the `NODE_SEA_BLOB` section exists in the
binary. If it is found, it passes the buffer to
[`single_executable_application.js`][], which executes the contents of the
embedded script.

[Next-10 discussions]: https://github.com/nodejs/next-10/blob/main/meetings/summit-nov-2021.md#single-executable-applications
[Node.js CLI options]: https://nodejs.org/api/cli.html
[XCOFF]: https://www.ibm.com/docs/en/aix/7.2?topic=formats-xcoff-object-file-format
[`postject-api.h`]: https://github.com/nodejs/node/blob/71951a0e86da9253d7c422fa2520ee9143e557fa/test/fixtures/postject-copy/node_modules/postject/dist/postject-api.h
[`single_executable_application.js`]: https://github.com/nodejs/node/blob/main/lib/internal/main/single_executable_application.js
[boxednode]: https://github.com/mongodb-js/boxednode
[pkg]: https://github.com/vercel/pkg
[postject]: https://github.com/nodejs/postject
[single executable applications]: https://github.com/nodejs/node/blob/main/doc/contributing/technical-priorities.md#single-executable-applications
[single-executable repository]: https://github.com/nodejs/single-executable
