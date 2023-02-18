/// <reference types="node" />
import { Bundle, Envelope, SignOptions } from './sigstore';
import { SignerFunc } from './types/signature';
export declare function createDSSEEnvelope(payload: Buffer, payloadType: string, options: {
    signer: SignerFunc;
}): Promise<Envelope>;
export declare function createRekorEntry(dsseEnvelope: Envelope, publicKey: string, options?: SignOptions): Promise<Bundle>;
