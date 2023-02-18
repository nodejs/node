import { JSONObject, JSONValue } from '../utils/types';
import { Key } from './key';
import { DelegatedRole, SuccinctRoles } from './role';
type DelegatedRoleMap = Record<string, DelegatedRole>;
type KeyMap = Record<string, Key>;
interface DelegationsOptions {
    keys: KeyMap;
    roles?: DelegatedRoleMap;
    succinctRoles?: SuccinctRoles;
    unrecognizedFields?: Record<string, JSONValue>;
}
/**
 * A container object storing information about all delegations.
 *
 * Targets roles that are trusted to provide signed metadata files
 * describing targets with designated pathnames and/or further delegations.
 */
export declare class Delegations {
    readonly keys: KeyMap;
    readonly roles?: DelegatedRoleMap;
    readonly unrecognizedFields?: Record<string, JSONValue>;
    readonly succinctRoles?: SuccinctRoles;
    constructor(options: DelegationsOptions);
    equals(other: Delegations): boolean;
    rolesForTarget(targetPath: string): Generator<{
        role: string;
        terminating: boolean;
    }>;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Delegations;
}
export {};
