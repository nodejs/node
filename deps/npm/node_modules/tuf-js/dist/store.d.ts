/// <reference types="node" />
import { Metadata, Root, Snapshot, Targets, Timestamp } from './models';
export declare class TrustedMetadataStore {
    private trustedSet;
    private referenceTime;
    constructor(rootData: Buffer);
    get root(): Metadata<Root>;
    get timestamp(): Metadata<Timestamp> | undefined;
    get snapshot(): Metadata<Snapshot> | undefined;
    get targets(): Metadata<Targets> | undefined;
    getRole(name: string): Metadata<Targets> | undefined;
    updateRoot(bytesBuffer: Buffer): Metadata<Root>;
    updateTimestamp(bytesBuffer: Buffer): Metadata<Timestamp>;
    updateSnapshot(bytesBuffer: Buffer, trusted?: boolean): Metadata<Snapshot>;
    updateDelegatedTargets(bytesBuffer: Buffer, roleName: string, delegatorName: string): void;
    private loadTrustedRoot;
    private checkFinalTimestamp;
    private checkFinalSnapsnot;
}
