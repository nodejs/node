"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Root = void 0;
const util_1 = __importDefault(require("util"));
const base_1 = require("./base");
const error_1 = require("./error");
const key_1 = require("./key");
const role_1 = require("./role");
const utils_1 = require("./utils");
/**
 * A container for the signed part of root metadata.
 *
 * The top-level role and metadata file signed by the root keys.
 * This role specifies trusted keys for all other top-level roles, which may further delegate trust.
 */
class Root extends base_1.Signed {
    constructor(options) {
        super(options);
        this.type = base_1.MetadataKind.Root;
        this.keys = options.keys || {};
        this.consistentSnapshot = options.consistentSnapshot ?? true;
        if (!options.roles) {
            this.roles = role_1.TOP_LEVEL_ROLE_NAMES.reduce((acc, role) => ({
                ...acc,
                [role]: new role_1.Role({ keyIDs: [], threshold: 1 }),
            }), {});
        }
        else {
            const roleNames = new Set(Object.keys(options.roles));
            if (!role_1.TOP_LEVEL_ROLE_NAMES.every((role) => roleNames.has(role))) {
                throw new error_1.ValueError('missing top-level role');
            }
            this.roles = options.roles;
        }
    }
    addKey(key, role) {
        if (!this.roles[role]) {
            throw new error_1.ValueError(`role ${role} does not exist`);
        }
        if (!this.roles[role].keyIDs.includes(key.keyID)) {
            this.roles[role].keyIDs.push(key.keyID);
        }
        this.keys[key.keyID] = key;
    }
    equals(other) {
        if (!(other instanceof Root)) {
            return false;
        }
        return (super.equals(other) &&
            this.consistentSnapshot === other.consistentSnapshot &&
            util_1.default.isDeepStrictEqual(this.keys, other.keys) &&
            util_1.default.isDeepStrictEqual(this.roles, other.roles));
    }
    toJSON() {
        return {
            _type: this.type,
            spec_version: this.specVersion,
            version: this.version,
            expires: this.expires,
            keys: keysToJSON(this.keys),
            roles: rolesToJSON(this.roles),
            consistent_snapshot: this.consistentSnapshot,
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(data) {
        const { unrecognizedFields, ...commonFields } = base_1.Signed.commonFieldsFromJSON(data);
        const { keys, roles, consistent_snapshot, ...rest } = unrecognizedFields;
        if (typeof consistent_snapshot !== 'boolean') {
            throw new TypeError('consistent_snapshot must be a boolean');
        }
        return new Root({
            ...commonFields,
            keys: keysFromJSON(keys),
            roles: rolesFromJSON(roles),
            consistentSnapshot: consistent_snapshot,
            unrecognizedFields: rest,
        });
    }
}
exports.Root = Root;
function keysToJSON(keys) {
    return Object.entries(keys).reduce((acc, [keyID, key]) => ({ ...acc, [keyID]: key.toJSON() }), {});
}
function rolesToJSON(roles) {
    return Object.entries(roles).reduce((acc, [roleName, role]) => ({ ...acc, [roleName]: role.toJSON() }), {});
}
function keysFromJSON(data) {
    let keys;
    if (utils_1.guard.isDefined(data)) {
        if (!utils_1.guard.isObjectRecord(data)) {
            throw new TypeError('keys must be an object');
        }
        keys = Object.entries(data).reduce((acc, [keyID, keyData]) => ({
            ...acc,
            [keyID]: key_1.Key.fromJSON(keyID, keyData),
        }), {});
    }
    return keys;
}
function rolesFromJSON(data) {
    let roles;
    if (utils_1.guard.isDefined(data)) {
        if (!utils_1.guard.isObjectRecord(data)) {
            throw new TypeError('roles must be an object');
        }
        roles = Object.entries(data).reduce((acc, [roleName, roleData]) => ({
            ...acc,
            [roleName]: role_1.Role.fromJSON(roleData),
        }), {});
    }
    return roles;
}
