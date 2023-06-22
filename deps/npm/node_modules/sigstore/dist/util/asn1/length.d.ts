/// <reference types="node" />
import { ByteStream } from '../stream';
export declare function decodeLength(stream: ByteStream): number;
export declare function encodeLength(len: number): Buffer;
