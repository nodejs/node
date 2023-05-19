/// <reference types="node" />
import type { FetchOptions } from '../types/fetch';
export interface TimestampRequest {
    artifactHash: string;
    hashAlgorithm: string;
    certificates?: boolean;
    nonce?: number;
    tsaPolicyOID?: string;
}
export type TimestampAuthorityOptions = {
    baseURL: string;
} & FetchOptions;
export declare class TimestampAuthority {
    private fetch;
    private baseUrl;
    constructor(options: TimestampAuthorityOptions);
    createTimestamp(request: TimestampRequest): Promise<Buffer>;
}
