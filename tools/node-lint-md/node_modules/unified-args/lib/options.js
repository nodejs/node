/**
 * @typedef {import('unified-engine').Options} EngineOptions
 * @typedef {import('./schema.js').Option} Option
 *
 * @typedef {Required<
 *   Pick<
 *     EngineOptions,
 *     | 'extensions'
 *     | 'ignoreName'
 *     | 'packageField'
 *     | 'pluginPrefix'
 *     | 'processor'
 *     | 'rcName'
 *   >
 * >} RequiredEngineOptions
 *
 * @typedef ArgsOptionsFields
 * @property {string} name
 *   Name of executable
 * @property {string} description
 *   Description of executable
 * @property {string} version
 *   Version (semver) of executable
 *
 * @typedef {RequiredEngineOptions & Pick<EngineOptions, 'cwd'> & ArgsOptionsFields} Options
 */

import table from 'text-table'
import camelcase from 'camelcase'
import minimist from 'minimist'
import json5 from 'json5'
import {fault} from 'fault'
import {schema} from './schema.js'

const own = {}.hasOwnProperty

/**
 * Schema for `minimist`.
 */
const minischema = {
  unknown: handleUnknownArgument,
  /** @type {Record<string, string|boolean|null>} */
  default: {},
  /** @type {Record<string, string>} */
  alias: {},
  /** @type {string[]} */
  string: [],
  /** @type {string[]} */
  boolean: []
}

let index = -1
while (++index < schema.length) {
  addEach(schema[index])
}

/**
 * Parse CLI options.
 *
 * @param {string[]} flags
 * @param {Options} configuration
 */
export function options(flags, configuration) {
  const extension = configuration.extensions[0]
  const name = configuration.name
  const config = toCamelCase(minimist(flags, minischema))
  let index = -1

  while (++index < schema.length) {
    const option = schema[index]
    if (option.type === 'string' && config[option.long] === '') {
      throw fault('Missing value:%s', inspect(option).join(' '))
    }
  }

  const ext = commaSeparated(/** @type {string} */ (config.ext))
  const report = reporter(/** @type {string} */ (config.report))
  const help = [
    inspectAll(schema),
    '',
    'Examples:',
    '',
    '  # Process `input.' + extension + '`',
    '  $ ' + name + ' input.' + extension + ' -o output.' + extension,
    '',
    '  # Pipe',
    '  $ ' + name + ' < input.' + extension + ' > output.' + extension,
    '',
    '  # Rewrite all applicable files',
    '  $ ' + name + ' . -o'
  ].join('\n')

  return {
    helpMessage: help,
    cwd: configuration.cwd,
    processor: configuration.processor,
    help: config.help,
    version: config.version,
    files: config._,
    filePath: config.filePath,
    watch: config.watch,
    extensions: ext.length === 0 ? configuration.extensions : ext,
    output: config.output,
    out: config.stdout,
    tree: config.tree,
    treeIn: config.treeIn,
    treeOut: config.treeOut,
    inspect: config.inspect,
    rcName: configuration.rcName,
    packageField: configuration.packageField,
    rcPath: config.rcPath,
    detectConfig: config.config,
    settings: /** @type {Record<string, unknown>} */ (
      settings(/** @type {string} */ (config.setting))
    ),
    ignoreName: configuration.ignoreName,
    ignorePath: config.ignorePath,
    ignorePathResolveFrom: config.ignorePathResolveFrom,
    ignorePatterns: commaSeparated(
      /** @type {string} */ (config.ignorePattern)
    ),
    silentlyIgnore: config.silentlyIgnore,
    detectIgnore: config.ignore,
    pluginPrefix: configuration.pluginPrefix,
    plugins: plugins(/** @type {string} */ (config.use)),
    reporter: report[0],
    reporterOptions: report[1],
    color: config.color,
    silent: config.silent,
    quiet: config.quiet,
    frail: config.frail
  }
}

/**
 * @param {Option} option
 */
function addEach(option) {
  const value = option.default

  minischema.default[option.long] = value === undefined ? null : value

  if (option.type && option.type in minischema) {
    minischema[option.type].push(option.long)
  }

  if (option.short) {
    minischema.alias[option.short] = option.long
  }
}

/**
 * Parse `extensions`.
 *
 * @param {string[]|string|null|undefined} value
 * @returns {string[]}
 */
function commaSeparated(value) {
  return flatten(normalize(value).map((d) => splitList(d)))
}

/**
 * Parse `plugins`.
 *
 * @param {string[]|string|null|undefined} value
 * @returns {Record<string, Record<string, unknown>|undefined>}
 */
function plugins(value) {
  const normalized = normalize(value).map((d) => splitOptions(d))
  let index = -1
  /** @type {Record<string, Record<string, unknown>|undefined>} */
  const result = {}

  while (++index < normalized.length) {
    const value = normalized[index]
    result[value[0]] = value[1] ? parseConfig(value[1], {}) : undefined
  }

  return result
}

