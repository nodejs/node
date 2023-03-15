import { MetadataKind, Signed, SignedOptions } from './base';
import { Key } from './key';
import { Role } from './role';
import { JSONObject } from './utils';
type KeyMap = Record<string, Key>;
type RoleMap = Record<string, Role>;
export interface RootOptions extends SignedOptions {
    keys?: Record<string, Key>;
    roles?: Record<string, Role>;
    consistentSnapshot?: boolean;
}
/**
 * A container for the signed part of root metadata.
 *
 * The top-level role and metadata file signed by the root keys.
 * This role specifies trusted keys for all other top-level roles, which may further delegate trust.
 */
export declare class Root extends Signed {
    readonly type = MetadataKind.Root;
    readonly keys: KeyMap;
    readonly roles: RoleMap;
    readonly consistentSnapshot: boolean;
    constructor(options: RootOptions);
    addKey(key: Key, role: string): void;
    equals(other: Root): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Root;
}
export {};
