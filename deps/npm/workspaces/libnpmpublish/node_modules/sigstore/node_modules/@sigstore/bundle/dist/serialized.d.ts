import { Envelope } from '@sigstore/protobuf-specs';
import { Bundle } from './bundle';
import type { OneOf } from './utility';
export declare const bundleFromJSON: (obj: unknown) => Bundle;
export declare const bundleToJSON: (bundle: Bundle) => SerializedBundle;
export declare const envelopeFromJSON: (obj: unknown) => Envelope;
export declare const envelopeToJSON: (envelope: Envelope) => SerializedEnvelope;
type SerializedTLogEntry = {
    logIndex: string;
    logId: {
        keyId: string;
    };
    kindVersion: {
        kind: string;
        version: string;
    } | undefined;
    integratedTime: string;
    inclusionPromise: {
        signedEntryTimestamp: string;
    } | undefined;
    inclusionProof: {
        logIndex: string;
        rootHash: string;
        treeSize: string;
        hashes: string[];
        checkpoint: {
            envelope: string;
        };
    } | undefined;
    canonicalizedBody: string;
};
type SerializedTimestampVerificationData = {
    rfc3161Timestamps: {
        signedTimestamp: string;
    }[];
};
type SerializedMessageSignature = {
    messageDigest: {
        algorithm: string;
        digest: string;
    } | undefined;
    signature: string;
};
export type SerializedEnvelope = {
    payload: string;
    payloadType: string;
    signatures: {
        sig: string;
        keyid: string;
    }[];
};
export type SerializedBundle = {
    mediaType: string;
    verificationMaterial: (OneOf<{
        x509CertificateChain: {
            certificates: {
                rawBytes: string;
            }[];
        };
        publicKey: {
            hint: string;
        };
        certificate: {
            rawBytes: string;
        };
    }> | undefined) & {
        tlogEntries: SerializedTLogEntry[];
        timestampVerificationData: SerializedTimestampVerificationData | undefined;
    };
} & OneOf<{
    dsseEnvelope: SerializedEnvelope;
    messageSignature: SerializedMessageSignature;
}>;
export {};
