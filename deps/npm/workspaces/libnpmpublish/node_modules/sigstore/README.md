# sigstore &middot; [![npm version](https://img.shields.io/npm/v/sigstore.svg?style=flat)](https://www.npmjs.com/package/sigstore) [![CI Status](https://github.com/sigstore/sigstore-js/workflows/CI/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/ci.yml) [![Smoke test](https://github.com/sigstore/sigstore-js/actions/workflows/smoke-test.yml/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/smoke-test.yml)

A JavaScript library for generating and verifying Sigstore signatures. One of
the intended uses is to sign and verify npm packages but it can be used to sign
and verify any file.

## Features

- Support for signing using an OpenID Connect identity
- Support for publishing signatures to a [Rekor][1] instance
- Support for verifying Sigstore bundles

## Prerequisites

- Node.js version ^22.22.2 || ^24.15.0 || >=26.0.0

## Installation

```
npm install sigstore
```

## Compatibility

The following table documents which combinations of Sigstore bundle versions
and Rekor types can be verified by different versions of the `sigstore`
library. It also lists which `sigstore` versions were shipped with different
`npm` CLI versions.

<table>
  <thead>
    <tr>
      <th colspan=2><code>sigstore</code></th>
      <th>1.0</th>
      <th>1.1</th>
      <th>1.2</th>
      <th>1.3</th>
      <th>1.4</th>
      <th>1.5</th>
      <th>1.6</th>
      <th>1.7</th>
      <th>1.8</th>
    </tr>
    <tr>
      <th colspan=2><code>npm</code></th>
      <th>9.5.0</th>
      <th>9.6.2</th>
      <th>9.6.3</th>
      <th>9.6.5</th>
      <th>9.6.6</th>
      <th>9.6.7</th>
      <th>9.7.2</th>
      <th>9.8.0</th>
      <th></th>
    </tr>
    <tr>
      <th>Bundle Version</th>
      <th>Rekor Type</th>
      <th colspan=9></th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td rowspan=3>0.1</td>
      <td>hashedrekord</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
    <tr>
      <td>intoto</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
    <tr>
      <td>dsse</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
    <tr>
      <td rowspan=3>0.2</td>
      <td>hashedrekord</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
    <tr>
      <td>intoto</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
    <tr>
      <td>dsse</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:x:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
      <td>:white_check_mark:</td>
    </tr>
  </tbody>
</table>

## Usage

```javascript
const { attest, verify } = require('sigstore');
```

```javascript
import { attest, verify } from 'sigstore';
```

### sign(payload[, options])

Generates a Sigstore signature for the supplied payload. Returns a
[Sigstore bundle][2] containing the signature and the verification material
necessary to verify the signature.

- `payload` `<Buffer>`: The bytes of the artifact to be signed.
- `options` `<Object>`
  - `fulcioURL` `<string>`: The base URL of the Fulcio instance to use for retrieving the signing certificate. Defaults to `'https://fulcio.sigstore.dev'`.
  - `rekorURL` `<string>`: The base URL of the Rekor instance to use when adding the signature to the transparency log. Defaults to `'https://rekor.sigstore.dev'`.
  - `tsaServerURL` `<string>`: The base URL of the Timestamp Authority instance to use when requesting a signed timestamp. If omitted, no timestamp will be requested.
  - `tlogUpload` `<boolean>`: Flag indicating whether or not the signature should be recorded on the Rekor transparency log. Defaults to `true`.
  - `identityToken` `<string>`: The OIDC token identifying the signer. If no explicit token is supplied, an attempt will be made to retrieve one from the environment. This config cannot be used with `identityProvider`.
  - `identityProvider` `<IdentityProvider>`: Object which implements `getToken: () => Promise<string>`. The supplied provider will be used to retrieve an OIDC token. If no provider is supplied, an attempt will be made to retrieve an OIDC token from the environment. This config cannot be used with `identityToken`.
  - `legacyCompatibility` `<boolean>`: Flag indicating whether to enable legacy compatibility mode. When set to `true`, the returned bundle will use the Sigstore v0.2 bundle format. When unset or `false`, the returned bundle will be v0.3 or greater.

### attest(payload, payloadType[, options])

