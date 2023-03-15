import { TargetFile } from '@tufjs/models';
import { Config } from './config';
import { Fetcher } from './fetcher';
export interface UpdaterOptions {
    metadataDir: string;
    metadataBaseUrl: string;
    targetDir?: string;
    targetBaseUrl?: string;
    fetcher?: Fetcher;
    config?: Partial<Config>;
}
export declare class Updater {
    private dir;
    private metadataBaseUrl;
    private targetDir?;
    private targetBaseUrl?;
    private trustedSet;
    private config;
    private fetcher;
    constructor(options: UpdaterOptions);
    refresh(): Promise<void>;
    getTargetInfo(targetPath: string): Promise<TargetFile | undefined>;
    downloadTarget(targetInfo: TargetFile, filePath?: string, targetBaseUrl?: string): Promise<string>;
    findCachedTarget(targetInfo: TargetFile, filePath?: string): Promise<string | undefined>;
    private loadLocalMetadata;
    private loadRoot;
    private loadTimestamp;
    private loadSnapshot;
    private loadTargets;
    private preorderDepthFirstWalk;
    private generateTargetPath;
    private persistMetadata;
}
