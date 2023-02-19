"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Delegations = void 0;
const util_1 = __importDefault(require("util"));
const error_1 = require("../error");
const guard_1 = require("../utils/guard");
const key_1 = require("./key");
const role_1 = require("./role");
/**
 * A container object storing information about all delegations.
 *
 * Targets roles that are trusted to provide signed metadata files
 * describing targets with designated pathnames and/or further delegations.
 */
class Delegations {
    constructor(options) {
        this.keys = options.keys;
        this.unrecognizedFields = options.unrecognizedFields || {};
        if (options.roles) {
            if (Object.keys(options.roles).some((roleName) => role_1.TOP_LEVEL_ROLE_NAMES.includes(roleName))) {
                throw new error_1.ValueError('Delegated role name conflicts with top-level role name');
            }
        }
        this.succinctRoles = options.succinctRoles;
        this.roles = options.roles;
    }
    equals(other) {
        if (!(other instanceof Delegations)) {
            return false;
        }
        return (util_1.default.isDeepStrictEqual(this.keys, other.keys) &&
            util_1.default.isDeepStrictEqual(this.roles, other.roles) &&
            util_1.default.isDeepStrictEqual(this.unrecognizedFields, other.unrecognizedFields) &&
            util_1.default.isDeepStrictEqual(this.succinctRoles, other.succinctRoles));
    }
    *rolesForTarget(targetPath) {
        if (this.roles) {
            for (const role of Object.values(this.roles)) {
                if (role.isDelegatedPath(targetPath)) {
                    yield { role: role.name, terminating: role.terminating };
                }
            }
        }
        else if (this.succinctRoles) {
            yield {
                role: this.succinctRoles.getRoleForTarget(targetPath),
                terminating: true,
            };
        }
    }
    toJSON() {
        const json = {
            keys: keysToJSON(this.keys),
            ...this.unrecognizedFields,
        };
        if (this.roles) {
            json.roles = rolesToJSON(this.roles);
        }
        else if (this.succinctRoles) {
            json.succinct_roles = this.succinctRoles.toJSON();
        }
        return json;
    }
    static fromJSON(data) {
        const { keys, roles, succinct_roles, ...unrecognizedFields } = data;
        let succinctRoles;
        if ((0, guard_1.isObject)(succinct_roles)) {
            succinctRoles = role_1.SuccinctRoles.fromJSON(succinct_roles);
        }
        return new Delegations({
            keys: keysFromJSON(keys),
            roles: rolesFromJSON(roles),
            unrecognizedFields,
            succinctRoles,
        });
    }
}
exports.Delegations = Delegations;
function keysToJSON(keys) {
    return Object.entries(keys).reduce((acc, [keyId, key]) => ({
        ...acc,
        [keyId]: key.toJSON(),
    }), {});
}
function rolesToJSON(roles) {
    return Object.values(roles).map((role) => role.toJSON());
}
function keysFromJSON(data) {
    if (!(0, guard_1.isObjectRecord)(data)) {
        throw new TypeError('keys is malformed');
    }
    return Object.entries(data).reduce((acc, [keyID, keyData]) => ({
        ...acc,
        [keyID]: key_1.Key.fromJSON(keyID, keyData),
    }), {});
}
function rolesFromJSON(data) {
    let roleMap;
    if ((0, guard_1.isDefined)(data)) {
        if (!(0, guard_1.isObjectArray)(data)) {
            throw new TypeError('roles is malformed');
        }
        roleMap = data.reduce((acc, role) => {
            const delegatedRole = role_1.DelegatedRole.fromJSON(role);
            return {
                ...acc,
                [delegatedRole.name]: delegatedRole,
            };
        }, {});
    }
    return roleMap;
}
