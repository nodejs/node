/**
 * An error thrown when one or more errors have occurred during the
 * `unsubscribe` of a {@link Subscription}.
 */
export declare class UnsubscriptionError extends Error {
    errors: any[];
    constructor(errors: any[]);
}
