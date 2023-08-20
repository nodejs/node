/// <reference types="node" />
export declare class StreamError extends Error {
}
export declare class ByteStream {
    private static BLOCK_SIZE;
    private buf;
    private view;
    private start;
    constructor(buffer?: ArrayBuffer);
    get buffer(): Buffer;
    get length(): number;
    get position(): number;
    seek(position: number): void;
    slice(start: number, len: number): Buffer;
    appendChar(char: number): void;
    appendUint16(num: number): void;
    appendUint24(num: number): void;
    appendView(view: Uint8Array): void;
    getBlock(size: number): Buffer;
    getUint8(): number;
    getUint16(): number;
    private ensureCapacity;
    private realloc;
}
