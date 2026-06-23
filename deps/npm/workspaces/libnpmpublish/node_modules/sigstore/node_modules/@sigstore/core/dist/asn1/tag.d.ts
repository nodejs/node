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
