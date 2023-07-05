/// <reference types="node" />
export declare function parseInteger(buf: Buffer): bigint;
export declare function parseStringASCII(buf: Buffer): string;
export declare function parseTime(buf: Buffer, shortYear: boolean): Date;
export declare function parseOID(buf: Buffer): string;
export declare function parseBoolean(buf: Buffer): boolean;
export declare function parseBitString(buf: Buffer): number[];
