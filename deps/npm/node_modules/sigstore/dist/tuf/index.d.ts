import * as sigstore from '../types/sigstore';
export interface TUFOptions {
    mirrorURL?: string;
    rootPath?: string;
}
export declare function getTrustedRoot(cachePath: string, options?: TUFOptions): Promise<sigstore.TrustedRoot>;
