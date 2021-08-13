// Manually “tree shaken” from:
// <https://github.com/nodejs/node/blob/89f592c/lib/internal/modules/esm/get_format.js>
import path from 'path'
import {URL, fileURLToPath} from 'url'
import {getPackageType} from './resolve.js'
import {codes} from './errors.js'

const {ERR_UNKNOWN_FILE_EXTENSION} = codes

const extensionFormatMap = {
  __proto__: null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module'
}

/**
 * @param {string} url
 * @returns {{format: string|null}}
 */
export function defaultGetFormat(url) {
  if (url.startsWith('node:')) {
    return {format: 'builtin'}
  }

  const parsed = new URL(url)

  if (parsed.protocol === 'data:') {
    const {1: mime} = /^([^/]+\/[^;,]+)[^,]*?(;base64)?,/.exec(
      parsed.pathname
    ) || [null, null]
    const format = mime === 'text/javascript' ? 'module' : null
    return {format}
  }

  if (parsed.protocol === 'file:') {
    const ext = path.extname(parsed.pathname)
    /** @type {string} */
    let format
    if (ext === '.js') {
      format = getPackageType(parsed.href) === 'module' ? 'module' : 'commonjs'
    } else {
      format = extensionFormatMap[ext]
    }

    if (!format) {
      throw new ERR_UNKNOWN_FILE_EXTENSION(ext, fileURLToPath(url))
    }

    return {format: format || null}
  }

  return {format: null}
}
