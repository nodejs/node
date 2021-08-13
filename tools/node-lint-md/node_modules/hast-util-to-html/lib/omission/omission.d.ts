/**
 * Factory to check if a given node can have a tag omitted.
 *
 * @param {Object.<string, OmitHandle>} handlers
 * @returns {OmitHandle}
 */
export function omission(handlers: {[x: string]: OmitHandle}): OmitHandle
export type OmitHandle = import('../types.js').OmitHandle
