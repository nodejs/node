import { crypto } from '@sigstore/core';
import type { Envelope } from '@sigstore/bundle';
import type { SignatureContent } from '../shared.types';
export declare class DSSESignatureContent implements SignatureContent {
    private readonly env;
    constructor(env: Envelope);
    compareDigest(digest: Buffer): boolean;
    compareSignature(signature: Buffer): boolean;
    verifySignature(key: crypto.KeyObject): boolean;
    get signature(): Buffer;
    private get preAuthEncoding();
}
