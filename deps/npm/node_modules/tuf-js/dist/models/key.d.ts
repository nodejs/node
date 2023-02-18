import { JSONObject, JSONValue } from '../utils/types';
import { Signable } from './base';
export interface KeyOptions {
    keyID: string;
    keyType: string;
    scheme: string;
    keyVal: Record<string, string>;
    unrecognizedFields?: Record<string, JSONValue>;
}
export declare class Key {
    readonly keyID: string;
    readonly keyType: string;
    readonly scheme: string;
    readonly keyVal: Record<string, string>;
    readonly unrecognizedFields?: Record<string, JSONValue>;
    constructor(options: KeyOptions);
    verifySignature(metadata: Signable): void;
    equals(other: Key): boolean;
    toJSON(): JSONObject;
    static fromJSON(keyID: string, data: JSONObject): Key;
}
