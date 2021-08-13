export namespace path {
  export {basename}
  export {dirname}
  export {extname}
  export {join}
  export const sep: string
}
/**
 * @param {string} path
 * @param {string} [ext]
 * @returns {string}
 */
declare function basename(path: string, ext?: string | undefined): string
/**
 * @param {string} path
 * @returns {string}
 */
declare function dirname(path: string): string
/**
 * @param {string} path
 * @returns {string}
 */
declare function extname(path: string): string
/**
 * @param {Array.<string>} segments
 * @returns {string}
 */
declare function join(...segments: Array<string>): string
export {}
