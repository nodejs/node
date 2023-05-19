import * as sigstore from '../types/sigstore';
import type { FetchOptions } from '../types/fetch';
export type TUFOptions = {
    cachePath?: string;
    mirrorURL?: string;
    rootPath?: string;
} & FetchOptions;
export interface TUF {
    getTarget(targetName: string): Promise<string>;
}
export declare function getTrustedRoot(options?: TUFOptions): Promise<sigstore.TrustedRoot>;
export declare class TUFClient implements TUF {
    private updater;
    constructor(options: TUFOptions);
    refresh(): Promise<void>;
    getTarget(targetName: string): Promise<string>;
}
