"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Targets = void 0;
const util_1 = __importDefault(require("util"));
const base_1 = require("./base");
const delegations_1 = require("./delegations");
const file_1 = require("./file");
const utils_1 = require("./utils");
// Container for the signed part of targets metadata.
//
// Targets contains verifying information about target files and also delegates
// responsible to other Targets roles.
class Targets extends base_1.Signed {
    type = base_1.MetadataKind.Targets;
    targets;
    delegations;
    constructor(options) {
        super(options);
        this.targets = options.targets || {};
        this.delegations = options.delegations;
    }
    addTarget(target) {
        this.targets[target.path] = target;
    }
    equals(other) {
        if (!(other instanceof Targets)) {
            return false;
        }
        return (super.equals(other) &&
            util_1.default.isDeepStrictEqual(this.targets, other.targets) &&
            util_1.default.isDeepStrictEqual(this.delegations, other.delegations));
    }
    toJSON() {
        const json = {
            _type: this.type,
            spec_version: this.specVersion,
            version: this.version,
            expires: this.expires,
            targets: targetsToJSON(this.targets),
            ...this.unrecognizedFields,
        };
        if (this.delegations) {
            json.delegations = this.delegations.toJSON();
        }
        return json;
    }
    static fromJSON(data) {
        const { unrecognizedFields, ...commonFields } = base_1.Signed.commonFieldsFromJSON(data);
        const { targets, delegations, ...rest } = unrecognizedFields;
        return new Targets({
            ...commonFields,
            targets: targetsFromJSON(targets),
            delegations: delegationsFromJSON(delegations),
            unrecognizedFields: rest,
        });
    }
}
exports.Targets = Targets;
function targetsToJSON(targets) {
    return Object.entries(targets).reduce((acc, [path, target]) => ({
        ...acc,
        [path]: target.toJSON(),
    }), {});
}
function targetsFromJSON(data) {
    let targets;
    if (utils_1.guard.isDefined(data)) {
        if (!utils_1.guard.isObjectRecord(data)) {
            throw new TypeError('targets must be an object');
        }
        else {
            targets = Object.entries(data).reduce((acc, [path, target]) => ({
                ...acc,
                [path]: file_1.TargetFile.fromJSON(path, target),
            }), {});
        }
    }
    return targets;
}
function delegationsFromJSON(data) {
    let delegations;
    if (utils_1.guard.isDefined(data)) {
        if (!utils_1.guard.isObject(data)) {
            throw new TypeError('delegations must be an object');
        }
        else {
            delegations = delegations_1.Delegations.fromJSON(data);
        }
    }
    return delegations;
}
