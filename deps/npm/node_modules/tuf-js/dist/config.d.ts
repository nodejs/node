export declare const defaultConfig: {
    maxRootRotations: number;
    maxDelegations: number;
    rootMaxLength: number;
    timestampMaxLength: number;
    snapshotMaxLength: number;
    targetsMaxLength: number;
    prefixTargetsWithHash: boolean;
    fetchTimeout: number;
    fetchRetries: number;
};
export type Config = typeof defaultConfig;
