/**
 * @typedef {import('micromark-util-types').Options} Options
 * @typedef {import('micromark-util-types').Value} Value
 * @typedef {import('micromark-util-types').Encoding} Encoding
 */

import {compile} from './lib/compile.js'
import {parse} from './lib/parse.js'
import {postprocess} from './lib/postprocess.js'
import {preprocess} from './lib/preprocess.js'

/**
 * @param value Markdown to parse (`string` or `Buffer`).
 * @param [encoding] Character encoding to understand `value` as when it’s a `Buffer` (`string`, default: `'utf8'`).
 * @param [options] Configuration
 */
export const micromark =
  /**
   * @type {(
   *   ((value: Value, encoding: Encoding, options?: Options) => string) &
   *   ((value: Value, options?: Options) => string)
   * )}
   */
  (
    /**
     * @param {Value} value
     * @param {Encoding} [encoding]
     * @param {Options} [options]
     */
    function (value, encoding, options) {
      if (typeof encoding !== 'string') {
        options = encoding
        encoding = undefined
      }

      return compile(options)(
        postprocess(
          parse(options).document().write(preprocess()(value, encoding, true))
        )
      )
    }
  )
