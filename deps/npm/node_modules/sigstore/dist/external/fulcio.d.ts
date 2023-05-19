import type { FetchOptions } from '../types/fetch';
export type FulcioOptions = {
    baseURL: string;
} & FetchOptions;
export interface SigningCertificateRequest {
    credentials: {
        oidcIdentityToken: string;
    };
    publicKeyRequest: {
        publicKey: {
            algorithm: string;
            content: string;
        };
        proofOfPossession: string;
    };
}
export interface SigningCertificateResponse {
    signedCertificateEmbeddedSct?: {
        chain: {
            certificates: string[];
        };
    };
    signedCertificateDetachedSct?: {
        chain: {
            certificates: string[];
        };
        signedCertificateTimestamp: string;
    };
}
/**
 * Fulcio API client.
 */
export declare class Fulcio {
    private fetch;
    private baseUrl;
    constructor(options: FulcioOptions);
    createSigningCertificate(request: SigningCertificateRequest): Promise<SigningCertificateResponse>;
}
