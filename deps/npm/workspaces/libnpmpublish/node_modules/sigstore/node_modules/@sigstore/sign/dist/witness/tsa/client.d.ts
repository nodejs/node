import type { FetchOptions } from '../../types/fetch';
export interface TSA {
    createTimestamp: (signature: Buffer) => Promise<Buffer>;
}
export type TSAClientOptions = {
    tsaBaseURL: string;
} & FetchOptions;
export declare class TSAClient implements TSA {
    private tsa;
    constructor(options: TSAClientOptions);
    createTimestamp(signature: Buffer): Promise<Buffer>;
}
