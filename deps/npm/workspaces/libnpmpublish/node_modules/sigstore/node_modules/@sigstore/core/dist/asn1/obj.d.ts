import { ASN1Tag } from './tag';
export declare class ASN1Obj {
    readonly tag: ASN1Tag;
    readonly subs: ASN1Obj[];
    readonly value: Buffer<ArrayBufferLike>;
    constructor(tag: ASN1Tag, value: Buffer<ArrayBufferLike>, subs: ASN1Obj[]);
    static parseBuffer(buf: Buffer<ArrayBuffer>): ASN1Obj;
    toDER(): Buffer;
    toBoolean(): boolean;
    toInteger(): bigint;
    toOID(): string;
    toDate(): Date;
    toBitString(): number[];
}
