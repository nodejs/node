# Node.js Releases

<!-- TODO(joyeecheung): merge it into Release#readme? -->

Node.js releases are produced in three different release types: Current, LTS, and Nightly.

Current and LTS releases follow [semantic versioning](https://semver.org).
A member of the Release Team [signs](#release-keys) each Current and LTS release.
For more information, see the [Release README](https://github.com/nodejs/Release#readme).

## Current

Under active development. Code for the Current release is in the
branch for its major version number (for example,
[v22.x](https://github.com/nodejs/node/tree/v22.x)). Node.js releases a new
major version every 6 months, allowing for breaking changes. This happens in
April and October every year. Releases appearing each October have a support
life of 8 months. Releases appearing each April convert to LTS (see below)
each October.

The latest Current releases are available in <https://nodejs.org/download/release/latest>.

## LTS

Releases that receive Long Term Support, with a focus on stability
and security. Every even-numbered major version will become an LTS release.
LTS releases receive 12 months of _Active LTS_ support and a further 18 months
of _Maintenance_. LTS release lines have alphabetically-ordered code names,
beginning with v4 Argon. There are no breaking changes or feature additions,
except in some special circumstances.

LTS releases are available in <https://nodejs.org/download/release/>. They have
the `{ "lts": true }` property in [the index](https://nodejs.org/download/release/index.json).
The `latest-$codename` directory is an alias for the latest release from an LTS line. For example,
the [latest-hydrogen](https://nodejs.org/download/release/latest-hydrogen/)
directory contains the latest Hydrogen (Node.js 18) release.

## Nightly

Code from the Current branch built every 24-hours when there are changes. Use with caution.

The nightly releases are available in <https://nodejs.org/download/nightly/>.
Each directory and filename includes the version (e.g., `v22.0.0`),
followed by the UTC date (e.g., `20240424` for April 24, 2024),
and the short commit SHA of the HEAD of the release (e.g., `ddd0a9e494`).
For instance, a full directory name might look like `v22.0.0-nightly20240424ddd0a9e494`.

## Verifying binaries

Download directories contain a `SHASUMS256.txt.asc` file with SHA checksums for the
files and the releaser PGP signature.

You can get a trusted keyring from nodejs/release-keys, e.g. using `curl`:

```bash
curl -fsLo "/path/to/nodejs-keyring.kbx" "https://github.com/nodejs/release-keys/raw/HEAD/gpg/pubring.kbx"
```

Alternatively, you can import the releaser keys in your default keyring, see
[Release keys](./README.md#release-keys) for commands to how to do that.

Then, you can verify the files you've downloaded locally
(if you're using your default keyring, pass `--keyring="${GNUPGHOME:-~/.gnupg}/pubring.kbx"`):

```bash
curl -fsO "https://nodejs.org/dist/${VERSION}/SHASUMS256.txt.asc" \
&& gpgv --keyring="/path/to/nodejs-keyring.kbx" --output SHASUMS256.txt < SHASUMS256.txt.asc \
&& shasum --check SHASUMS256.txt --ignore-missing
```
