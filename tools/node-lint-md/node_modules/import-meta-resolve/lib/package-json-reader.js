// Manually “tree shaken” from:
// <https://github.com/nodejs/node/blob/89f592c/lib/internal/modules/package_json_reader.js>
// Removed the native dependency.
// Also: no need to cache, we do that in resolve already.

import fs from 'fs'
import path from 'path'

const reader = {read}
export default reader

/**
 * @param {string} jsonPath
 * @returns {{string: string}}
 */
function read(jsonPath) {
  return find(path.dirname(jsonPath))
}

/**
 * @param {string} dir
 * @returns {{string: string}}
 */
function find(dir) {
  try {
    const string = fs.readFileSync(
      path.toNamespacedPath(path.join(dir, 'package.json')),
      'utf8'
    )
    return {string}
  } catch (error) {
    if (error.code === 'ENOENT') {
      const parent = path.dirname(dir)
      if (dir !== parent) return find(parent)
      return {string: undefined}
      // Throw all other errors.
      /* c8 ignore next 4 */
    }

    throw error
  }
}
