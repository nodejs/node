/// <reference types="node" />
import { Bundle } from "./sigstore_bundle";
import { ObjectIdentifierValuePair, PublicKey, SubjectAlternativeName } from "./sigstore_common";
import { TrustedRoot } from "./sigstore_trustroot";
/** The identity of a X.509 Certificate signer. */
export interface CertificateIdentity {
    /** The X.509v3 issuer extension (OID 1.3.6.1.4.1.57264.1.1) */
    issuer: string;
    san: SubjectAlternativeName | undefined;
    /**
     * An unordered list of OIDs that must be verified.
     * All OID/values provided in this list MUST exactly match against
     * the values in the certificate for verification to be successful.
     */
    oids: ObjectIdentifierValuePair[];
}
export interface CertificateIdentities {
    identities: CertificateIdentity[];
}
export interface PublicKeyIdentities {
    publicKeys: PublicKey[];
}
/**
 * A light-weight set of options/policies for identifying trusted signers,
 * used during verification of a single artifact.
 */
export interface ArtifactVerificationOptions {
    signers?: {
        $case: "certificateIdentities";
        certificateIdentities: CertificateIdentities;
    } | {
        $case: "publicKeys";
        publicKeys: PublicKeyIdentities;
    };
    /**
     * Optional options for artifact transparency log verification.
     * If none is provided, the default verification options are:
     * Threshold: 1
     * Online verification: false
     * Disable: false
     */
    tlogOptions?: ArtifactVerificationOptions_TlogOptions | undefined;
    /**
     * Optional options for certificate transparency log verification.
     * If none is provided, the default verification options are:
     * Threshold: 1
     * Detached SCT: false
     * Disable: false
     */
    ctlogOptions?: ArtifactVerificationOptions_CtlogOptions | undefined;
    /**
     * Optional options for certificate signed timestamp verification.
     * If none is provided, the default verification options are:
     * Threshold: 1
     * Disable: false
     */
    tsaOptions?: ArtifactVerificationOptions_TimestampAuthorityOptions | undefined;
}
export interface ArtifactVerificationOptions_TlogOptions {
    /** Number of transparency logs the entry must appear on. */
    threshold: number;
    /** Perform an online inclusion proof. */
    performOnlineVerification: boolean;
    /** Disable verification for transparency logs. */
    disable: boolean;
}
export interface ArtifactVerificationOptions_CtlogOptions {
    /**
     * The number of ct transparency logs the certificate must
     * appear on.
     */
    threshold: number;
    /**
     * Expect detached SCTs.
     * This is not supported right now as we can't capture an
     * detached SCT in the bundle.
     */
    detachedSct: boolean;
    /** Disable ct transparency log verification */
    disable: boolean;
}
export interface ArtifactVerificationOptions_TimestampAuthorityOptions {
    /** The number of signed timestamps that are expected. */
    threshold: number;
    /** Disable signed timestamp verification. */
    disable: boolean;
}
export interface Artifact {
    data?: {
        $case: "artifactUri";
        artifactUri: string;
    } | {
        $case: "artifact";
        artifact: Buffer;
    };
}
/**
 * Input captures all that is needed to call the bundle verification method,
 * to verify a single artifact referenced by the bundle.
 */
export interface Input {
    /**
     * The verification materials provided during a bundle verification.
     * The running process is usually preloaded with a "global"
     * dev.sisgtore.trustroot.TrustedRoot.v1 instance. Prior to
     * verifying an artifact (i.e a bundle), and/or based on current
     * policy, some selection is expected to happen, to filter out the
     * exact certificate authority to use, which transparency logs are
     * relevant etc. The result should b ecaptured in the
     * `artifact_trust_root`.
     */
    artifactTrustRoot: TrustedRoot | undefined;
    artifactVerificationOptions: ArtifactVerificationOptions | undefined;
    bundle: Bundle | undefined;
    /**
     * If the bundle contains a message signature, the artifact must be
     * provided.
     */
    artifact?: Artifact | undefined;
}
export declare const CertificateIdentity: {
    fromJSON(object: any): CertificateIdentity;
    toJSON(message: CertificateIdentity): unknown;
};
export declare const CertificateIdentities: {
    fromJSON(object: any): CertificateIdentities;
    toJSON(message: CertificateIdentities): unknown;
};
export declare const PublicKeyIdentities: {
    fromJSON(object: any): PublicKeyIdentities;
    toJSON(message: PublicKeyIdentities): unknown;
};
export declare const ArtifactVerificationOptions: {
    fromJSON(object: any): ArtifactVerificationOptions;
    toJSON(message: ArtifactVerificationOptions): unknown;
};
export declare const ArtifactVerificationOptions_TlogOptions: {
    fromJSON(object: any): ArtifactVerificationOptions_TlogOptions;
    toJSON(message: ArtifactVerificationOptions_TlogOptions): unknown;
};
export declare const ArtifactVerificationOptions_CtlogOptions: {
    fromJSON(object: any): ArtifactVerificationOptions_CtlogOptions;
    toJSON(message: ArtifactVerificationOptions_CtlogOptions): unknown;
};
export declare const ArtifactVerificationOptions_TimestampAuthorityOptions: {
    fromJSON(object: any): ArtifactVerificationOptions_TimestampAuthorityOptions;
    toJSON(message: ArtifactVerificationOptions_TimestampAuthorityOptions): unknown;
};
export declare const Artifact: {
    fromJSON(object: any): Artifact;
    toJSON(message: Artifact): unknown;
};
export declare const Input: {
    fromJSON(object: any): Input;
    toJSON(message: Input): unknown;
};
