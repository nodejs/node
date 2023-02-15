"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Snapshot = void 0;
const util_1 = __importDefault(require("util"));
const guard_1 = require("../utils/guard");
const types_1 = require("../utils/types");
const base_1 = require("./base");
const file_1 = require("./file");
/**
 * A container for the signed part of snapshot metadata.
 *
 * Snapshot contains information about all target Metadata files.
 * A top-level role that specifies the latest versions of all targets metadata files,
 * and hence the latest versions of all targets (including any dependencies between them) on the repository.
 */
class Snapshot extends base_1.Signed {
    constructor(opts) {
        super(opts);
        this.type = types_1.MetadataKind.Snapshot;
        this.meta = opts.meta || { 'targets.json': new file_1.MetaFile({ version: 1 }) };
    }
    equals(other) {
        if (!(other instanceof Snapshot)) {
            return false;
        }
        return super.equals(other) && util_1.default.isDeepStrictEqual(this.meta, other.meta);
    }
    toJSON() {
        return {
            meta: metaToJSON(this.meta),
            spec_version: this.specVersion,
            version: this.version,
            expires: this.expires,
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(data) {
        const { unrecognizedFields, ...commonFields } = base_1.Signed.commonFieldsFromJSON(data);
        const { meta, ...rest } = unrecognizedFields;
        return new Snapshot({
            ...commonFields,
            meta: metaFromJSON(meta),
            unrecognizedFields: rest,
        });
    }
}
exports.Snapshot = Snapshot;
function metaToJSON(meta) {
    return Object.entries(meta).reduce((acc, [path, metadata]) => ({
        ...acc,
        [path]: metadata.toJSON(),
    }), {});
}
function metaFromJSON(data) {
    let meta;
    if ((0, guard_1.isDefined)(data)) {
        if (!(0, guard_1.isObjectRecord)(data)) {
            throw new TypeError('meta field is malformed');
        }
        else {
            meta = Object.entries(data).reduce((acc, [path, metadata]) => ({
                ...acc,
                [path]: file_1.MetaFile.fromJSON(metadata),
            }), {});
        }
        return meta;
    }
}
