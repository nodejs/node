export interface FulcioOptions {
    baseURL: string;
}
export interface CertificateRequest {
    publicKey: {
        content: string;
    };
    signedEmailAddress: string;
}
/**
 * Fulcio API client.
 */
export declare class Fulcio {
    private fetch;
    private baseUrl;
    constructor(options: FulcioOptions);
    createSigningCertificate(idToken: string, request: CertificateRequest): Promise<string>;
}
