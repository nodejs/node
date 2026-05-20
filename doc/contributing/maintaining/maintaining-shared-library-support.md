# Maintaining shared library support

Node.js unofficially supports a build option where Node.js is built as
a shared library. The shared library is called libnode with additional postfixes
as appropriate for the platform (for example libnode.dll on windows).
The shared library provides a way to embed Node.js into other
applications and to have multiple applications use a single copy of
Node.js instead of having to bundle in the full Node.js footprint
into each application. For workloads that require multiple Node.js
instances this can result in a significant footprint savings.

This document provides an outline of the approach and things to look
out for when maintaining the shared library support.

Currently, shared library support has only been tested on:

* Linux
* macOS
* Windows
* AIX

## Building with shared library option

On non-Windows platforms, Node.js is built with the shared library
option by adding `--shared` to the configure step. On Windows
platforms Node.js is built with the shared library option by
adding `dll` to the vcbuild command line.

Once built there are two key components:

* executable - node
* library - libnode

The node executable is a thin wrapper around libnode which is
generated so that we can run the standard Node.js test suite
against the shared library.

The executable and library will have extensions as appropriate
for the platform on which they are built. For
example, node.exe on windows and node on other platforms for
the executable.

libnode may have additional naming components, as an example
in a build on macOS `libnode.105.dylib`. For non-windows platforms
the additional naming components include the `NODE_MODULE_VERSION` and
the appropriate postfix used for shared libraries on the platform.

In cases where an application links against the shared
library it is up to the application developer to add options
so that the shared library can be found by the application or
to set the LIBPATH (AIX), LD\_LIBRARY\_PATH (Linux/Unix), etc.
so that it is found at runtime.

For the node wrapper, on linux and macOS it is built
so that it can find the shared library in one of
the following:

* the same directory as the node executable
* ../lib with the expectation that the executable is
  installed in a `bin` directory at the same level
  as a `lib` directory in which the shared library is
  installed. This is where the default package that
  is build with the shared library option will
  place the executable and library.

For the node wrapper on windows it is built expecting
that both the executable and shared library will
be in the same directory as it common practice on
that platform.

For the node wrapper on AIX, it is built with
the path to the shared library hardcoded as that
is the only option.

## Exports

On windows, functions that may be linked from native
addons or additional Node.js executables need to have
NODE\_EXTERN\_PRIVATE or NODE\_EXTERN otherwise they will
not be exported by the shared library. In the case of
functions used by additional Node.js executables
(ex: `mksnapshot`) a missing NODE\_EXTERN or
NODE\_EXTERN\_PRIVATE will cause the build to fail.
NODE\_EXTERN\_PRIVATE should be used in these cases
unless the intent is to add the function to the
public embedder API.

## Native addons

For regular Node.js builds, running native addons relies on symbols
exported by the node executable. As a result any
pre-built binaries expect symbols to be exported from the executable
instead of the shared library itself.

The node executable and shared library are built and linked
so that the required symbols are exported from the node
executable. This requires some extra work on some platforms
and the process to build the node executable is a good example
of how this can be achieved. Applications that use the shared
library and want to support native addons should employ
a similar technique.

## Testing

There is currently no testing in the regular CI run for PRs. There
are some CI jobs that can be used to test the shared library support and
some are run as part of daily testing. These include:

* [node-test-commit-linux-as-shared-lib](https://ci.nodejs.org/view/Node.js%20Daily/job/node-test-commit-linux-as-shared-lib/)
* <https://ci.nodejs.org/view/All/job/node-test-commit-osx-as-shared-lib/>
* [node-test-commit-aix-shared-lib](https://ci.nodejs.org/view/Node.js%20Daily/job/node-test-commit-aix-shared-lib/)

TODO: add a Job for windows

For code that modifies/affects shared library support these CI jobs should
run and it should be validated that there are no regressions in
the test suite.
