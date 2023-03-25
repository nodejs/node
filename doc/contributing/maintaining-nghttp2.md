# Maintaining nghttp2 in Node.js

The low-level implementation of
[HTTP2](https://nodejs.org/docs/latest/api/http2.html)
is based on [nghttp2](https://nghttp2.org/). Updates are pulled into Node.js
under [deps/nghttp2](https://github.com/nodejs/node/tree/HEAD/deps/nghttp2)
as needed.

The low-level implementation is made available in the Node.js API through
JavaScript code in the [lib](https://github.com/nodejs/node/tree/HEAD/lib)
directory and C++ code in the
[src](https://github.com/nodejs/node/tree/HEAD/src) directory.

## Step 1: Updating nghttp2

The `tools/dep_updaters/update-nghttp2.sh` script automates the update of the
postject source files.

## Step 2: Test the build

```console
$  make test-http2
```

## Step 3: Commit new nghttp2

```console
$ git add -A deps/nghttp2
$ git commit -m "deps: upgrade nghttp2 to x.y.z"
```

## Step 4: Update licenses

```console
$ ./tools/license-builder.sh
# The following commands are only necessary if there are changes
$ git add .
$ git commit -m "doc: update nghttp2 LICENSE using license-builder.sh"
```
