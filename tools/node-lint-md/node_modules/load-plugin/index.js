/**
 * @typedef ResolveOptions
 * @property {string} [prefix]
 * @property {string|string[]} [cwd]
 * @property {boolean} [global]
 */

/**
 * @typedef {ResolveOptions & {key?: string|false}} LoadOptions
 */

import fs from 'fs'
import {pathToFileURL, fileURLToPath} from 'url'
import path from 'path'
import {resolve as esmResolve} from 'import-meta-resolve'
import libNpmConfig from 'libnpmconfig'

const electron = process.versions.electron !== undefined
const windows = process.platform === 'win32'

const argv = process.argv[1] || /* c8 ignore next */ ''
const nvm = process.env.NVM_BIN
const appData = process.env.APPDATA

/* c8 ignore next */
const globalsLibrary = windows ? '' : 'lib'

/** @type {{prefix?: string}} */
let builtinNpmConfig

// The prefix config defaults to the location where node is installed.
// On Windows, this is in a place called `%AppData%`, which we have to
// pass to `libnpmconfig` explicitly:
/* c8 ignore next 4 */
if (windows && appData) {
  builtinNpmConfig = {prefix: path.join(appData, 'npm')}
}

/**
 * Note: `libnpmconfig` uses `figgy-pudding` which is slated for archival.
 * Either `libnpmconfig` will switch to an alternative or we’ll have to.
 * @type {string}
 */
let npmPrefix = libNpmConfig.read(null, builtinNpmConfig).prefix

// If there is no prefix defined, use the defaults
// See: <https://github.com/eush77/npm-prefix/blob/master/index.js>
/* c8 ignore next 5 */
if (!npmPrefix) {
  npmPrefix = windows
    ? path.dirname(process.execPath)
    : path.resolve(process.execPath, '../..')
}

const globalsDefault = electron || argv.indexOf(npmPrefix) === 0
let globalDir = path.resolve(npmPrefix, globalsLibrary, 'node_modules')

// If we’re in Electron, we’re running in a modified Node that cannot really
// install global node modules.
// To find the actual modules, the user has to set `prefix` somewhere in an
// `.npmrc` (which is picked up by `libnpmconfig`).
// Most people don’t do that, and some use NVM instead to manage different
// versions of Node.
// Luckily NVM leaks some environment variables that we can pick up on to try
// and detect the actual modules.
/* c8 ignore next 3 */
if (electron && nvm && !fs.existsSync(globalDir)) {
  globalDir = path.resolve(nvm, '..', globalsLibrary, 'node_modules')
}

/**
 *  Load the plugin found using `resolvePlugin`.
 *
 * @param {string} name The name to import.
 * @param {LoadOptions} [options]
 * @returns {Promise<unknown>}
 */
export async function loadPlugin(name, options = {}) {
  const {key = 'default', ...rest} = options
  const fp = await resolvePlugin(name, rest)
  /** @type {Object.<string, unknown>} */
  // Bug with coverage on Node@12.
  /* c8 ignore next 3 */
  const mod = await import(pathToFileURL(fp).href)
  return key === false ? mod : mod[key]
}

/**
 * Find a plugin.
 *
 * See also:
 * *   https://docs.npmjs.com/files/folders#node-modules
 * *   https://github.com/sindresorhus/resolve-from
 *
 * Uses the standard node module loading strategy to find `$name` in each given
 * `cwd` (and optionally the global `node_modules` directory).
 *
 * If a prefix is given and `$name` is not a path, `$prefix-$name` is also
 * searched (preferring these over non-prefixed modules).
 *
 * @param {string} name
 * @param {ResolveOptions} [options]
 * @returns {Promise.<string>}
 */
export async function resolvePlugin(name, options = {}) {
  const prefix = options.prefix
    ? options.prefix +
      (options.prefix.charAt(options.prefix.length - 1) === '-' ? '' : '-')
    : undefined
  const cwd = options.cwd
  const globals =
    options.global === undefined || options.global === null
      ? globalsDefault
      : options.global
  const sources = Array.isArray(cwd) ? cwd.concat() : [cwd || process.cwd()]
  /** @type {string} */
  let plugin
  /** @type {Error} */
  let lastError

  // Non-path.
  if (name.charAt(0) !== '.') {
    if (globals) {
      sources.push(globalDir)
    }

    let scope = ''

    // Unprefix module.
    if (prefix) {
      // Scope?
      if (name.charAt(0) === '@') {
        const slash = name.indexOf('/')

        // Let’s keep the algorithm simple.
        // No need to care if this is a “valid” scope (I think?).
        // But we do check for the slash.
        if (slash !== -1) {
          scope = name.slice(0, slash + 1)
          name = name.slice(slash + 1)
        }
      }

      if (name.slice(0, prefix.length) !== prefix) {
        plugin = scope + prefix + name
      }

      name = scope + name
    }
  }

  let index = -1
  /** @type {string} */
  let fp

  while (++index < sources.length) {
    fp = plugin && (await attempt(sources[index], plugin))
    if (fp) return fp

    fp = await attempt(sources[index], name)
    if (fp) return fp
  }

  // There’s always an error.
  // Bug with coverage on Node@12.
  /* c8 ignore next 8 */
  throw lastError

  /**
   * @param {string} base
   * @param {string} name
   * @returns {Promise<string>}
   */
  async function attempt(base, name) {
    try {
      // `import-meta-resolve` resolves from files, whereas `load-plugin` works
      // on folders, which is why we add a `/` at the end.
      return fileURLToPath(
        await esmResolve(name, pathToFileURL(base).href + '/')
      )
      // Bug with coverage on Node@12.
      /* c8 ignore next 1 */
    } catch (error) {
      lastError = error
    }
  }
}
