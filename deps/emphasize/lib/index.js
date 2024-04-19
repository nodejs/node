/**
 * @typedef {import('hast').Element} Element
 * @typedef {import('hast').Root} Root
 * @typedef {import('hast').RootData} RootData
 * @typedef {import('hast').Text} Text
 *
 * @typedef {import('lowlight').AutoOptions} LowlightAutoOptions
 * @typedef {import('lowlight').LanguageFn} LanguageFn
 */

/**
 * @typedef AutoFieldsExtra
 *   Extra fields.
 * @property {Sheet | null | undefined} [sheet]
 *   Sheet (optional).
 *
 * @typedef {Pick<LowlightAutoOptions, 'subset'>} AutoFieldsPicked
 *   Picked fields.
 *
 * @typedef {AutoFieldsExtra & AutoFieldsPicked} AutoOptions
 *   Configuration for `highlightAuto`.
 *
 * @typedef Result
 *   Result.
 * @property {string | undefined} language
 *   Detected programming language.
 * @property {number | undefined} relevance
 *   How sure `lowlight` is that the given code is in the language.
 * @property {string} value
 *   Highlighted code.
 *
 * @typedef {Record<string, Style>} Sheet
 *   Map `highlight.js` classes to styles functions.
 *
 *   The `hljs-` prefix must not be used in those classes.
 *   The “descendant selector” (a space) is supported.
 *
 *   For convenience [chalk’s chaining of styles][styles] is suggested.
 *   An abbreviated example is as follows:
 *
 *   ```js
 *   {
 *     'comment': chalk.gray,
 *     'meta meta-string': chalk.cyan,
 *     'meta keyword': chalk.magenta,
 *     'emphasis': chalk.italic,
 *     'strong': chalk.bold,
 *     'formula': chalk.inverse
 *   }
 *   ```
 *
 * @callback Style
 *   Color something.
 * @param {string} value
 *   Input.
 * @returns {string}
 *   Output.
 */

import {Chalk} from 'chalk'
import {createLowlight} from 'lowlight'

const chalk = new Chalk({level: 2})

/**
 * Default style sheet.
 *
 * @type {Readonly<Sheet>}
 */
const defaultSheet = {
  comment: chalk.gray,
  quote: chalk.gray,

  keyword: chalk.green,
  'selector-tag': chalk.green,
  addition: chalk.green,

  number: chalk.cyan,
  string: chalk.cyan,
  'meta meta-string': chalk.cyan,
  literal: chalk.cyan,
  doctag: chalk.cyan,
  regexp: chalk.cyan,

  title: chalk.blue,
  section: chalk.blue,
  name: chalk.blue,
  'selector-id': chalk.blue,
  'selector-class': chalk.blue,

  attribute: chalk.yellow,
  attr: chalk.yellow,
  variable: chalk.yellow,
  'template-variable': chalk.yellow,
  'class title': chalk.yellow,
  type: chalk.yellow,

  symbol: chalk.magenta,
  bullet: chalk.magenta,
  subst: chalk.magenta,
  meta: chalk.magenta,
  'meta keyword': chalk.magenta,
  'selector-attr': chalk.magenta,
  'selector-pseudo': chalk.magenta,
  link: chalk.magenta,

  /* eslint-disable camelcase */
  built_in: chalk.red,
  /* eslint-enable camelcase */
  deletion: chalk.red,

  emphasis: chalk.italic,
  strong: chalk.bold,
  formula: chalk.inverse
}

/**
 * Create an `emphasize` instance.
 *
 * @param {Readonly<Record<string, LanguageFn>> | null | undefined} [grammars]
 *   Grammars to add (optional).
 * @returns
 *   Emphasize.
 */
export function createEmphasize(grammars) {
  const lowlight = createLowlight(grammars)

  return {
    highlight,
    highlightAuto,
    listLanguages: lowlight.listLanguages,
    register: lowlight.register,
    registerAlias: lowlight.registerAlias,
    registered: lowlight.registered
  }

  /**
   * Highlight `value` (code) as `language` (name).
   *
   * @param {string} language
   *   Programming language name.
   * @param {string} value
   *   Code to highlight.
   * @param {Readonly<Sheet> | null | undefined} [sheet]
   *   Style sheet (optional).
   * @returns {Result}
   *   Result.
   */
  function highlight(language, value, sheet) {
    const result = lowlight.highlight(language, value)
    const data = /** @type {RootData} */ (result.data)

    return {
      language: data.language,
      relevance: data.relevance,
      value: visit(sheet || defaultSheet, result)
    }
  }

  /**
   * Highlight `value` (code) and guess its programming language.
   *
   * @param {string} value
   *   Code to highlight.
   * @param {Readonly<AutoOptions> | Readonly<Sheet> | null | undefined} [options]
   *   Configuration or style sheet (optional).
   * @returns {Result}
   *   Result.
   */
  function highlightAuto(value, options) {
    /** @type {Readonly<Sheet> | null | undefined} */
    let sheet
    /** @type {Readonly<LowlightAutoOptions> | undefined} */
    let config

    if (options && ('subset' in options || 'sheet' in options)) {
      const settings = /** @type {Readonly<AutoOptions>} */ (options)
      config = {subset: settings.subset}
      sheet = settings.sheet
    } else {
      sheet = /** @type {Readonly<Sheet> | null | undefined} */ (options)
    }

    const result = lowlight.highlightAuto(value, config)
    const data = /** @type {RootData} */ (result.data)
    return {
      language: data.language,
      relevance: data.relevance,
      value: visit(sheet || defaultSheet, result)
    }
  }
}

/**
 * Visit one `node`.
 *
 * @param {Readonly<Sheet>} sheet
 *   Sheet.
 * @param {Readonly<Element> | Readonly<Root> | Readonly<Text>} node
 *   Node.
 * @returns {string}
 *   Result.
 */
function visit(sheet, node) {
  const names = new Set(
    node.type === 'element' && Array.isArray(node.properties.className)
      ? node.properties.className.map(function (d) {
          return String(d).replace(/^hljs-/, '')
        })
      : []
  )
  /** @type {Sheet} */
  const scoped = {}
  /** @type {Style | undefined} */
  let style
  /** @type {string} */
  let content = ''
  /** @type {string} */
  let key

  for (key in sheet) {
    if (Object.hasOwn(sheet, key)) {
      const parts = key.split(' ')
      const color = sheet[key]

      if (names.has(parts[0])) {
        if (parts.length === 1) {
          style = color
        } else {
          scoped[parts.slice(1).join(' ')] = color
        }
      } else {
        scoped[key] = color
      }
    }
  }

  if ('value' in node) {
    content = node.value
  } else if ('children' in node) {
    content = all(
      scoped,
      /** @type {ReadonlyArray<Element | Text>} */ (node.children)
    )
  }

  if (style) {
    content = style(content)
  }

  return content
}

/**
 * Visit children in `node`.
 *
 * @param {Readonly<Sheet>} sheet
 *   Sheet.
 * @param {ReadonlyArray<Element | Text>} nodes
 *   Nodes.
 * @returns {string}
 *   Result.
 */
function all(sheet, nodes) {
  /** @type {Array<string>} */
  const result = []
  let index = -1

  while (++index < nodes.length) {
    result.push(visit(sheet, nodes[index]))
  }

  return result.join('')
}
