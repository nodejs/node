/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('vfile').VFile} VFile
 *
 * @typedef {0|1|2} Severity
 * @typedef {'warn'|'on'|'off'|'error'} Label
 * @typedef {[Severity, ...unknown[]]} SeverityTuple
 *
 * @callback Rule
 * @param {Node} tree
 * @param {VFile} file
 * @param {unknown} options
 * @returns {void}
 */

import {wrap} from 'trough'

const primitives = new Set(['string', 'number', 'boolean'])

/**
 * @param {string} id
 * @param {Rule} rule
 */
export function lintRule(id, rule) {
  const parts = id.split(':')
  // Possibly useful if externalised later.
  /* c8 ignore next */
  const source = parts[1] ? parts[0] : undefined
  const ruleId = parts[1]

  Object.defineProperty(plugin, 'name', {value: id})

  return plugin

  /** @type {import('unified').Plugin<[unknown]|void[]>} */
  function plugin(raw) {
    const [severity, options] = coerce(ruleId, raw)

    if (!severity) return

    const fatal = severity === 2

    return (tree, file, next) => {
      let index = file.messages.length - 1

      wrap(rule, (error) => {
        const messages = file.messages

        // Add the error, if not already properly added.
        // Only happens for incorrect plugins.
        /* c8 ignore next 6 */
        // @ts-expect-error: errors could be `messages`.
        if (error && !messages.includes(error)) {
          try {
            file.fail(error)
          } catch {}
        }

        while (++index < messages.length) {
          Object.assign(messages[index], {ruleId, source, fatal})
        }

        next()
      })(tree, file, options)
    }
  }
}

/**
 * Coerce a value to a severity--options tuple.
 *
 * @param {string} name
 * @param {unknown} value
 * @returns {SeverityTuple}
 */
function coerce(name, value) {
  /** @type {unknown[]} */
  let result

  if (typeof value === 'boolean') {
    result = [value]
  } else if (value === null || value === undefined) {
    result = [1]
  } else if (
    Array.isArray(value) &&
    // `isArray(unknown)` is turned into `any[]`:
    // type-coverage:ignore-next-line
    primitives.has(typeof value[0])
  ) {
    // `isArray(unknown)` is turned into `any[]`:
    // type-coverage:ignore-next-line
    result = [...value]
  } else {
    result = [1, value]
  }

  let level = result[0]

  if (typeof level === 'boolean') {
    level = level ? 1 : 0
  } else if (typeof level === 'string') {
    if (level === 'off') {
      level = 0
    } else if (level === 'on' || level === 'warn') {
      level = 1
    } else if (level === 'error') {
      level = 2
    } else {
      level = 1
      result = [level, result]
    }
  }

  if (typeof level !== 'number' || level < 0 || level > 2) {
    throw new Error(
      'Incorrect severity `' +
        level +
        '` for `' +
        name +
        '`, ' +
        'expected 0, 1, or 2'
    )
  }

  result[0] = level

  // @ts-expect-error: it’s now a valid tuple.
  return result
}
