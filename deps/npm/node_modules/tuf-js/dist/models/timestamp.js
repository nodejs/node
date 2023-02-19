"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Timestamp = void 0;
const guard_1 = require("../utils/guard");
const types_1 = require("../utils/types");
const base_1 = require("./base");
const file_1 = require("./file");
/**
 * A container for the signed part of timestamp metadata.
 *
 * A top-level that specifies the latest version of the snapshot role metadata file,
 * and hence the latest versions of all metadata and targets on the repository.
 */
class Timestamp extends base_1.Signed {
    constructor(options) {
        super(options);
        this.type = types_1.MetadataKind.Timestamp;
        this.snapshotMeta = options.snapshotMeta || new file_1.MetaFile({ version: 1 });
    }
    equals(other) {
        if (!(other instanceof Timestamp)) {
            return false;
        }
        return super.equals(other) && this.snapshotMeta.equals(other.snapshotMeta);
    }
    toJSON() {
        return {
            spec_version: this.specVersion,
            version: this.version,
            expires: this.expires,
            meta: { 'snapshot.json': this.snapshotMeta.toJSON() },
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(data) {
        const { unrecognizedFields, ...commonFields } = base_1.Signed.commonFieldsFromJSON(data);
        const { meta, ...rest } = unrecognizedFields;
        return new Timestamp({
            ...commonFields,
            snapshotMeta: snapshotMetaFromJSON(meta),
            unrecognizedFields: rest,
        });
    }
}
exports.Timestamp = Timestamp;
function snapshotMetaFromJSON(data) {
    let snapshotMeta;
    if ((0, guard_1.isDefined)(data)) {
        const snapshotData = data['snapshot.json'];
        if (!(0, guard_1.isDefined)(snapshotData) || !(0, guard_1.isObject)(snapshotData)) {
            throw new TypeError('missing snapshot.json in meta');
        }
        else {
            snapshotMeta = file_1.MetaFile.fromJSON(snapshotData);
        }
    }
    return snapshotMeta;
}
