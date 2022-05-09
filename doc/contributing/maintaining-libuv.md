# Maintaining libuv

This documents contains instructions about how to update libuv.

## Updating libuv

[Download](https://github.com/libuv/libuv/releases) the distribution and
signature file of the version to update to.

### Verify the download

Import the public key of the person who signed the distribution:

```console
git tag -l | grep pubkey
pubkey-bnoordhuis
pubkey-cjihrig
pubkey-cjihrig-kb
pubkey-indutny
pubkey-iwuzhere
pubkey-richardlau
pubkey-saghul
pubkey-santigimeno
pubkey-vtjnash
pubkey2022-vtjnash

git show <pubkey-signer> | gpg --import
gpg --verify /path/to/download/libuv-v1.44.1-dist.tar.gz.sign libuv-v1.44.1-dist.tar.gz
gpg: Signature made Wed 09 Mar 2022 06:58:08 PM CET
gpg:                using RSA key CFBB9CA9A5BEAFD70E2B3C5A79A67C55A3679C8B
gpg: Good signature from "Jameson Nash <vtjnash@gmail.com>" [unknown]
gpg: WARNING: This key is not certified with a trusted signature!
gpg:          There is no indication that the signature belongs to the owner.
Primary key fingerprint: CFBB 9CA9 A5BE AFD7 0E2B  3C5A 79A6 7C55 A367 9C8B
```

### Extract the distribution into deps/uv

```bash
cd /path/to/nodejs/node
find deps/uv -maxdepth 1 ! -name . ! -name .. ! -name common.gypi ! -name uv.gyp | xargs rm -rf {}
tar xvzf /path/to/download/libuv-v1.44.1-dist.tar.gz -C deps/uv --strip 1
```

`common.gypi` and `uv.gyp` may require changes and it is recommended inspect to
these files even if the build and tests pass in the next stage.

### Check that Node.js still builds and tests

```console
./configure && make -j8 test
./node -p process.versions.uv
1.44.1
```

### Committing libuv

```console
git add --all deps/uv
```

Commit the changes with a message like:

```text
deps: update libuv to upstream version

Updated as described in doc/contributing/maintaining-libuv.md.
```
