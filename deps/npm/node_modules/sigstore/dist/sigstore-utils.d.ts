/// <reference types="node" />
import { SignOptions } from './config';
import { SignerFunc } from './types/signature';
import * as sigstore from './types/sigstore';
export declare function createDSSEEnvelope(payload: Buffer, payloadType: string, options: {
    signer: SignerFunc;
}): Promise<sigstore.SerializedEnvelope>;
export declare function createRekorEntry(dsseEnvelope: sigstore.SerializedEnvelope, publicKey: string, options?: SignOptions): Promise<sigstore.SerializedBundle>;
