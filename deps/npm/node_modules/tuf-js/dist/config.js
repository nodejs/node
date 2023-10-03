"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.defaultConfig = void 0;
exports.defaultConfig = {
    maxRootRotations: 32,
    maxDelegations: 32,
    rootMaxLength: 512000,
    timestampMaxLength: 16384,
    snapshotMaxLength: 2000000,
    targetsMaxLength: 5000000,
    prefixTargetsWithHash: true,
    fetchTimeout: 100000,
    fetchRetries: undefined,
    fetchRetry: 2,
};
