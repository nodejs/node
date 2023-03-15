import { MetadataKind, Signed, SignedOptions } from './base';
import { MetaFile } from './file';
import { JSONObject } from './utils';
type MetaFileMap = Record<string, MetaFile>;
export interface SnapshotOptions extends SignedOptions {
    meta?: MetaFileMap;
}
/**
 * A container for the signed part of snapshot metadata.
 *
 * Snapshot contains information about all target Metadata files.
 * A top-level role that specifies the latest versions of all targets metadata files,
 * and hence the latest versions of all targets (including any dependencies between them) on the repository.
 */
export declare class Snapshot extends Signed {
    readonly type = MetadataKind.Snapshot;
    readonly meta: MetaFileMap;
    constructor(opts: SnapshotOptions);
    equals(other: Snapshot): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Snapshot;
}
export {};
