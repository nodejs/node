import { CA } from './ca';
import { Provider } from './identity';
import { TLog } from './tlog';
import { TSA } from './tsa';
import * as sigstore from './types/sigstore';
import type { FetchOptions, Retry } from './types/fetch';
import type { KeySelector } from './verify';
interface CAOptions {
    fulcioURL?: string;
}
interface TLogOptions {
    rekorURL?: string;
}
interface TSAOptions {
    tsaServerURL?: string;
}
export interface IdentityProviderOptions {
    identityToken?: string;
    oidcIssuer?: string;
    oidcClientID?: string;
    oidcClientSecret?: string;
    oidcRedirectURL?: string;
}
export type TUFOptions = {
    tufMirrorURL?: string;
    tufRootPath?: string;
    tufCachePath?: string;
} & FetchOptions;
export type SignOptions = {
    identityProvider?: Provider;
    tlogUpload?: boolean;
} & CAOptions & TLogOptions & TSAOptions & FetchOptions & IdentityProviderOptions;
export type VerifyOptions = {
    ctLogThreshold?: number;
    tlogThreshold?: number;
    certificateIssuer?: string;
    certificateIdentityEmail?: string;
    certificateIdentityURI?: string;
    certificateOIDs?: Record<string, string>;
    keySelector?: KeySelector;
} & TLogOptions & TUFOptions;
export type CreateVerifierOptions = {
    keySelector?: KeySelector;
} & TUFOptions;
export declare const DEFAULT_FULCIO_URL = "https://fulcio.sigstore.dev";
export declare const DEFAULT_REKOR_URL = "https://rekor.sigstore.dev";
export declare const DEFAULT_RETRY: Retry;
export declare const DEFAULT_TIMEOUT = 5000;
export declare function createCAClient(options: CAOptions & FetchOptions): CA;
export declare function createTLogClient(options: TLogOptions & FetchOptions): TLog;
export declare function createTSAClient(options: TSAOptions & FetchOptions): TSA | undefined;
export declare function artifactVerificationOptions(options: VerifyOptions): sigstore.RequiredArtifactVerificationOptions;
export declare function identityProviders(options: IdentityProviderOptions): Provider[];
export {};
