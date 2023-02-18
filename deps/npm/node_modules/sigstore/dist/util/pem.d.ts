/// <reference types="node" />
export declare function split(certificate: string): string[];
export declare function toDER(certificate: string): Buffer;
export declare function fromDER(certificate: Buffer, type?: string): string;
