# Maintaining ngtcp2 and nghttp3

The ngtcp2 and nghttp3 libraries form the foundation of the QUIC
implementation. They are acquired from the [ngtcp2/ngtcp2][] and
[ngtcp2/nghttp3][] repositories on GitHub.

ngtcp2 and nghttp3 are tightly related. They will typically be
updated together.

## Updating ngtcp2

Update ngtcp2:
```bash
git clone https://github.com/ngtcp2/ngtcp2
cd ngtcp2
autoreconf -i
./configure --enable-lib-only
cd ..
cp -R ngtcp2/crypto node/deps/ngtcp2
cp -R ngtcp2/lib node/deps/ngtcp2
```

The `autoreconf -i` and `./configure --enable-lib-only` commands
ensure that the necessary template files (such as version.h.in
located in lib/includes/ngtcp2/ are processed appropriately.

Check that Node.js still builds and tests.

## Updating nghttp3

Update nghttp3:
```bash
git clone https://github.com/ngtcp2/nghttp3
cd nghttp3
autoreconf -i
./configure --enable-lib-only
cd ..
cp -R nghttp3/lib node/deps/nghttp3
```

The `autoreconf -i` and `./configure --enable-lib-only` commands
ensure that the necessary template files (such as version.h.in
located in lib/includes/ngtcp2/ are processed appropriately.

Check that Node.js still builds and tests.

## Commiting ngtcp2 and nghttp3

Use: `git add --all deps/ngtcp2` and `git add --all deps/nghttp3`

Commit the changes with a message like
```text
deps: update ngtcp2 and nghttp3

Updated as described in doc/guides/maintaining-ngtcp2-nghttp3.md.
```

[ngtcp2/nghttp3]: https://github.com/ngtcp2/nghttp3
[ngtcp2/ngtcp2]: https://github.com/ngtcp2/ngtcp2
