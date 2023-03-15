# Maintaining Dependencies

Node.js depends on additional components beyond the Node.js code
itself. These dependencies provide both native and JavaScript code
and are built together with the code under the `src` and `lib`
directories to create the Node.js binaries.

All dependencies are located within the `deps` directory.

Any code which meets one or more of these conditions should
be managed as a dependency:

* originates in an upstream project and is maintained
  in that upstream project.
* is not built from the `preferred form of the work for
  making modifications to it` (see
  [GNU GPL v2, section 3.](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
  when `make node` is run. A good example is
  WASM code generated from C (the preferred form).
  Typically generation is only supported on a subset of platforms, needs
  additional tools, and is pre-built outside of the `make node`
  step and then committed as a WASM binary in the directory
  for the dependency under the `deps` directory.

By default all dependencies are bundled into the Node.js
binary, however, `configure` options should be available to
use an externalized version at runtime when:

* the dependency provides native code and is available as
  a shared library in one or more of the common Node.js
  distributions.
* the dependency provides JavaScript and is not built
  from the `preferred form of the work for making modifications
  to it` when `make node` is run.

Many distributions use externalized dependencies for one or
more of these reasons:

1. They have a requirement to build everything that they ship
   from the `preferred form of the work for making
   modifications to it`. This means that they need to
   replace any pre-built components (for example WASM
   binaries) with an equivalent that they have built.
2. They manage the dependency separately as it is used by
   more applications than just Node.js. Linking against
   a shared library allows them to manage updates and
   CVE fixes against the library instead of having to
   patch all of the individual applications.
3. They have a system wide configuration for the
   dependency that all applications should respect.

## Supporting externalized dependencies with native code

Support for externalized dependencies with native code for which a
shared library is available can added by:

* adding options to `configure.py`. These are added to the
  shared\_optgroup and include an options to:
  * enable use of a shared library
  * set the name of the shared library
  * set the path to the directory with the includes for the
    shared library
  * set the path to where to find the shared library at
    runtime
* add a call to configure\_library() to `configure.py` for the
  library at the end of list of existing configure\_library() calls.
  If there are additional libraries that are required it is
  possible to list more than one with the `pkgname` option.
* in `node.gypi` guard the build for the dependency
  with `node_shared_depname` so that it is only built if
  the dependency is being bundled into Node.js itself. For example:

```text
    [ 'node_shared_brotli=="false"', {
      'dependencies': [ 'deps/brotli/brotli.gyp:brotli' ],
    }],
```

## Supporting externalizable dependencies with JavaScript code

Support for an externalizable dependency with JavaScript code
can be added by:

* adding an entry to the `sharable_builtins` map in
  `configure.py`. The path should correspond to the file
  within the deps directory that is normally bundled into
  Node.js. For example `deps/cjs-module-lexer/lexer.js`.
  This will add a new option for building with that dependency
  externalized. After adding the entry you can see
  the new option by running `./configure --help`.

* adding a call to `AddExternalizedBuiltin` to the constructor
  for BuildinLoader in `src/node_builtins.cc` for the
  dependency using the `NODE_SHARED_BUILTLIN` #define generated for
  the dependency. After running `./configure` with the new
  option you can find the #define in `config.gypi`. You can cut and
  paste one of the existing entries and then update to match the
  inport name for the dependency and the #define generated.

## Supporting non-externalized dependencies with JavaScript code

If the dependency consists of JavaScript in the
`preferred form of the work for making modifications to it`, it
can be added as a non-externalizable dependency. In this case
simply add the path to the JavaScript file in the `deps_files`
list in the `node.gyp` file.
