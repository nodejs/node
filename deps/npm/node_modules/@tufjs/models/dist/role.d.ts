import { JSONObject, JSONValue } from './utils';
export declare const TOP_LEVEL_ROLE_NAMES: string[];
export interface RoleOptions {
    keyIDs: string[];
    threshold: number;
    unrecognizedFields?: Record<string, JSONValue>;
}
/**
 * Container that defines which keys are required to sign roles metadata.
 *
 * Role defines how many keys are required to successfully sign the roles
 * metadata, and which keys are accepted.
 */
export declare class Role {
    readonly keyIDs: string[];
    readonly threshold: number;
    readonly unrecognizedFields?: Record<string, JSONValue>;
    constructor(options: RoleOptions);
    equals(other: Role): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Role;
}
interface DelegatedRoleOptions extends RoleOptions {
    name: string;
    terminating: boolean;
    paths?: string[];
    pathHashPrefixes?: string[];
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
export declare class DelegatedRole extends Role {
    readonly name: string;
    readonly terminating: boolean;
    readonly paths?: string[];
    readonly pathHashPrefixes?: string[];
    constructor(opts: DelegatedRoleOptions);
    equals(other: DelegatedRole): boolean;
    isDelegatedPath(targetFilepath: string): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): DelegatedRole;
}
interface SuccinctRolesOption extends RoleOptions {
    bitLength: number;
    namePrefix: string;
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
export declare class SuccinctRoles extends Role {
    readonly bitLength: number;
    readonly namePrefix: string;
    readonly numberOfBins: number;
    readonly suffixLen: number;
    constructor(opts: SuccinctRolesOption);
    equals(other: SuccinctRoles): boolean;
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
    getRoleForTarget(targetFilepath: string): string;
    getRoles(): Generator<string>;
    /***
     * Determines whether the given ``role_name`` is in one of
     * the delegated roles that ``SuccinctRoles`` represents.
     *
     * Args:
     *  role_name: The name of the role to check against.
     */
    isDelegatedRole(roleName: string): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): SuccinctRoles;
}
export {};
