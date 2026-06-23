import type { FetchOptions } from '../../types/fetch';
export interface CA {
    createSigningCertificate: (identityToken: string, publicKey: string, challenge: Buffer) => Promise<string[]>;
}
export type CAClientOptions = {
    fulcioBaseURL: string;
} & FetchOptions;
export declare class CAClient implements CA {
    private fulcio;
    constructor(options: CAClientOptions);
    createSigningCertificate(identityToken: string, publicKey: string, challenge: Buffer): Promise<string[]>;
}
