# ngtcp2 and nghttp3

The ngtcp2 and nghttp3 dependencies provide the core functionality for
QUIC and HTTP/3.

The sources are pulled from:

* ngtcp2: https://github.com/ngtcp2/ngtcp2
* nghttp3: https://github.com/ngtcp2/nghttp3

In both the `ngtcp2` and `nghttp3` git repos, the active development occurs
in the default branch (currently named `main` in each). Tagged versions do not
always point to the default branch.

We only use a subset of the sources for each.

## Updating

The `nghttp3` library depends on `ngtcp2`. Both should always be updated
together. From `ngtcp2` we only want the contents of the `lib` and `crypto`
directories; from `nghttp3` we only want the contents of the `lib` directory.

After updating either dependency, check if any source files or include
directories have been added or removed and update `ngtcp2.gyp` accordingly.

### Updating ngtcp2

To update ngtcp2, replace `v0.8.1` with the desired git tag:

```sh
$ git clone --depth=1 --branch=v0.8.1 https://github.com/ngtcp2/ngtcp2
$ cd ngtcp2
$ autoreconf -i
$ ./configure --prefix=$PWD/build --enable-lib-only
$ cp -R lib/* ../node/deps/ngtcp2/ngtcp2/lib/
$ cp -R crypto/* ../node/deps/ngtcp2/ngtcp2/crypto/
```

### Updating nghttp3

To update nghttp3, replace `v0.7.0` with the desired git tag:

```sh
$ git clone --depth=1 --branch=v0.7.0 https://github.com/ngtcp2/nghttp3
$ cd nghttp3
$ autoreconf -i
$ ./configure --prefix=$PWD/build --enable-lib-only
$ cp -R lib/* ../node/deps/ngtcp2/nghttp3/lib/
```
