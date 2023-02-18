/// <reference types="node" />
import { CA } from './ca';
import { Provider } from './identity';
import { TLog } from './tlog';
import { SignerFunc } from './types/signature';
import { Bundle } from './types/sigstore';
export interface SignOptions {
    ca: CA;
    tlog: TLog;
    identityProviders: Provider[];
    signer?: SignerFunc;
}
export declare class Signer {
    private ca;
    private tlog;
    private signer;
    private identityProviders;
    constructor(options: SignOptions);
    signBlob(payload: Buffer): Promise<Bundle>;
    signAttestation(payload: Buffer, payloadType: string): Promise<Bundle>;
    private signWithEphemeralKey;
    private getIdentityToken;
}
