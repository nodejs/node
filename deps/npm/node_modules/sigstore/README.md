# sigstore-js &middot; [![npm version](https://img.shields.io/npm/v/sigstore.svg?style=flat)](https://www.npmjs.com/package/sigstore) [![CI Status](https://github.com/sigstore/sigstore-js/workflows/CI/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/ci.yml) [![Smoke Test Status](https://github.com/sigstore/sigstore-js/workflows/smoke-test/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/smoke-test.yml)

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

### tuf

The `tuf` object contains utility function for working with the Sigstore TUF repository.

#### getTarget(path[, options])

Returns the contents of the target at the specified path in the Sigstore TUF repository.

* `path` `<string>`: The [path-relative-url string](https://url.spec.whatwg.org/#path-relative-url-string) that uniquely identifies the target within the Sigstore TUF repository.
* `options` `<Object>`
  * `tufMirrorURL` `<string>`: Base URL for the Sigstore TUF repository. Defaults to `'https://tuf-repo-cdn.sigstore.dev'`
  * `tufRootPath` `<string>`: Path to the initial trusted root for the TUF repository. Defaults to the embedded root.
  * `tufCachePath` `<string>`: Absolute path to the directory to be used for caching downloaded TUF metadata and targets. Defaults to a directory named "sigstore-js" within the platform-specific application data directory.


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

## Credential Sources

### GitHub Actions

If sigstore-js detects that it is being executed on GitHub Actions, it will use `ACTIONS_ID_TOKEN_REQUEST_URL`
and `ACTIONS_ID_TOKEN_REQUEST_TOKEN` environment variables to request an OIDC token with the correct scope.

Note: the `id_token: write` permission must be granted to the GitHub Action Job.

See https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/about-security-hardening-with-openid-connect
for more details.

### Environment Variables

If the `SIGSTORE_ID_TOKEN` environment variable is set, it will use this to authenticate to Fulcio.
It is the callers responsibility to make sure that this token has the correct scopes.

### Interactive Flow

If sigstore-js cannot detect ambient credentials, then it will prompt the user to go through the
interactive flow.

## Development

### Changesets
If you are contributing a user-facing or noteworthy change that should be added to the changelog, you should include a changeset with your PR by running the following command:

```console
npx changeset add
```

Follow the prompts to specify whether the change is a major, minor or patch change. This will create a file in the `.changesets` directory of the repo. This change should be committed and included with your PR.

### Updating Rekor Types

Update the git `REF` in `hack/generate-rekor-types` from the [sigstore/rekor][1] repository.

Generate TypeScript types (should update files in scr/types/rekor/\_\_generated\_\_):

```
npm run codegen:rekor
```

### Release Steps

Whenever a new changeset is merged to the "main" branch, the `release` workflow will open a PR (or append to the existing PR if one is already open) with the all of the pending changesets.

Publishing a release simply requires that you approve/merge this PR. This will trigger the publishing of the package to the npm registry and the creation of the GitHub release.

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
