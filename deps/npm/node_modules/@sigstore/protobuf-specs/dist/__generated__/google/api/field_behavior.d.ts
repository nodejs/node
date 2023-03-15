/**
 * An indicator of the behavior of a given field (for example, that a field
 * is required in requests, or given as output but ignored as input).
 * This **does not** change the behavior in protocol buffers itself; it only
 * denotes the behavior and may affect how API tooling handles the field.
 *
 * Note: This enum **may** receive new values in the future.
 */
export declare enum FieldBehavior {
    /** FIELD_BEHAVIOR_UNSPECIFIED - Conventional default for enums. Do not use this. */
    FIELD_BEHAVIOR_UNSPECIFIED = 0,
    /**
     * OPTIONAL - Specifically denotes a field as optional.
     * While all fields in protocol buffers are optional, this may be specified
     * for emphasis if appropriate.
     */
    OPTIONAL = 1,
    /**
     * REQUIRED - Denotes a field as required.
     * This indicates that the field **must** be provided as part of the request,
     * and failure to do so will cause an error (usually `INVALID_ARGUMENT`).
     */
    REQUIRED = 2,
    /**
     * OUTPUT_ONLY - Denotes a field as output only.
     * This indicates that the field is provided in responses, but including the
     * field in a request does nothing (the server *must* ignore it and
     * *must not* throw an error as a result of the field's presence).
     */
    OUTPUT_ONLY = 3,
    /**
     * INPUT_ONLY - Denotes a field as input only.
     * This indicates that the field is provided in requests, and the
     * corresponding field is not included in output.
     */
    INPUT_ONLY = 4,
    /**
     * IMMUTABLE - Denotes a field as immutable.
     * This indicates that the field may be set once in a request to create a
     * resource, but may not be changed thereafter.
     */
    IMMUTABLE = 5,
    /**
     * UNORDERED_LIST - Denotes that a (repeated) field is an unordered list.
     * This indicates that the service may provide the elements of the list
     * in any arbitrary order, rather than the order the user originally
     * provided. Additionally, the list's order may or may not be stable.
     */
    UNORDERED_LIST = 6
}
export declare function fieldBehaviorFromJSON(object: any): FieldBehavior;
export declare function fieldBehaviorToJSON(object: FieldBehavior): string;
