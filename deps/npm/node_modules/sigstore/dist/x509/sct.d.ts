/// <reference types="node" />
import * as sigstore from '../types/sigstore';
interface SCTOptions {
    version: number;
    logID: Buffer;
    timestamp: Buffer;
    extensions: Buffer;
    hashAlgorithm: number;
    signatureAlgorithm: number;
    signature: Buffer;
}
export declare class SignedCertificateTimestamp {
    readonly version: number;
    readonly logID: Buffer;
    readonly timestamp: Buffer;
    readonly extensions: Buffer;
    readonly hashAlgorithm: number;
    readonly signatureAlgorithm: number;
    readonly signature: Buffer;
    constructor(options: SCTOptions);
    get datetime(): Date;
    get algorithm(): string;
    verify(preCert: Buffer, logs: sigstore.TransparencyLogInstance[]): boolean;
    static parse(buf: Buffer): SignedCertificateTimestamp;
}
export {};
