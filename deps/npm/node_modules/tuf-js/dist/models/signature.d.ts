import { JSONObject } from '../utils/types';
export interface SignatureOptions {
    keyID: string;
    sig: string;
}
/**
 * A container class containing information about a signature.
 *
 * Contains a signature and the keyid uniquely identifying the key used
 * to generate the signature.
 *
 * Provide a `fromJSON` method to create a Signature from a JSON object.
 */
export declare class Signature {
    readonly keyID: string;
    readonly sig: string;
    constructor(options: SignatureOptions);
    static fromJSON(data: JSONObject): Signature;
}
