/// <reference types="node" />
import { VerifyKeyObjectInput } from 'crypto';
interface KeyInfo {
    keyType: string;
    scheme: string;
    keyVal: string;
}
export declare function getPublicKey(keyInfo: KeyInfo): VerifyKeyObjectInput;
export {};
