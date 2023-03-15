/// <reference types="node" />
/** An authenticated message of arbitrary type. */
export interface Envelope {
    /**
     * Message to be signed. (In JSON, this is encoded as base64.)
     * REQUIRED.
     */
    payload: Buffer;
    /**
     * String unambiguously identifying how to interpret payload.
     * REQUIRED.
     */
    payloadType: string;
    /**
     * Signature over:
     *     PAE(type, body)
     * Where PAE is defined as:
     * PAE(type, body) = "DSSEv1" + SP + LEN(type) + SP + type + SP + LEN(body) + SP + body
     * +               = concatenation
     * SP              = ASCII space [0x20]
     * "DSSEv1"        = ASCII [0x44, 0x53, 0x53, 0x45, 0x76, 0x31]
     * LEN(s)          = ASCII decimal encoding of the byte length of s, with no leading zeros
     * REQUIRED (length >= 1).
     */
    signatures: Signature[];
}
export interface Signature {
    /**
     * Signature itself. (In JSON, this is encoded as base64.)
     * REQUIRED.
     */
    sig: Buffer;
    /**
     * Unauthenticated* hint identifying which public key was used.
     * OPTIONAL.
     */
    keyid: string;
}
export declare const Envelope: {
    fromJSON(object: any): Envelope;
    toJSON(message: Envelope): unknown;
};
export declare const Signature: {
    fromJSON(object: any): Signature;
    toJSON(message: Signature): unknown;
};
