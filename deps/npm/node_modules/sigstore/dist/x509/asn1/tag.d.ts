export declare const UNIVERSAL_TAG: {
    BOOLEAN: number;
    INTEGER: number;
    BIT_STRING: number;
    OCTET_STRING: number;
    OBJECT_IDENTIFIER: number;
    SEQUENCE: number;
    SET: number;
    PRINTABLE_STRING: number;
    UTC_TIME: number;
    GENERALIZED_TIME: number;
};
export declare class ASN1Tag {
    readonly number: number;
    readonly constructed: boolean;
    readonly class: number;
    constructor(enc: number);
    isUniversal(): boolean;
    isContextSpecific(num?: number): boolean;
    isBoolean(): boolean;
    isInteger(): boolean;
    isBitString(): boolean;
    isOctetString(): boolean;
    isOID(): boolean;
    isUTCTime(): boolean;
    isGeneralizedTime(): boolean;
    toDER(): number;
}
