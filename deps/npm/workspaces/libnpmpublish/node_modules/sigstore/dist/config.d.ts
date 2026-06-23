import { DSSEBundleBuilder, IdentityProvider, MessageSignatureBundleBuilder } from '@sigstore/sign';
import { KeyFinderFunc, VerificationPolicy } from '@sigstore/verify';
import type { MakeFetchHappenOptions } from 'make-fetch-happen';
type Retry = MakeFetchHappenOptions['retry'];
type FetchOptions = {
    retry?: Retry;
    timeout?: number | undefined;
};
type KeySelector = (hint: string) => string | Buffer | undefined;
export type SignOptions = {
    fulcioURL?: string;
    identityProvider?: IdentityProvider;
    identityToken?: string;
    rekorURL?: string;
    tlogUpload?: boolean;
    tsaServerURL?: string;
    legacyCompatibility?: boolean;
} & FetchOptions;
export type VerifyOptions = {
    ctLogThreshold?: number;
    tlogThreshold?: number;
    certificateIssuer?: string;
    certificateIdentityEmail?: string;
    certificateIdentityURI?: string;
    certificateOIDs?: Record<string, string>;
    keySelector?: KeySelector;
    tufMirrorURL?: string;
    tufRootPath?: string;
    tufCachePath?: string;
    tufForceCache?: boolean;
} & FetchOptions;
export declare const DEFAULT_RETRY: Retry;
export declare const DEFAULT_TIMEOUT = 5000;
export declare function createBundleBuilder(bundleType: 'messageSignature', options: SignOptions): MessageSignatureBundleBuilder;
export declare function createBundleBuilder(bundleType: 'dsseEnvelope', options: SignOptions): DSSEBundleBuilder;
export declare function createKeyFinder(keySelector: KeySelector): KeyFinderFunc;
export declare function createVerificationPolicy(options: VerifyOptions): VerificationPolicy;
export {};
