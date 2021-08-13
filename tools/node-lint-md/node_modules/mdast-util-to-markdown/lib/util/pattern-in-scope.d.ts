/**
 * @typedef {import('../types.js').Unsafe} Unsafe
 */
/**
 * @param {Array.<string>} stack
 * @param {Unsafe} pattern
 * @returns {boolean}
 */
export function patternInScope(stack: Array<string>, pattern: Unsafe): boolean
export type Unsafe = import('../types.js').Unsafe
