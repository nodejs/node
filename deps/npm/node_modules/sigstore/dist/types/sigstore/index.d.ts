/// <reference types="node" />
import { Entry } from '../../tlog';
import { x509Certificate } from '../../x509/cert';
import { SignatureMaterial } from '../signature';
import { WithRequired } from '../utility';
import { ValidBundle } from './validate';
import { Envelope } from './__generated__/envelope';
import { Bundle, VerificationMaterial } from './__generated__/sigstore_bundle';
import { TransparencyLogEntry } from './__generated__/sigstore_rekor';
import { ArtifactVerificationOptions } from './__generated__/sigstore_verification';
export * from './serialized';
export * from './validate';
export * from './__generated__/envelope';
export * from './__generated__/sigstore_bundle';
export * from './__generated__/sigstore_common';
export { TransparencyLogEntry } from './__generated__/sigstore_rekor';
export * from './__generated__/sigstore_trustroot';
export * from './__generated__/sigstore_verification';
export declare const bundleToJSON: (message: Bundle) => unknown;
export declare const bundleFromJSON: (obj: any) => ValidBundle;
export declare const envelopeToJSON: (message: Envelope) => unknown;
export declare const envelopeFromJSON: (object: any) => Envelope;
export type BundleWithVerificationMaterial = WithRequired<Bundle, 'verificationMaterial'>;
export declare function isBundleWithVerificationMaterial(bundle: Bundle): bundle is BundleWithVerificationMaterial;
export type BundleWithCertificateChain = Bundle & {
    verificationMaterial: VerificationMaterial & {
        content: Extract<VerificationMaterial['content'], {
            $case: 'x509CertificateChain';
        }>;
    };
};
export declare function isBundleWithCertificateChain(bundle: Bundle): bundle is BundleWithCertificateChain;
export type RequiredArtifactVerificationOptions = WithRequired<ArtifactVerificationOptions, 'ctlogOptions' | 'tlogOptions'>;
export type CAArtifactVerificationOptions = WithRequired<ArtifactVerificationOptions, 'ctlogOptions'> & {
    signers?: Extract<ArtifactVerificationOptions['signers'], {
        $case: 'certificateIdentities';
    }>;
};
export declare function isCAVerificationOptions(options: ArtifactVerificationOptions): options is CAArtifactVerificationOptions;
export type VerifiableTransparencyLogEntry = WithRequired<TransparencyLogEntry, 'logId' | 'inclusionPromise' | 'kindVersion'>;
export declare function isVerifiableTransparencyLogEntry(entry: TransparencyLogEntry): entry is VerifiableTransparencyLogEntry;
export declare const bundle: {
    toDSSEBundle: (envelope: Envelope, signature: SignatureMaterial, rekorEntry: Entry) => Bundle;
    toMessageSignatureBundle: (digest: Buffer, signature: SignatureMaterial, rekorEntry: Entry) => Bundle;
};
export declare function signingCertificate(bundle: Bundle): x509Certificate | undefined;
