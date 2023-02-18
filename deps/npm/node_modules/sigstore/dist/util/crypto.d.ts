/// <reference types="node" />
/// <reference types="node" />
/// <reference types="node" />
import { BinaryLike, KeyLike, KeyPairKeyObjectResult } from 'crypto';
export declare function generateKeyPair(): KeyPairKeyObjectResult;
export declare function createPublicKey(key: string | Buffer): KeyLike;
export declare function signBlob(data: NodeJS.ArrayBufferView, privateKey: KeyLike): Buffer;
export declare function verifyBlob(data: Buffer, key: KeyLike, signature: Buffer, algorithm?: string): boolean;
export declare function hash(data: BinaryLike): Buffer;
export declare function randomBytes(count: number): Buffer;
