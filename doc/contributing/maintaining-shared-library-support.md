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

On non-Windows platoforms, Node.js is built with the shared library
option by adding `--shared` to the configure step. On Windows
platofrms Node.js is built with the shared library option by
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
in a build on macOS `libnode.105.dylib`.

In cases where an application links against the shared
library it is up to the application developer to add options
so that the shared library can be found by the application or
to set the LIBPATH (AIX), LD\_LIBRARY\_PATH (Linux/Unix), etc.
so that it is found at runtime.

For the node.exe wrapper, it is built so that it can
find the shared library in one of the following:

* the same directory as the node executable
* ../lib with the expectation that the executable is
  installed in a `bin` directory at the same level
  as a `lib` directory in which the shared library is
  installed. This is where the default package that
  is build with the shared library option will
  place the executable and library.

## Exports

On windows, functions that may be linked from native
addons or additional Node.js executables need to have
NODE\_EXTERN otherwise they will not be exported by
the shared library. In the case of functions used
by additional Node.js executables (ex: `mksnapshot`)
a missing NODE\_EXTERN will cause the build to fail.

## Native addons

For regular Node.js builds, native addons rely on symbols
exported by the node executable. As a result any
pre-built binaries will expect symbols from the executable
instead of the shared library itself.

The current node executable wrapper addresses this by
ensuring that node.lib from node.exe does not get generated
and instead copy libnode.lib to node.lib. This means addons
compiled when referencing the correct node.lib file will correctly
depend on libnode.dll. The down side is that native addons compiled
with stock Node.js will still try to resolve symbols against
node.exe rather than libnode.dll. This would be a problem for
package that use node-addon-api pre-compiled binaries with the
goal of them working across different Node.js versions and
types (standard versus shared library). At this point the
main purpose for the node executable wrapper is for testing
so this is not considered a major issue.

For applications that use the shared library and also
want to support pre-compiled addons it should be possible
to use an approach as follows on windows (and something similar
on other platforms):

* the exports from libnode using dumpbin
* process the dumpbin output to generate a node.def file to be linked
  into node.exe with the /DEF:node.def flag.
  The export entries in node.def would all read my\_symbol=libnode.my\_symbol
  so that node.exe will redirect all exported symbols back to libnode.dll.

However, this has not been validated/tested. It would be
a good contribution for somebody to extend the node executable
wrapper to do this.

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
