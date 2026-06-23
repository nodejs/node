import type { Bundle as ProtoBundle, InclusionProof as ProtoInclusionProof, MessageSignature as ProtoMessageSignature, TransparencyLogEntry as ProtoTransparencyLogEntry, VerificationMaterial as ProtoVerificationMaterial } from '@sigstore/protobuf-specs';
import type { WithRequired } from './utility';
export declare const BUNDLE_V01_MEDIA_TYPE = "application/vnd.dev.sigstore.bundle+json;version=0.1";
export declare const BUNDLE_V02_MEDIA_TYPE = "application/vnd.dev.sigstore.bundle+json;version=0.2";
export declare const BUNDLE_V03_LEGACY_MEDIA_TYPE = "application/vnd.dev.sigstore.bundle+json;version=0.3";
export declare const BUNDLE_V03_MEDIA_TYPE = "application/vnd.dev.sigstore.bundle.v0.3+json";
type DsseEnvelopeContent = Extract<ProtoBundle['content'], {
    $case: 'dsseEnvelope';
}>;
type MessageSignatureContent = Extract<ProtoBundle['content'], {
    $case: 'messageSignature';
}>;
export type MessageSignature = WithRequired<ProtoMessageSignature, 'messageDigest'>;
export type VerificationMaterial = WithRequired<ProtoVerificationMaterial, 'content'>;
export type TransparencyLogEntry = WithRequired<ProtoTransparencyLogEntry, 'logId' | 'kindVersion'>;
export type InclusionProof = WithRequired<ProtoInclusionProof, 'checkpoint'>;
export type TLogEntryWithInclusionPromise = WithRequired<TransparencyLogEntry, 'inclusionPromise'>;
export type TLogEntryWithInclusionProof = TransparencyLogEntry & {
    inclusionProof: InclusionProof;
};
export type Bundle = ProtoBundle & {
    verificationMaterial: VerificationMaterial & {
        tlogEntries: TransparencyLogEntry[];
    };
    content: (MessageSignatureContent & {
        messageSignature: MessageSignature;
    }) | DsseEnvelopeContent;
};
export type BundleV01 = Bundle & {
    verificationMaterial: Bundle['verificationMaterial'] & {
        tlogEntries: TLogEntryWithInclusionPromise[];
    };
};
export type BundleLatest = Bundle & {
    verificationMaterial: Bundle['verificationMaterial'] & {
        tlogEntries: TLogEntryWithInclusionProof[];
    };
};
export type BundleWithCertificateChain = Bundle & {
    verificationMaterial: Bundle['verificationMaterial'] & {
        content: Extract<VerificationMaterial['content'], {
            $case: 'x509CertificateChain';
        }>;
    };
};
export type BundleWithSingleCertificate = Bundle & {
    verificationMaterial: Bundle['verificationMaterial'] & {
        content: Extract<VerificationMaterial['content'], {
            $case: 'certificate';
        }>;
    };
};
export type BundleWithPublicKey = Bundle & {
    verificationMaterial: Bundle['verificationMaterial'] & {
        content: Extract<VerificationMaterial['content'], {
            $case: 'publicKey';
        }>;
    };
};
export type BundleWithMessageSignature = Bundle & {
    content: Extract<Bundle['content'], {
        $case: 'messageSignature';
    }>;
};
export type BundleWithDsseEnvelope = Bundle & {
    content: Extract<Bundle['content'], {
        $case: 'dsseEnvelope';
    }>;
};
export declare function isBundleWithCertificateChain(b: Bundle): b is BundleWithCertificateChain;
export declare function isBundleWithPublicKey(b: Bundle): b is BundleWithPublicKey;
export declare function isBundleWithMessageSignature(b: Bundle): b is BundleWithMessageSignature;
export declare function isBundleWithDsseEnvelope(b: Bundle): b is BundleWithDsseEnvelope;
export {};
