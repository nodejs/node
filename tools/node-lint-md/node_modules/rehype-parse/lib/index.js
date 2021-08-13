/**
 * @typedef {import('hast').Root} Root
 * @typedef {Pick<import('hast-util-from-parse5').Options, 'space' | 'verbose'>} FromParse5Options
 *
 * @typedef {keyof errors} ErrorCode
 * @typedef {0|1|2|boolean|null|undefined} ErrorSeverity
 * @typedef {Partial<Record<ErrorCode, ErrorSeverity>>} ErrorFields
 *
 * @typedef ParseFields
 * @property {boolean|undefined} [fragment=false]
 *   Specify whether to parse a fragment, instead of a complete document.
 *   In document mode, unopened `html`, `head`, and `body` elements are opened
 *   in just the right places.
 * @property {boolean|undefined} [emitParseErrors=false]
 *   > ⚠️ Parse errors are currently being added to HTML.
 *   > Not all errors emitted by parse5 (or rehype-parse) are specced yet.
 *   > Some documentation may still be missing.
 *
 *   Emit parse errors while parsing on the vfile.
 *   Setting this to `true` starts emitting HTML parse errors.
 *
 *   Specific rules can be turned off by setting them to `false` (or `0`).
 *   The default, when `emitParseErrors: true`, is `true` (or `1`), and means
 *   that rules emit as warnings.
 *   Rules can also be configured with `2`, to turn them into fatal errors.
 *
 * @typedef {FromParse5Options & ParseFields & ErrorFields} Options
 */

// @ts-expect-error: remove when typed
import Parser5 from 'parse5/lib/parser/index.js'
import {fromParse5} from 'hast-util-from-parse5'
import {errors} from './errors.js'

const base = 'https://html.spec.whatwg.org/multipage/parsing.html#parse-error-'

const fatalities = {2: true, 1: false, 0: null}

/** @type {import('unified').Plugin<[Options?] | void[], string, Root>} */
export default function rehypeParse(options) {
  const processorSettings = /** @type {Options} */ (this.data('settings'))
  const settings = Object.assign({}, options, processorSettings)

  Object.assign(this, {Parser: parser})

  /** @type {import('unified').ParserFunction<Root>} */
  function parser(doc, file) {
    const fn = settings.fragment ? 'parseFragment' : 'parse'
    const onParseError = settings.emitParseErrors ? onerror : null
    const parse5 = new Parser5({
      sourceCodeLocationInfo: true,
      onParseError,
      scriptingEnabled: false
    })

    // @ts-expect-error: `parse5` returns document or fragment, which are always
    // mapped to roots.
    return fromParse5(parse5[fn](doc), {
      space: settings.space,
      file,
      verbose: settings.verbose
    })

    /**
     * @param {{code: string, startLine: number, startCol: number, startOffset: number, endLine: number, endCol: number, endOffset: number}} error
     */
    function onerror(error) {
      const code = error.code
      const name = camelcase(code)
      const setting = settings[name]
      const config = setting === undefined || setting === null ? true : setting
      const level = typeof config === 'number' ? config : config ? 1 : 0
      const start = {
        line: error.startLine,
        column: error.startCol,
        offset: error.startOffset
      }
      const end = {
        line: error.endLine,
        column: error.endCol,
        offset: error.endOffset
      }
      if (level) {
        /* c8 ignore next */
        const info = errors[name] || {reason: '', description: '', url: ''}
        const message = file.message(format(info.reason), {start, end})
        message.source = 'parse-error'
        message.ruleId = code
        message.fatal = fatalities[level]
        message.note = format(info.description)
        message.url = 'url' in info && info.url === false ? null : base + code
      }

      /**
       * @param {string} value
       * @returns {string}
       */
      function format(value) {
        return value
          .replace(/%c(?:-(\d+))?/g, (_, /** @type {string} */ $1) => {
            const offset = $1 ? -Number.parseInt($1, 10) : 0
            const char = doc.charAt(error.startOffset + offset)
            return char === '`' ? '` ` `' : char
          })
          .replace(
            /%x/g,
            () =>
              '0x' +
              doc.charCodeAt(error.startOffset).toString(16).toUpperCase()
          )
      }
    }
  }
}

/**
 * @param {string} value
 * @returns {ErrorCode}
 */
function camelcase(value) {
  // @ts-expect-error: this returns a valid error code.
  return value.replace(/-[a-z]/g, ($0) => $0.charAt(1).toUpperCase())
}