Generates a Sigstore signature for the supplied in-toto statement. Returns a
[Sigstore bundle][2] containing the [DSSE][3]-wrapped statement and signature
as well as the verification material necessary to verify the signature.

- `payload` `<Buffer>`: The bytes of the statement to be signed.
- `payloadType` `<string>`: MIME or content type describing the statement to be signed.
- `options` `<Object>`
  - `fulcioURL` `<string>`: The base URL of the Fulcio instance to use for retrieving the signing certificate. Defaults to `'https://fulcio.sigstore.dev'`.
  - `rekorURL` `<string>`: The base URL of the Rekor instance to use when adding the signature to the transparency log. Defaults to `'https://rekor.sigstore.dev'`.
  - `tsaServerURL` `<string>`: The base URL of the Timestamp Authority instance to use when requesting a signed timestamp. If omitted, no timestamp will be requested.
  - `tlogUpload` `<boolean>`: Flag indicating whether or not the signed statement should be recorded on the Rekor transparency log. Defaults to `true`.
  - `identityToken` `<string>`: The OIDC token identifying the signer. If no explicit token is supplied, an attempt will be made to retrieve one from the environment. This config cannot be used with `identityProvider`.
  - `identityProvider` `<IdentityProvider>`: Object which implements `getToken: () => Promise<string>`. The supplied provider will be used to retrieve an OIDC token. If no provider is supplied, an attempt will be made to retrieve an OIDC token from the environment. This config cannot be used with `identityToken`.
  - `legacyCompatibility` `<boolean>`: Flag indicating whether to enable legacy compatibility mode. When set to `true`, any record written to the Rekor transparency log will use the "intoto" record type and the returned bundle will use the Sigstore v0.2 bundle format. When unset or `false`, the "dsse" Rekor type will be used and the returned bundle will be v0.3 or greater.

### verify(bundle[, payload][, options])

Verifies the signature in the supplied bundle. Returns a `Signer` object containing the public key and identity information from the verification.

- `bundle` `<Bundle>`: The Sigstore bundle containing the signature to be verified and the verification material necessary to verify the signature.
- `payload` `<Buffer>`: The bytes of the artifact over which the signature was created. Only necessary when the `sign` function was used to generate the signature since the Bundle does not contain any information about the artifact which was signed. Not required when the `attest` function was used to generate the Bundle.
- `options` `<Object>`
  - `ctLogThreshold` `<number>`: The number of certificate transparency logs on which the signing certificate must appear. Defaults to `1`.
  - `tlogThreshold` `<number>`: The number of transparency logs on which the signature must appear. Defaults to `1`.
  - `certificateIssuer` `<string>`: Value that must appear in the signing certificate's issuer extension (OID 1.3.6.1.4.1.57264.1.1). Not verified if no value is supplied.
  - `certificateIdentityEmail` `<string>`: Email address expected in the signing certificate's Subject Alternative Name (SAN) extension. The value is matched as a regular expression against the SAN; for exact matching, use an anchored pattern (e.g. `^user@example\\.com$`). Must be specified in conjunction with the `certificateIssuer` option. Takes precedence over the `certificateIdentityURI` option. Not verified if no value is supplied.
  - `certificateIdentityURI` `<string>`: URI expected in the signing certificate's Subject Alternative Name (SAN) extension. The value is matched as a regular expression against the SAN; for exact matching, use an anchored pattern (e.g. `^https://github\\.com/owner/repo$`). Must be specified in conjunction with the `certificateIssuer` option. Ignored if the `certificateIdentityEmail` option is set. Not verified if no value is supplied.
  - `certificateOIDs` `<Object>`: A collection of OID/value pairs which must be present in the certificate's extension list. Not verified if no value is supplied.
  - `keySelector` `<Function>`: Callback invoked to retrieve the public key (as either `string` or `Buffer`) necessary to verify the bundle signature. Not used when the signature was generated from a Fulcio-issued signing certificate.
    - `hint` `<String>`: The hint from the bundle used to identify the the signing key.

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

[1]: https://github.com/sigstore/rekor
[2]: https://github.com/sigstore/protobuf-specs/blob/9b722b68a717778ba4f11543afa4ef93205ab502/protos/sigstore_bundle.proto#L63-L84
[3]: https://github.com/secure-systems-lab/dsse
[4]: https://github.com/sigstore/cosign
