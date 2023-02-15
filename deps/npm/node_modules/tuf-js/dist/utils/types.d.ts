export declare enum MetadataKind {
    Root = "root",
    Timestamp = "timestamp",
    Snapshot = "snapshot",
    Targets = "targets"
}
export type JSONObject = {
    [key: string]: JSONValue;
};
export type JSONValue = null | boolean | number | string | JSONValue[] | JSONObject;
