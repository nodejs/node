# sigstore-js

A JavaScript library for generating and verifying Sigstore signatures. One of
the intended uses is to sign and verify npm packages but it can be used to sign
and verify any file.

## Features

* Support for signing using an OpenID Connect identity
* Support for publishing signatures to a [Rekor][1] instance
* Support for verifying Sigstore bundles

## Prerequisites

- Node.js version >= 14.17.0

## Installation

```
npm install sigstore
```

## Usage

```javascript
const { sigstore } = require('sigstore')
```

```javascript
import { sigstore } from 'sigstore'
```

### sign(payload[, options])

Generates a Sigstore signature for the supplied payload. Returns a
[Sigstore bundle][2] containing the signature and the verification material
necessary to verify the signature.

* `payload` `<Buffer>`: The bytes of the artifact to be signed.
* `options` `<Object>`
  * `fulcioURL` `<string>`: The base URL of the Fulcio instance to use for retrieving the signing certificate. Defaults to `'https://fulcio.sigstore.dev'`.
  * `rekorURL` `<string>`: The base URL of the Rekor instance to use when adding the signature to the transparency log. Defaults to `'https://rekor.sigstore.dev'`.
  * `identityToken` `<string>`: The OIDC token identifying the signer. If no explicit token is supplied, an attempt will be made to retrieve one from the environment.

### attest(payload, payloadType[, options])

Generates a Sigstore signature for the supplied in-toto statement. Returns a
[Sigstore bundle][2] containing the [DSSE][3]-wrapped statement and signature
as well as the verification material necessary to verify the signature.

* `payload` `<Buffer>`: The bytes of the statement to be signed.
* `payloadType` `<string>`: MIME or content type describing the statement to be signed.
* `options` `<Object>`
  * `fulcioURL` `<string>`: The base URL of the Fulcio instance to use for retrieving the signing certificate. Defaults to `'https://fulcio.sigstore.dev'`.
  * `rekorURL` `<string>`: The base URL of the Rekor instance to use when adding the signature to the transparency log. Defaults to `'https://rekor.sigstore.dev'`.
  * `identityToken` `<string>`: The OIDC token identifying the signer. If no explicit token is supplied, an attempt will be made to retrieve one from the environment.


### verify(bundle[, payload][, options])

Verifies the signature in the supplied bundle.

* `bundle` `<Bundle>`: The Sigstore bundle containing the signature to be verified and the verification material necessary to verify the signature.
* `payload` `<Buffer>`: The bytes of the artifact over which the signature was created. Only necessary when the `sign` function was used to generate the signature since the Bundle does not contain any information about the artifact which was signed. Not required when the `attest` function was used to generate the Bundle.
* `options` `<Object>`
  * `ctLogThreshold` `<number>`: The number of certificate transparency logs on which the signing certificate must appear. Defaults to `1`.
  * `tlogThreshold` `<number>`: The number of transparency logs on which the signature must appear. Defaults to `1`.
  * `certificateIssuer` `<string>`: Value that must appear in the signing certificate's issuer extension (OID 1.3.6.1.4.1.57264.1.1). Not verified if no value is supplied.
  * `certificateIdentityEmail` `<string>`: Email address which must appear in the signing certificate's Subject Alternative Name (SAN) extension. Must be specified in conjunction with the `certificateIssuer` option. Takes precedence over the `certificateIdentityURI` option. Not verified if no value is supplied.
  * `certificateIdentityURI` `<string>`: URI which must appear in the signing certificate's Subject Alternative Name (SAN) extension. Must be specified in conjunction with the `certificateIssuer` option. Ignored if the `certificateIdentityEmail` option is set. Not verified if no value is supplied.
  * `certificateOIDs` `<Object>`: A collection of OID/value pairs which must be present in the certificate's extension list. Not verified if no value is supplied.
  * `keySelector` `<Function>`: Callback invoked to retrieve the public key (as either `string` or `Buffer`) necessary to verify the bundle signature. Not used when the signature was generated from a Fulcio-issued signing certificate.
    * `hint` `<String>`: The hint from the bundle used to identify the the signing key.


### utils

The `utils` object contains a few internal utility functions. These are exposed
to support the needs of specific `sigstore-js` consumers but should **NOT** be
considered part of the stable public interface.

## CLI

The `sigstore-js` library comes packaged with a basic command line interface
for testing and demo purposes. However, the CLI should **NOT** be considered
part of the stable interface of the library. If you require a production-ready
Sigstore CLI, we recommend you use [`cosign`][4].

```shell
$ npx sigstore help
sigstore <command> <artifact>

  Usage:

  sigstore sign         sign an artifact
  sigstore attest       sign an artifact using dsse (Dead Simple Signing Envelope)
  sigstore verify       verify an artifact
  sigstore version      print version information
  sigstore help         print help information
```

## Development

### Updating protobufs

[Docker](https://docs.docker.com/engine/install/) is required to generate protobufs for the `.sigstore` bundle format.

Install Docker on MacOS using [Homebrew](https://brew.sh/):

```
brew install --cask docker && open -a Docker
```

View [Docker install instructions](https://docs.docker.com/engine/install/) for other platforms.

#### Updating Sigstore Protobufs

Update the Git `REF` in `hack/generate-bundle-types` from the [sigstore/protobuf-specs][5] repository.

Generate TypeScript protobufs (should update files in scr/types/sigstore/\_\_generated\_\_):

```
npm run codegen:bundle
```

#### Updating Rekor Types

Update the git `REF` in `hack/generate-rekor-types` from the [sigstore/rekor][1] repository.

Generate TypeScript types (should update files in scr/types/rekor/\_\_generated\_\_):

```
npm run codegen:rekor
```

### Release Steps

1. Update the version inside `package.json` and run `npm i` to update `package-lock.json`.
2. PR the changes above and merge to the "main" branch when approved.
3. Use either the "[Create a new release](https://github.com/sigstore/sigstore-js/releases/new)" link or the `gh release create` CLI command to start a new release.
4. Tag the release with a label matching the version in the `package.json` (with a `v` prefix).
5. Add a title matching the tag.
6. Add release notes.
7. Mark as pre-release as appropriate.
8. Publish release.

After publishing the release, a new npm package will be built and published automatically to the npm registry.
## Licensing

`sigstore-js` is licensed under the Apache 2.0 License.

## Contributing

See [the contributing docs][7] for details.

## Code of Conduct
Everyone interacting with this project is expected to follow the [sigstore Code of Conduct][8].

## Security

Should you discover any security issues, please refer to sigstore's [security process][9].

## Info

`sigstore-js` is developed as part of the [`sigstore`][6] project.

We also use a [slack channel][10]! Click [here][11] for the invite link.


[1]: https://github.com/sigstore/rekor
[2]: https://github.com/sigstore/protobuf-specs/blob/9b722b68a717778ba4f11543afa4ef93205ab502/protos/sigstore_bundle.proto#L63-L84
[3]: https://github.com/secure-systems-lab/dsse
[4]: https://github.com/sigstore/cosign
[5]: https://github.com/sigstore/protobuf-specs
[6]: https://sigstore.dev
[7]: https://github.com/sigstore/.github/blob/main/CONTRIBUTING.md
[8]: https://github.com/sigstore/.github/blob/main/CODE_OF_CONDUCT.md
[9]: https://github.com/sigstore/.github/blob/main/SECURITY.md
[10]: https://sigstore.slack.com
[11]: https://join.slack.com/t/sigstore/shared_invite/zt-mhs55zh0-XmY3bcfWn4XEyMqUUutbUQ
