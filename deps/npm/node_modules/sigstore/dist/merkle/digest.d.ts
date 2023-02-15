/// <reference types="node" />
export declare class Hasher {
    private algorithm;
    constructor(algorithm?: string);
    size(): number;
    hashLeaf(leaf: Buffer): Buffer;
    hashChildren(l: Buffer, r: Buffer): Buffer;
}
