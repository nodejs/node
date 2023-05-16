export interface FulcioOptions {
    baseURL: string;
}
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
    signedCertificateEmbeddedSct: {
        chain: {
            certificates: string[];
        };
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
