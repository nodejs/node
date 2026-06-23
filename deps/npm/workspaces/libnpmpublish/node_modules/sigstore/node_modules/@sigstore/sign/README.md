# @sigstore/sign &middot; [![npm version](https://img.shields.io/npm/v/@sigstore/sign.svg?style=flat)](https://www.npmjs.com/package/@sigstore/sign) [![CI Status](https://github.com/sigstore/sigstore-js/workflows/CI/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/ci.yml) [![Smoke Test Status](https://github.com/sigstore/sigstore-js/workflows/smoke-test/badge.svg)](https://github.com/sigstore/sigstore-js/actions/workflows/smoke-test.yml)

A library for generating [Sigstore][1] signatures.

## Features

- Support for keyless signature generation with [Fulcio][2]-issued signing
  certificates
- Support for ambient OIDC credential detection in CI/CD environments
- Support for recording signatures to the [Rekor][3] transparency log
- Support for requesting timestamped countersignature from a [Timestamp
  Authority][4]

## Prerequisites

- Node.js version ^22.22.2 || ^24.15.0 || >=26.0.0

## Installation

```
npm install @sigstore/sign
```

## Overview

This library provides the building blocks for composing custom Sigstore signing
workflows.

### BundleBuilder

The top-level component is the `BundleBuilder` which has responsibility for
taking some artifact and returning a [Sigstore bundle][5] containing the
signature for that artifact and the various materials necessary to verify that
signature.

```typescript
interface BundleBuilder {
  create: (artifact: Artifact) => Promise<Bundle>;
}
```

The artifact to be signed is simply an array of bytes and an optional mimetype.
The type is necessary when the signature is packaged as a [DSSE][6] envelope.

```typescript
type Artifact = {
  data: Buffer;
  type?: string;
};
```

There are two `BundleBuilder` implementations provided as part of this package:

- [`DSSEBundleBuilder`](./src/bundler/dsse.ts) - Combines the verification material and
  artifact signature into a [`dsse_envelope`][7] -style Sigstore bundle
- [`MessageBundleBuilder`](./src/bundler/message.ts) - Combines the verification
  material and artifact signature into a [`message_signature`][8]-style Sigstore
  bundle.

### Signer

Every `BundleBuilder` must be instantiated with a `Signer` implementation. The
`Signer` is responsible for taking a `Buffer` and returning an `Signature`.

```typescript
interface Signer {
  sign: (data: Buffer) => Promise<Signature>;
}
```

The returned `Signature` contains a signature and the public key which can be
used to verify that signature -- the key may either take the form of a x509
certificate or public key.

```typescript
type Signature = {
  signature: Buffer;
  key: KeyMaterial;
};

type KeyMaterial =
  | {
      $case: 'x509Certificate';
      certificate: string;
    }
  | {
      $case: 'publicKey';
      publicKey: string;
      hint?: string;
    };
```

This package provides the [`FulcioSigner`](./src/signer/fulcio/index.ts)
which implements the `Signer` interface and signs the artifact with an
ephemeral keypair. It will also retrieve an OIDC token from the configured
`IdentityProvider` and then request a signing certificate from Fulcio which binds
the ephemeral key to the identity embedded in the token. This signing
certificate is returned as part of the `Signature`.

### Witness

The `BundleBuilder` may also be configured with zero-or-more `Witness`
instances. Each `Witness` receives the artifact signature and the public key
and returns an `VerificationMaterial` which represents some sort of
counter-signature for the artifact's signature.

```typescript
interface Witness {
  testify: (
    signature: SignatureBundle,
    publicKey: string
  ) => Promise<VerificationMaterial>;
}
```

The returned `VerificationMaterial` may contain either Rekor transparency log
entries or RFC3161 timestamps.

```typescript
type VerificationMaterial = {
  tlogEntries?: TransparencyLogEntry[];
  rfc3161Timestamps?: RFC3161SignedTimestamp[];
};
```

The entries in the returned `VerificationMaterial` are automatically added to
the Sigstore `Bundle` by the `BundleBuilder`.

The package provides two different `Witness` implementations:

- [`RekorWitness`](./src/witness/tlog/index.ts) - Adds an entry to the Rekor
  transparency log and returns a `TransparencyLogEntry` to be included in the
  `Bundle`
- [`TSAWitness`](./src/witness/tsa/index.ts) - Requests an RFC3161 timestamp
  over the artifact signature and returns an `RFC3161SignedTimestamp` to be
  included in the `Bundle`

## Usage Example

```typescript
const {
  CIContextProvider,
  DSSEBundleBuilder,
  FulcioSigner,
  RekorWitness,
  TSAWitness,
} = require('@sigstore/sign');

// Set-up the signer
const signer = new FulcioSigner({
  fulcioBaseURL: 'https://fulcio.sigstore.dev',
  identityProvider: new CIContextProvider('sigstore'),
});

// Set-up the witnesses
const rekorWitness = new RekorWitness({
  rekorBaseURL: 'https://rekor.sigstore.dev',
});

const tsaWitness = new TSAWitness({
  tsaBaseURL: 'https://tsa.github.com',
});

// Instantiate a bundle builder
const bundler = new DSSEBundleBuilder({
  signer,
  witnesses: [rekorWitness, tsaWitness],
});

// Sign a thing
const artifact = {
  type: 'text/plain',
  data: Buffer.from('something to be signed'),
};
const bundle = await bundler.create(artifact);
```

[1]: https://www.sigstore.dev
[2]: https://github.com/sigstore/fulcio
[3]: https://github.com/sigstore/rekor
[4]: https://github.com/sigstore/timestamp-authority
[5]: https://github.com/sigstore/protobuf-specs/blob/main/protos/sigstore_bundle.proto
[6]: https://github.com/secure-systems-lab/dsse
[7]: https://github.com/sigstore/protobuf-specs/blob/5ef54068bb534152474c5685f5cd248f38549fbd/protos/sigstore_bundle.proto#L80
[8]: https://github.com/sigstore/protobuf-specs/blob/5ef54068bb534152474c5685f5cd248f38549fbd/protos/sigstore_bundle.proto#L74
