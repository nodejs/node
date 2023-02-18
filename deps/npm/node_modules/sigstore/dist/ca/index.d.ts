/// <reference types="node" />
/// <reference types="node" />
import { KeyObject } from 'crypto';
export interface CA {
    createSigningCertificate: (identityToken: string, publicKey: KeyObject, challenge: Buffer) => Promise<string[]>;
}
export interface CAClientOptions {
    fulcioBaseURL: string;
}
export declare class CAClient implements CA {
    private fulcio;
    constructor(options: CAClientOptions);
    createSigningCertificate(identityToken: string, publicKey: KeyObject, challenge: Buffer): Promise<string[]>;
}
