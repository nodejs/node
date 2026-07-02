import { ASN1Obj } from '../asn1';
export declare class TSTInfo {
    root: ASN1Obj;
    constructor(asn1: ASN1Obj);
    get version(): bigint;
    get genTime(): Date;
    get messageImprintHashAlgorithm(): string;
    get messageImprintHashedMessage(): Buffer;
    get raw(): Buffer;
    verify(data: Buffer): void;
    private get messageImprintObj();
}
