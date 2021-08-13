/**
 * @typedef {import('unified').Plugin<unknown[]>} Plugin
 * @typedef {import('unified').PluginTuple<unknown[]>} PluginTuple
 * @typedef {import('unified').PluggableList} PluggableList
 *
 * @typedef {Record<string, unknown>} Settings
 *
 * @typedef {Record<string, Settings|null|undefined>} PluginIdObject
 * @typedef {Array<string|[string, ...unknown[]]>} PluginIdList
 *
 * @typedef Preset
 * @property {Settings} [settings]
 * @property {PluggableList|PluginIdObject|PluginIdList|undefined} [plugins]
 *
 * @typedef Config
 * @property {Settings} [settings]
 * @property {Array<PluginTuple>} [plugins]
 *
 * @callback ConfigTransform
 * @param {any} config
 * @param {string} filePath
 * @returns {Preset}
 *
 * @callback Loader
 * @param {Buffer} buf
 * @param {string} filePath
 * @returns {Promise<Preset|undefined>}
 *
 * @callback Callback
 * @param {Error|null} error
 * @param {Config} [result]
 * @returns {void}
 */

import path from 'node:path'
import {pathToFileURL} from 'node:url'
import jsYaml from 'js-yaml'
import parseJson from 'parse-json'
import createDebug from 'debug'
import {resolvePlugin} from 'load-plugin'
import isPlainObj from 'is-plain-obj'
import {fault} from 'fault'
import {FindUp} from './find-up.js'

const debug = createDebug('unified-engine:configuration')

const own = {}.hasOwnProperty

/** @type {Record<string, Loader>} */
const loaders = {
  '.json': loadJson,
  '.cjs': loadScriptOrModule,
  '.mjs': loadScriptOrModule,
  '.js': loadScriptOrModule,
  '.yaml': loadYaml,
  '.yml': loadYaml
}

const defaultLoader = loadJson

/**
 * @typedef Options
 * @property {string} cwd
 * @property {string} [packageField]
 * @property {string} [pluginPrefix]
 * @property {string} [rcName]
 * @property {string} [rcPath]
 * @property {boolean} [detectConfig]
 * @property {ConfigTransform} [configTransform]
 * @property {Preset} [defaultConfig]
 * @property {Preset['settings']} [settings]
 * @property {Preset['plugins']} [plugins]
 */

export class Configuration {
  /**
   * @param {Options} options
   */
  constructor(options) {
    /** @type {string[]} */
    const names = []

    /** @type {string} */
    this.cwd = options.cwd
    /** @type {string|undefined} */
    this.packageField = options.packageField
    /** @type {string|undefined} */
    this.pluginPrefix = options.pluginPrefix
    /** @type {ConfigTransform|undefined} */
    this.configTransform = options.configTransform
    /** @type {Preset|undefined} */
    this.defaultConfig = options.defaultConfig

    if (options.rcName) {
      names.push(
        options.rcName,
        options.rcName + '.js',
        options.rcName + '.yml',
        options.rcName + '.yaml'
      )
      debug('Looking for `%s` configuration files', names)
    }

    if (options.packageField) {
      names.push('package.json')
      debug(
        'Looking for `%s` fields in `package.json` files',
        options.packageField
      )
    }

    /** @type {Preset} */
    this.given = {settings: options.settings, plugins: options.plugins}
    this.create = this.create.bind(this)

    /** @type {FindUp<Config>} */
    this.findUp = new FindUp({
      cwd: options.cwd,
      filePath: options.rcPath,
      detect: options.detectConfig,
      names,
      create: this.create
    })
  }

  /**
   * @param {string} filePath
   * @param {Callback} callback
   * @returns {void}
   */
  load(filePath, callback) {
    this.findUp.load(
      filePath || path.resolve(this.cwd, 'stdin.js'),
      (error, file) => {
        if (error || file) {
          return callback(error, file)
        }

        this.create(undefined, undefined).then((result) => {
          callback(null, result)
        }, callback)
      }
    )
  }

  /**
   * @param {Buffer|undefined} buf
   * @param {string|undefined} filePath
   * @returns {Promise<Config|undefined>}
   */
  async create(buf, filePath) {
    const options = {prefix: this.pluginPrefix, cwd: this.cwd}
    const result = {settings: {}, plugins: []}
    const extname = filePath ? path.extname(filePath) : undefined
    const loader =
      extname && extname in loaders ? loaders[extname] : defaultLoader
    /** @type {Preset|undefined} */
    let value

    if (filePath && buf) {
      value = await loader.call(this, buf, filePath)

      if (this.configTransform && value !== undefined) {
        value = this.configTransform(value, filePath)
      }
    }

    // Exit if we did find a `package.json`, but it does not have configuration.
    if (
      filePath &&
      value === undefined &&
      path.basename(filePath) === 'package.json'
    ) {
      return
    }

    if (value === undefined) {
      if (this.defaultConfig) {
        await merge(
          result,
          this.defaultConfig,
          Object.assign({}, options, {root: this.cwd})
        )
      }
    } else {
      await merge(
        result,
        value,
        // @ts-expect-error: `value` can only exist if w/ `filePath`.
        Object.assign({}, options, {root: path.dirname(filePath)})
      )
    }

    await merge(
      result,
      this.given,
      Object.assign({}, options, {root: this.cwd})
    )

    // C8 bug on Node@12
    /* c8 ignore next 2 */
    return result
  }
}

/** @type {Loader} */
async function loadScriptOrModule(_, filePath) {
  // C8 bug on Node@12
  /* c8 ignore next 4 */
  // @ts-expect-error: Assume it matches config.
  // type-coverage:ignore-next-line
  return loadFromAbsolutePath(filePath, this.cwd)
}

