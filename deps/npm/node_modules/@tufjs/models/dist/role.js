"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.SuccinctRoles = exports.DelegatedRole = exports.Role = exports.TOP_LEVEL_ROLE_NAMES = void 0;
const crypto_1 = __importDefault(require("crypto"));
const minimatch_1 = require("minimatch");
const util_1 = __importDefault(require("util"));
const error_1 = require("./error");
const utils_1 = require("./utils");
exports.TOP_LEVEL_ROLE_NAMES = [
    'root',
    'targets',
    'snapshot',
    'timestamp',
];
/**
 * Container that defines which keys are required to sign roles metadata.
 *
 * Role defines how many keys are required to successfully sign the roles
 * metadata, and which keys are accepted.
 */
class Role {
    constructor(options) {
        const { keyIDs, threshold, unrecognizedFields } = options;
        if (hasDuplicates(keyIDs)) {
            throw new error_1.ValueError('duplicate key IDs found');
        }
        if (threshold < 1) {
            throw new error_1.ValueError('threshold must be at least 1');
        }
        this.keyIDs = keyIDs;
        this.threshold = threshold;
        this.unrecognizedFields = unrecognizedFields || {};
    }
    equals(other) {
        if (!(other instanceof Role)) {
            return false;
        }
        return (this.threshold === other.threshold &&
            util_1.default.isDeepStrictEqual(this.keyIDs, other.keyIDs) &&
            util_1.default.isDeepStrictEqual(this.unrecognizedFields, other.unrecognizedFields));
    }
    toJSON() {
        return {
            keyids: this.keyIDs,
            threshold: this.threshold,
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(data) {
        const { keyids, threshold, ...rest } = data;
        if (!utils_1.guard.isStringArray(keyids)) {
            throw new TypeError('keyids must be an array');
        }
        if (typeof threshold !== 'number') {
            throw new TypeError('threshold must be a number');
        }
        return new Role({
            keyIDs: keyids,
            threshold,
            unrecognizedFields: rest,
        });
    }
}
exports.Role = Role;
function hasDuplicates(array) {
    return new Set(array).size !== array.length;
}
/**
 * A container with information about a delegated role.
 *
 * A delegation can happen in two ways:
 *   - ``paths`` is set: delegates targets matching any path pattern in ``paths``
 *   - ``pathHashPrefixes`` is set: delegates targets whose target path hash
 *      starts with any of the prefixes in ``pathHashPrefixes``
 *
 *   ``paths`` and ``pathHashPrefixes`` are mutually exclusive: both cannot be
 *   set, at least one of them must be set.
 */
class DelegatedRole extends Role {
    constructor(opts) {
        super(opts);
        const { name, terminating, paths, pathHashPrefixes } = opts;
        this.name = name;
        this.terminating = terminating;
        if (opts.paths && opts.pathHashPrefixes) {
            throw new error_1.ValueError('paths and pathHashPrefixes are mutually exclusive');
        }
        this.paths = paths;
        this.pathHashPrefixes = pathHashPrefixes;
    }
    equals(other) {
        if (!(other instanceof DelegatedRole)) {
            return false;
        }
        return (super.equals(other) &&
            this.name === other.name &&
            this.terminating === other.terminating &&
            util_1.default.isDeepStrictEqual(this.paths, other.paths) &&
            util_1.default.isDeepStrictEqual(this.pathHashPrefixes, other.pathHashPrefixes));
    }
    isDelegatedPath(targetFilepath) {
        if (this.paths) {
            return this.paths.some((pathPattern) => isTargetInPathPattern(targetFilepath, pathPattern));
        }
        if (this.pathHashPrefixes) {
            const hasher = crypto_1.default.createHash('sha256');
            const pathHash = hasher.update(targetFilepath).digest('hex');
            return this.pathHashPrefixes.some((pathHashPrefix) => pathHash.startsWith(pathHashPrefix));
        }
        return false;
    }
    toJSON() {
        const json = {
            ...super.toJSON(),
            name: this.name,
            terminating: this.terminating,
        };
        if (this.paths) {
            json.paths = this.paths;
        }
        if (this.pathHashPrefixes) {
            json.path_hash_prefixes = this.pathHashPrefixes;
        }
        return json;
    }
    static fromJSON(data) {
        const { keyids, threshold, name, terminating, paths, path_hash_prefixes, ...rest } = data;
        if (!utils_1.guard.isStringArray(keyids)) {
            throw new TypeError('keyids must be an array of strings');
        }
        if (typeof threshold !== 'number') {
            throw new TypeError('threshold must be a number');
        }
        if (typeof name !== 'string') {
            throw new TypeError('name must be a string');
        }
        if (typeof terminating !== 'boolean') {
            throw new TypeError('terminating must be a boolean');
        }
        if (utils_1.guard.isDefined(paths) && !utils_1.guard.isStringArray(paths)) {
            throw new TypeError('paths must be an array of strings');
        }
        if (utils_1.guard.isDefined(path_hash_prefixes) &&
            !utils_1.guard.isStringArray(path_hash_prefixes)) {
            throw new TypeError('path_hash_prefixes must be an array of strings');
        }
        return new DelegatedRole({
            keyIDs: keyids,
            threshold,
            name,
            terminating,
            paths,
            pathHashPrefixes: path_hash_prefixes,
            unrecognizedFields: rest,
        });
    }
}
exports.DelegatedRole = DelegatedRole;
// JS version of Ruby's Array#zip
const zip = (a, b) => a.map((k, i) => [k, b[i]]);
function isTargetInPathPattern(target, pattern) {
    const targetParts = target.split('/');
    const patternParts = pattern.split('/');
    if (patternParts.length != targetParts.length) {
        return false;
    }
    return zip(targetParts, patternParts).every(([targetPart, patternPart]) => (0, minimatch_1.minimatch)(targetPart, patternPart));
}
/**
 * Succinctly defines a hash bin delegation graph.
 *
 * A ``SuccinctRoles`` object describes a delegation graph that covers all
 * targets, distributing them uniformly over the delegated roles (i.e. bins)
 * in the graph.
 *
 * The total number of bins is 2 to the power of the passed ``bit_length``.
 *
 * Bin names are the concatenation of the passed ``name_prefix`` and a
 * zero-padded hex representation of the bin index separated by a hyphen.
 *
 * The passed ``keyids`` and ``threshold`` is used for each bin, and each bin
 * is 'terminating'.
 *
 * For details: https://github.com/theupdateframework/taps/blob/master/tap15.md
 */
class SuccinctRoles extends Role {
    constructor(opts) {
        super(opts);
        const { bitLength, namePrefix } = opts;
        if (bitLength <= 0 || bitLength > 32) {
            throw new error_1.ValueError('bitLength must be between 1 and 32');
        }
        this.bitLength = bitLength;
        this.namePrefix = namePrefix;
        // Calculate the suffix_len value based on the total number of bins in
        // hex. If bit_length = 10 then number_of_bins = 1024 or bin names will
        // have a suffix between "000" and "3ff" in hex and suffix_len will be 3
        // meaning the third bin will have a suffix of "003".
        this.numberOfBins = Math.pow(2, bitLength);
        // suffix_len is calculated based on "number_of_bins - 1" as the name
        // of the last bin contains the number "number_of_bins -1" as a suffix.
        this.suffixLen = (this.numberOfBins - 1).toString(16).length;
    }
    equals(other) {
        if (!(other instanceof SuccinctRoles)) {
            return false;
        }
        return (super.equals(other) &&
            this.bitLength === other.bitLength &&
            this.namePrefix === other.namePrefix);
    }
    /***
     * Calculates the name of the delegated role responsible for 'target_filepath'.
     *
     * The target at path ''target_filepath' is assigned to a bin by casting
     * the left-most 'bit_length' of bits of the file path hash digest to
     * int, using it as bin index between 0 and '2**bit_length - 1'.
     *
     * Args:
     *  target_filepath: URL path to a target file, relative to a base
     *  targets URL.
     */
    getRoleForTarget(targetFilepath) {
        const hasher = crypto_1.default.createHash('sha256');
        const hasherBuffer = hasher.update(targetFilepath).digest();
        // can't ever need more than 4 bytes (32 bits).
        const hashBytes = hasherBuffer.subarray(0, 4);
        // Right shift hash bytes, so that we only have the leftmost
        // bit_length bits that we care about.
        const shiftValue = 32 - this.bitLength;
        const binNumber = hashBytes.readUInt32BE() >>> shiftValue;
        // Add zero padding if necessary and cast to hex the suffix.
        const suffix = binNumber.toString(16).padStart(this.suffixLen, '0');
        return `${this.namePrefix}-${suffix}`;
    }
    *getRoles() {
        for (let i = 0; i < this.numberOfBins; i++) {
            const suffix = i.toString(16).padStart(this.suffixLen, '0');
            yield `${this.namePrefix}-${suffix}`;
        }
    }
    /***
     * Determines whether the given ``role_name`` is in one of
     * the delegated roles that ``SuccinctRoles`` represents.
     *
     * Args:
     *  role_name: The name of the role to check against.
     */
    isDelegatedRole(roleName) {
        const desiredPrefix = this.namePrefix + '-';
        if (!roleName.startsWith(desiredPrefix)) {
            return false;
        }
        const suffix = roleName.slice(desiredPrefix.length, roleName.length);
        if (suffix.length != this.suffixLen) {
            return false;
        }
        // make sure the suffix is a hex string
        if (!suffix.match(/^[0-9a-fA-F]+$/)) {
            return false;
        }
        const num = parseInt(suffix, 16);
        return 0 <= num && num < this.numberOfBins;
    }
    toJSON() {
        const json = {
            ...super.toJSON(),
            bit_length: this.bitLength,
            name_prefix: this.namePrefix,
        };
        return json;
    }
    static fromJSON(data) {
        const { keyids, threshold, bit_length, name_prefix, ...rest } = data;
        if (!utils_1.guard.isStringArray(keyids)) {
            throw new TypeError('keyids must be an array of strings');
        }
        if (typeof threshold !== 'number') {
            throw new TypeError('threshold must be a number');
        }
        if (typeof bit_length !== 'number') {
            throw new TypeError('bit_length must be a number');
        }
        if (typeof name_prefix !== 'string') {
            throw new TypeError('name_prefix must be a string');
        }
        return new SuccinctRoles({
            keyIDs: keyids,
            threshold,
            bitLength: bit_length,
            namePrefix: name_prefix,
            unrecognizedFields: rest,
        });
    }
}
exports.SuccinctRoles = SuccinctRoles;
