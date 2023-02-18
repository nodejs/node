import { Updater } from 'tuf-js';
import * as sigstore from '../types/sigstore';
export declare class TrustedRootFetcher {
    private tuf;
    constructor(tuf: Updater);
    getTrustedRoot(): Promise<sigstore.TrustedRoot>;
    private allTargets;
    private getTLogKeys;
    private getCAKeys;
    private readTargetBytes;
}
