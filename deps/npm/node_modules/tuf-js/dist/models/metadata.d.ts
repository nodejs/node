import { JSONObject, JSONValue, MetadataKind } from '../utils/types';
import { Signable } from './base';
import { Root } from './root';
import { Signature } from './signature';
import { Snapshot } from './snapshot';
import { Targets } from './targets';
import { Timestamp } from './timestamp';
type MetadataType = Root | Timestamp | Snapshot | Targets;
/***
 * A container for signed TUF metadata.
 *
 * Provides methods to convert to and from json, read and write to and
 * from JSON and to create and verify metadata signatures.
 *
 * ``Metadata[T]`` is a generic container type where T can be any one type of
 * [``Root``, ``Timestamp``, ``Snapshot``, ``Targets``]. The purpose of this
 * is to allow static type checking of the signed attribute in code using
 * Metadata::
 *
 * root_md = Metadata[Root].fromJSON("root.json")
 * # root_md type is now Metadata[Root]. This means signed and its
 * # attributes like consistent_snapshot are now statically typed and the
 * # types can be verified by static type checkers and shown by IDEs
 *
 * Using a type constraint is not required but not doing so means T is not a
 * specific type so static typing cannot happen. Note that the type constraint
 * ``[Root]`` is not validated at runtime (as pure annotations are not available
 * then).
 *
 * Apart from ``expires`` all of the arguments to the inner constructors have
 * reasonable default values for new metadata.
 */
export declare class Metadata<T extends MetadataType> implements Signable {
    signed: T;
    signatures: Record<string, Signature>;
    unrecognizedFields: Record<string, JSONValue>;
    constructor(signed: T, signatures?: Record<string, Signature>, unrecognizedFields?: Record<string, JSONValue>);
    verifyDelegate(delegatedRole: string, delegatedMetadata: Metadata<MetadataType>): void;
    equals(other: T): boolean;
    static fromJSON(type: MetadataKind.Root, data: JSONObject): Metadata<Root>;
    static fromJSON(type: MetadataKind.Timestamp, data: JSONObject): Metadata<Timestamp>;
    static fromJSON(type: MetadataKind.Snapshot, data: JSONObject): Metadata<Snapshot>;
    static fromJSON(type: MetadataKind.Targets, data: JSONObject): Metadata<Targets>;
}
export {};
