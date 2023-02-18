import { JSONObject, JSONValue } from '../utils/types';
import { Signature } from './signature';
export interface Signable {
    signatures: Record<string, Signature>;
    signed: Signed;
}
export interface SignedOptions {
    version?: number;
    specVersion?: string;
    expires?: string;
    unrecognizedFields?: Record<string, JSONValue>;
}
/***
 * A base class for the signed part of TUF metadata.
 *
 * Objects with base class Signed are usually included in a ``Metadata`` object
 * on the signed attribute. This class provides attributes and methods that
 * are common for all TUF metadata types (roles).
 */
export declare abstract class Signed {
    readonly specVersion: string;
    readonly expires: string;
    readonly version: number;
    readonly unrecognizedFields: Record<string, JSONValue>;
    constructor(options: SignedOptions);
    equals(other: Signed): boolean;
    isExpired(referenceTime?: Date): boolean;
    static commonFieldsFromJSON(data: JSONObject): SignedOptions;
    abstract toJSON(): JSONObject;
}
