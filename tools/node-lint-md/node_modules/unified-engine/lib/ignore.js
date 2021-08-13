/**
 * @typedef {import('ignore').Ignore & {filePath: string}} IgnoreConfig
 *
 * @typedef {'cwd'|'dir'} ResolveFrom
 *
 * @typedef Options
 * @property {string} cwd
 * @property {boolean|undefined} detectIgnore
 * @property {string|undefined} ignoreName
 * @property {string|undefined} ignorePath
 * @property {ResolveFrom|undefined} ignorePathResolveFrom
 *
 * @callback Callback
 * @param {Error|null} error
 * @param {boolean|undefined} [result]
 */

import path from 'node:path'
import ignore from 'ignore'
import {FindUp} from './find-up.js'

export class Ignore {
  /**
   * @param {Options} options
   */
  constructor(options) {
    /** @type {string} */
    this.cwd = options.cwd
    /** @type {ResolveFrom|undefined} */
    this.ignorePathResolveFrom = options.ignorePathResolveFrom

    /** @type {FindUp<IgnoreConfig>} */
    this.findUp = new FindUp({
      cwd: options.cwd,
      filePath: options.ignorePath,
      detect: options.detectIgnore,
      names: options.ignoreName ? [options.ignoreName] : [],
      create
    })
  }

  /**
   * @param {string} filePath
   * @param {Callback} callback
   */
  check(filePath, callback) {
    this.findUp.load(filePath, (error, ignoreSet) => {
      if (error) {
        callback(error)
      } else if (ignoreSet) {
        const normal = path.relative(
          path.resolve(
            this.cwd,
            this.ignorePathResolveFrom === 'cwd' ? '.' : ignoreSet.filePath
          ),
          path.resolve(this.cwd, filePath)
        )

        if (
          normal === '' ||
          normal === '..' ||
          normal.charAt(0) === path.sep ||
          normal.slice(0, 3) === '..' + path.sep
        ) {
          callback(null, false)
        } else {
          callback(null, ignoreSet.ignores(normal))
        }
      } else {
        callback(null, false)
      }
    })
  }
}

/**
 * @param {Buffer} buf
 * @param {string} filePath
 * @returns {IgnoreConfig}
 */
function create(buf, filePath) {
  /** @type {IgnoreConfig} */
  return Object.assign(ignore().add(String(buf)), {
    filePath: path.dirname(filePath)
  })
}
