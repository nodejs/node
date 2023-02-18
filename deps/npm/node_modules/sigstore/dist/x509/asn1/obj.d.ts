/// <reference types="node" />
import { ASN1Tag } from './tag';
export declare class ASN1Obj {
    readonly tag: ASN1Tag;
    readonly subs: ASN1Obj[];
    private buf;
    private headerLength;
    constructor(tag: ASN1Tag, headerLength: number, buf: Buffer, subs: ASN1Obj[]);
    static parseBuffer(buf: Buffer): ASN1Obj;
    get value(): Buffer;
    get raw(): Buffer;
    toDER(): Buffer;
    toBoolean(): boolean;
    toInteger(): bigint;
    toOID(): string;
    toDate(): Date;
    toBitString(): number[];
}