/**
 * Parse `reporter`: only one is accepted.
 *
 * @param {string[]|string|null|undefined} value
 * @returns {[string|undefined, Record<string, unknown>|undefined]}
 */
function reporter(value) {
  const all = normalize(value)
    .map((d) => splitOptions(d))
    .map(
      /**
       * @returns {[string, Record<string, unknown>|undefined]}
       */
      (value) => [value[0], value[1] ? parseConfig(value[1], {}) : undefined]
    )

  return all[all.length - 1] || []
}

/**
 * Parse `settings`.
 *
 * @param {string[]|string|null|undefined} value
 * @returns {Record<string, unknown>}
 */
function settings(value) {
  const normalized = normalize(value)
  let index = -1
  /** @type {Record<string, unknown>} */
  const cache = {}

  while (++index < normalized.length) {
    parseConfig(normalized[index], cache)
  }

  return cache
}

/**
 * Parse configuration.
 *
 * @param {string} value
 * @param {Record<string, unknown>} cache
 * @returns {Record<string, unknown>}
 */
function parseConfig(value, cache) {
  /** @type {Record<string, unknown>} */
  let flags
  /** @type {string} */
  let flag

  try {
    flags = toCamelCase(parseJSON(value))
  } catch (error) {
    throw fault(
      'Cannot parse `%s` as JSON: %s',
      value,
      // Fix position
      error.message.replace(/at(?= position)/, 'around')
    )
  }

  for (flag in flags) {
    if (own.call(flags, flag)) {
      cache[flag] = flags[flag]
    }
  }

  return cache
}

/**
 * Handle an unknown flag.
 *
 * @param {string} flag
 * @returns {boolean}
 */
function handleUnknownArgument(flag) {
  // Not a glob.
  if (flag.charAt(0) === '-') {
    // Long options, always unknown.
    if (flag.charAt(1) === '-') {
      throw fault(
        'Unknown option `%s`, expected:\n%s',
        flag,
        inspectAll(schema)
      )
    }

    // Short options, can be grouped.
    const found = flag.slice(1).split('')
    const known = schema.filter((d) => d.short)
    const knownKeys = new Set(known.map((d) => d.short))
    let index = -1

    while (++index < found.length) {
      const key = found[index]
      if (!knownKeys.has(key)) {
        throw fault(
          'Unknown short option `-%s`, expected:\n%s',
          key,
          inspectAll(known)
        )
      }
    }
  }

  return true
}

/**
 * Inspect all `options`.
 *
 * @param {Option[]} options
 * @returns {string}
 */
function inspectAll(options) {
  return table(options.map((d) => inspect(d)))
}

/**
 * Inspect one `option`.
 *
 * @param {Option} option
 * @returns {string[]}
 */
function inspect(option) {
  let description = option.description
  let long = option.long

  if (option.default === true || option.truelike) {
    description += ' (on by default)'
    long = '[no-]' + long
  }

  return [
    '',
    option.short ? '-' + option.short : '',
    '--' + long + (option.value ? ' ' + option.value : ''),
    description
  ]
}

/**
 * Normalize `value`.
 *
 * @param {string[]|string|null|undefined} value
 * @returns {string[]}
 */
function normalize(value) {
  if (!value) {
    return []
  }

  if (typeof value === 'string') {
    return [value]
  }

  return flatten(value.map((d) => normalize(d)))
}

/**
 * Flatten `values`.
 *
 * @param {string|string[]|string[][]} values
 * @returns {string[]}
 */
function flatten(values) {
  // @ts-expect-error: TS is wrong.
  return values.flat()
}

/**
 * @param {string} value
 * @returns {string[]}
 */
function splitOptions(value) {
  return value.split('=')
}

/**
 * @param {string} value
 * @returns {string[]}
 */
function splitList(value) {
  return value.split(',')
}

/**
 * Transform the keys on an object to camel-case, recursivly.
 *
 * @param {Record<string, unknown>} object
 * @returns {Record<string, unknown>}
 */
function toCamelCase(object) {
  /** @type {Record<string, unknown>} */
  const result = {}
  /** @type {string} */
  let key

  for (key in object) {
    if (own.call(object, key)) {
      let value = object[key]

      if (value && typeof value === 'object' && !Array.isArray(value)) {
        // @ts-expect-error: looks like an object.
        value = toCamelCase(value)
      }

      result[camelcase(key)] = value
    }
  }

  return result
}

/**
 * Parse a (lazy?) JSON config.
 *
 * @param {string} value
 * @returns {Record<string, unknown>}
 */
function parseJSON(value) {
  return json5.parse('{' + value + '}')
}
