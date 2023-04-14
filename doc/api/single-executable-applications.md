# Single executable applications

<!--introduced_in=v19.7.0-->

> Stability: 1 - Experimental: This feature is being designed and will change.

<!-- source_link=lib/internal/main/single_executable_application.js -->

This feature allows the distribution of a Node.js application conveniently to a
system that does not have Node.js installed.

Node.js supports the creation of [single executable applications][] by allowing
the injection of a blob prepared by Node.js, which can contain a bundled script,
into the `node` binary. During start up, the program checks if anything has been
injected. If the blob is found, it executes the script in the blob. Otherwise
Node.js operates as it normally does.

The single executable application feature currently only supports running a
single embedded script using the [CommonJS][] module system.

Users can create a single executable application from their bundled script
with the `node` binary itself and any tool which can inject resources into the
binary.

Here are the steps for creating a single executable application using one such
tool, [postject][]:

1. Create a JavaScript file:
   ```console
   $ echo 'console.log(`Hello, ${process.argv[2]}!`);' > hello.js
   ```

2. Create a configuration file building a blob that can be injected into the
   single executable application (see
   [Generating single executable preparation blobs][] for details):
   ```console
   $ echo '{ "main": "hello.js", "output": "sea-prep.blob" }' > sea-config.json
   ```

3. Generate the blob to be injected:
   ```console
   $ node --experimental-sea-config sea-config.json
   ```

4. Create a copy of the `node` executable and name it according to your needs:
   ```console
   $ cp $(command -v node) hello
   ```

5. Remove the signature of the binary (macOS and Windows only):

   * On macOS:

   ```console
   $ codesign --remove-signature hello
   ```

   * On Windows (optional):

   [signtool][] can be used from the installed [Windows SDK][]. If this step is
   skipped, ignore any signature-related warning from postject.

   ```console
   $ signtool remove /s hello
   ```

6. Inject the blob into the copied binary by running `postject` with
   the following options:

   * `hello` - The name of the copy of the `node` executable created in step 2.
   * `NODE_SEA_BLOB` - The name of the resource / note / section in the binary
     where the contents of the blob will be stored.
   * `sea-prep.blob` - The name of the blob created in step 1.
   * `--sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2` - The
     [fuse][] used by the Node.js project to detect if a file has been injected.
   * `--macho-segment-name NODE_SEA` (only needed on macOS) - The name of the
     segment in the binary where the contents of the blob will be
     stored.

   To summarize, here is the required command for each platform:

   * On systems other than macOS:
     ```console
     $ npx postject hello NODE_SEA_BLOB sea-prep.blob \
         --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2
     ```

   * On macOS:
     ```console
     $ npx postject hello NODE_SEA_BLOB sea-prep.blob \
         --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2 \
         --macho-segment-name NODE_SEA
     ```

7. Sign the binary (macOS and Windows only):

   * On macOS:

   ```console
   $ codesign --sign - hello
   ```

   * On Windows (optional):

   A certificate needs to be present for this to work. However, the unsigned
   binary would still be runnable.

   ```console
   $ signtool sign /fd SHA256 hello
   ```

8. Run the binary:
   ```console
   $ ./hello world
   Hello, world!
   ```

## Generating single executable preparation blobs

Single executable preparation blobs that are injected into the application can
be generated using the `--experimental-sea-config` flag of the Node.js binary
that will be used to build the single executable. It takes a path to a
configuration file in JSON format. If the path passed to it isn't absolute,
Node.js will use the path relative to the current working directory.

The configuration currently reads the following top-level fields:

```json
{
  "main": "/path/to/bundled/script.js",
  "output": "/path/to/write/the/generated/blob.blob",
  "disableExperimentalSEAWarning": true / false // Default: false
}
```

If the paths are not absolute, Node.js will use the path relative to the
current working directory. The version of the Node.js binary used to produce
the blob must be the same as the one to which the blob will be injected.

## Notes

### `require(id)` in the injected module is not file based

`require()` in the injected module is not the same as the [`require()`][]
available to modules that are not injected. It also does not have any of the
properties that non-injected [`require()`][] has except [`require.main`][]. It
can only be used to load built-in modules. Attempting to load a module that can
only be found in the file system will throw an error.

Instead of relying on a file based `require()`, users can bundle their
application into a standalone JavaScript file to inject into the executable.
This also ensures a more deterministic dependency graph.

However, if a file based `require()` is still needed, that can also be achieved:

```js
const { createRequire } = require('node:module');
require = createRequire(__filename);
```

### `__filename` and `module.filename` in the injected module

The values of `__filename` and `module.filename` in the injected module are
equal to [`process.execPath`][].

### `__dirname` in the injected module

The value of `__dirname` in the injected module is equal to the directory name
of [`process.execPath`][].

### Single executable application creation process

A tool aiming to create a single executable Node.js application must
inject the contents of the blob prepared with `--experimental-sea-config"`
into:

* a resource named `NODE_SEA_BLOB` if the `node` binary is a [PE][] file
* a section named `NODE_SEA_BLOB` in the `NODE_SEA` segment if the `node` binary
  is a [Mach-O][] file
* a note named `NODE_SEA_BLOB` if the `node` binary is an [ELF][] file

Search the binary for the
`NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0` [fuse][] string and flip the
last character to `1` to indicate that a resource has been injected.

### Platform support

Single-executable support is tested regularly on CI only on the following
platforms:

* Windows
* macOS
* Linux (all distributions [supported by Node.js][] except Alpine and all
  architectures [supported by Node.js][] except s390x and ppc64)

This is due to a lack of better tools to generate single-executables that can be
used to test this feature on other platforms.

Suggestions for other resource injection tools/workflows are welcomed. Please
start a discussion at <https://github.com/nodejs/single-executable/discussions>
to help us document them.

[CommonJS]: modules.md#modules-commonjs-modules
[ELF]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
[Generating single executable preparation blobs]: #generating-single-executable-preparation-blobs
[Mach-O]: https://en.wikipedia.org/wiki/Mach-O
[PE]: https://en.wikipedia.org/wiki/Portable_Executable
[Windows SDK]: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
[`process.execPath`]: process.md#processexecpath
[`require()`]: modules.md#requireid
[`require.main`]: modules.md#accessing-the-main-module
[fuse]: https://www.electronjs.org/docs/latest/tutorial/fuses
[postject]: https://github.com/nodejs/postject
[signtool]: https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool
[single executable applications]: https://github.com/nodejs/single-executable
[supported by Node.js]: https://github.com/nodejs/node/blob/main/BUILDING.md#platform-list
