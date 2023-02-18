import { JSONObject, MetadataKind } from './types';
export declare function isDefined<T>(val: T | undefined): val is T;
export declare function isObject(value: unknown): value is JSONObject;
export declare function isStringArray(value: unknown): value is string[];
export declare function isObjectArray(value: unknown): value is JSONObject[];
export declare function isStringRecord(value: unknown): value is Record<string, string>;
export declare function isObjectRecord(value: unknown): value is Record<string, JSONObject>;
export declare function isMetadataKind(value: unknown): value is MetadataKind;
