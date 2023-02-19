/// <reference types="node" />
/// <reference types="node" />
import { Readable } from 'stream';
import { JSONObject, JSONValue } from '../utils/types';
interface MetaFileOptions {
    version: number;
    length?: number;
    hashes?: Record<string, string>;
    unrecognizedFields?: Record<string, JSONValue>;
}
export declare class MetaFile {
    readonly version: number;
    readonly length?: number;
    readonly hashes?: Record<string, string>;
    readonly unrecognizedFields?: Record<string, JSONValue>;
    constructor(opts: MetaFileOptions);
    equals(other: MetaFile): boolean;
    verify(data: Buffer): void;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): MetaFile;
}
interface TargetFileOptions {
    length: number;
    path: string;
    hashes: Record<string, string>;
    unrecognizedFields?: Record<string, JSONValue>;
}
export declare class TargetFile {
    readonly length: number;
    readonly path: string;
    readonly hashes: Record<string, string>;
    readonly unrecognizedFields: Record<string, JSONValue>;
    constructor(opts: TargetFileOptions);
    get custom(): Record<string, unknown>;
    equals(other: TargetFile): boolean;
    verify(stream: Readable): Promise<void>;
    toJSON(): JSONObject;
    static fromJSON(path: string, data: JSONObject): TargetFile;
}
export {};