/** @type {Loader} */
async function loadYaml(buf, filePath) {
  // C8 bug on Node@12
  /* c8 ignore next 4 */
  // @ts-expect-error: Assume it matches config.
  return jsYaml.load(String(buf), {filename: path.basename(filePath)})
}

/** @type {Loader} */
async function loadJson(buf, filePath) {
  /** @type {Record<string, unknown>} */
  const result = parseJson(String(buf), filePath)

  // C8 bug on Node@12
  /* c8 ignore next 8 */
  // @ts-expect-error: Assume it matches config.
  return path.basename(filePath) === 'package.json'
    ? // @ts-expect-error: `this` is the configuration context, TS doesn’t like
      // `this` on callbacks.
      // type-coverage:ignore-next-line
      result[this.packageField]
    : result
}

/**
 * @param {Required<Config>} target
 * @param {Preset} raw
 * @param {{root: string, prefix: string|undefined}} options
 * @returns {Promise<Config>}
 */
async function merge(target, raw, options) {
  if (typeof raw === 'object' && raw !== null) {
    await addPreset(raw)
  } else {
    throw new Error('Expected preset, not `' + raw + '`')
  }

  // C8 bug on Node@12
  /* c8 ignore next 6 */
  return target

  /**
   * @param {Preset} result
   */
  async function addPreset(result) {
    const plugins = result.plugins

    if (plugins === null || plugins === undefined) {
      // Empty.
    } else if (typeof plugins === 'object' && plugins !== null) {
      await (Array.isArray(plugins) ? addEach(plugins) : addIn(plugins))
    } else {
      throw new Error(
        'Expected a list or object of plugins, not `' + plugins + '`'
      )
    }

    target.settings = Object.assign({}, target.settings, result.settings)
    // C8 bug on Node@12
    /* c8 ignore next 6 */
  }

  /**
   * @param {PluginIdList|PluggableList} result
   */
  async function addEach(result) {
    let index = -1

    while (++index < result.length) {
      const value = result[index]

      // Keep order sequential instead of parallel.
      /* eslint-disable no-await-in-loop */
      // @ts-expect-error: Spreading is fine.
      // type-coverage:ignore-next-line
      await (Array.isArray(value) ? use(...value) : use(value, undefined))
      /* eslint-enable no-await-in-loop */
    }
    // C8 bug on Node@12
    /* c8 ignore next 6 */
  }

  /**
   * @param {PluginIdObject} result
   */
  async function addIn(result) {
    /** @type {string} */
    let key

    for (key in result) {
      if (own.call(result, key)) {
        // Keep order sequential instead of parallel.
        // eslint-disable-next-line no-await-in-loop
        await use(key, result[key])
      }
    }
    // C8 bug on Node@12
    /* c8 ignore next 7 */
  }

  /**
   * @param {string|Plugin|Preset} usable
   * @param {Settings|null|undefined} value
   */
  async function use(usable, value) {
    if (typeof usable === 'string') {
      await addModule(usable, value)
    } else if (typeof usable === 'function') {
      addPlugin(usable, value)
    } else {
      await merge(target, usable, options)
    }
    // C8 bug on Node@12
    /* c8 ignore next 7 */
  }

  /**
   * @param {string} id
   * @param {Settings|null|undefined} value
   */
  async function addModule(id, value) {
    /** @type {string} */
    let fp

    try {
      fp = await resolvePlugin(id, {
        cwd: options.root,
        prefix: options.prefix
      })
    } catch (error) {
      addPlugin(() => {
        throw fault('Could not find module `%s`\n%s', id, error.stack)
      }, value)
      return
    }

    const result = await loadFromAbsolutePath(fp, options.root)

    try {
      if (typeof result === 'function') {
        addPlugin(result, value)
      } else {
        await merge(
          target,
          result,
          Object.assign({}, options, {root: path.dirname(fp)})
        )
      }
    } catch {
      throw fault(
        'Error: Expected preset or plugin, not %s, at `%s`',
        result,
        path.relative(options.root, fp)
      )
    }
    // C8 bug on Node@12
    /* c8 ignore next 8 */
  }

  /**
   * @param {Plugin} plugin
   * @param {Settings|null|undefined} value
   * @returns {void}
   */
  function addPlugin(plugin, value) {
    const entry = find(target.plugins, plugin)

    if (value === null) {
      value = undefined
    }

    if (entry) {
      reconfigure(entry, value)
    } else {
      target.plugins.push([plugin, value])
    }
  }
}

/**
 * @param {PluginTuple} entry
 * @param {Settings|undefined} value
 * @returns {void}
 */
function reconfigure(entry, value) {
  if (isPlainObj(entry[1]) && isPlainObj(value)) {
    value = Object.assign({}, entry[1], value)
  }

  entry[1] = value
}

/**
 * @param {Array<PluginTuple>} entries
 * @param {Plugin} plugin
 * @returns {PluginTuple|undefined}
 */
function find(entries, plugin) {
  let index = -1

  while (++index < entries.length) {
    const entry = entries[index]
    if (entry[0] === plugin) {
      return entry
    }
  }
}

/**
 * @param {string} fp
 * @param {string} base
 * @returns {Promise<Plugin|Preset>}
 */
async function loadFromAbsolutePath(fp, base) {
  try {
    /** @type {{default?: unknown}} */
    const result = await import(pathToFileURL(fp).href)

    if (!('default' in result)) {
      throw new Error(
        'Expected a plugin or preset exported as the default export'
      )
    }

    // @ts-expect-error: assume plugin/preset.
    return result.default
    // C8 bug on Node@12
    /* c8 ignore next 4 */
  } catch (error) {
    throw fault('Cannot import `%s`\n%s', path.relative(base, fp), error.stack)
  }
}
