import type { TransparencyLogEntry } from '@sigstore/bundle';
import type { RFC3161Timestamp, X509Certificate, crypto } from '@sigstore/core';
import type { ObjectIdentifierValuePair } from '@sigstore/protobuf-specs';
export type CertificateExtensionName = 'issuer';
export type CertificateExtensions = {
    [key in CertificateExtensionName]?: string;
};
export type CertificateIdentity = {
    subjectAlternativeName?: string;
    extensions?: CertificateExtensions;
    oids?: ObjectIdentifierValuePair[];
};
export type VerificationPolicy = CertificateIdentity;
export type Signer = {
    key: crypto.KeyObject;
    identity?: CertificateIdentity;
};
export type Timestamp = {
    $case: 'timestamp-authority';
    timestamp: RFC3161Timestamp;
} | {
    $case: 'transparency-log';
    tlogEntry: TransparencyLogEntry;
};
export type VerificationKey = {
    $case: 'public-key';
    hint: string;
} | {
    $case: 'certificate';
    certificate: X509Certificate;
};
export type SignatureContent = {
    signature: Buffer;
    compareSignature(signature: Buffer): boolean;
    compareDigest(digest: Buffer): boolean;
    verifySignature(key: crypto.KeyObject): boolean;
};
export type TimestampProvider = {
    timestamps: Timestamp[];
};
export type SignatureProvider = {
    signature: SignatureContent;
};
export type KeyProvider = {
    key: VerificationKey;
};
export type TLogEntryProvider = {
    tlogEntries: TransparencyLogEntry[];
};
export type SignedEntity = SignatureProvider & KeyProvider & TimestampProvider & TLogEntryProvider;
