/// <reference types="node" />
import { ASN1Tag } from './tag';
export declare class ASN1Obj {
    readonly tag: ASN1Tag;
    readonly subs: ASN1Obj[];
    readonly value: Buffer;
    constructor(tag: ASN1Tag, value: Buffer, subs: ASN1Obj[]);
    static parseBuffer(buf: Buffer): ASN1Obj;
    toDER(): Buffer;
    toBoolean(): boolean;
    toInteger(): bigint;
    toOID(): string;
    toDate(): Date;
    toBitString(): number[];
}
