/**
 * @typedef {import('./types.js').Options} Options
 * @typedef {import('./types.js').Context} Context
 */
/**
 * @param {Context} base
 * @param {Options} extension
 * @returns {Context}
 */
export function configure(base: Context, extension: Options): Context
export type Options = import('./types.js').Options
export type Context = import('./types.js').Context
