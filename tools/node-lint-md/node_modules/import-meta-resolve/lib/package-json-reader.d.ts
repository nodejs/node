export default reader
declare namespace reader {
  export {read}
}
/**
 * @param {string} jsonPath
 * @returns {{string: string}}
 */
declare function read(jsonPath: string): {
  string: string
}
