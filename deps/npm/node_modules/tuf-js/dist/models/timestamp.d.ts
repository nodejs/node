import { JSONObject, MetadataKind } from '../utils/types';
import { Signed, SignedOptions } from './base';
import { MetaFile } from './file';
interface TimestampOptions extends SignedOptions {
    snapshotMeta?: MetaFile;
}
/**
 * A container for the signed part of timestamp metadata.
 *
 * A top-level that specifies the latest version of the snapshot role metadata file,
 * and hence the latest versions of all metadata and targets on the repository.
 */
export declare class Timestamp extends Signed {
    readonly type = MetadataKind.Timestamp;
    readonly snapshotMeta: MetaFile;
    constructor(options: TimestampOptions);
    equals(other: Timestamp): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Timestamp;
}
export {};
