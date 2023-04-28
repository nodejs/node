/// <reference types="node" />
import { Hasher } from './digest';
export declare function verifyInclusion(hasher: Hasher, index: bigint, size: bigint, leafHash: Buffer, proof: Buffer[], root: Buffer): boolean;
