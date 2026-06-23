import { crypto } from '@sigstore/core';
import type { MessageSignature } from '@sigstore/bundle';
import type { SignatureContent } from '../shared.types';
export declare class MessageSignatureContent implements SignatureContent {
    readonly signature: Buffer;
    private readonly messageDigest;
    private readonly artifact;
    private readonly hashAlgorithm;
    constructor(messageSignature: MessageSignature, artifact: Buffer);
    compareSignature(signature: Buffer): boolean;
    compareDigest(digest: Buffer): boolean;
    verifySignature(key: crypto.KeyObject): boolean;
}
