/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module load-plugin
 * @fileoverview Load a submodule, plugin, or file.
 */

'use strict';

/* Dependencies */
var path = require('path');
var resolve = require('resolve-from');
var npmPrefix = require('npm-prefix')();

/* Expose. */
module.exports = loadPlugin;
loadPlugin.resolve = resolvePlugin;

/* Constants. */
var isElectron = process.versions.electron !== undefined;
var argv = process.argv[1] || /* istanbul ignore next */ '';
var isGlobal = isElectron || argv.indexOf(npmPrefix) === 0;
var isWindows = process.platform === 'win32';
var prefix = isWindows ? /* istanbul ignore next */ '' : 'lib';
var globals = path.resolve(npmPrefix, prefix, 'node_modules');

/**
 * Loads the plug-in found using `resolvePlugin`.
 *
 * @param {string} name - Reference to plugin.
 * @param {Object?} [options] - Configuration,
 * @return {*} - Result of requiring `plugin`.
 */
function loadPlugin(name, options) {
  return require(resolvePlugin(name, options) || name);
}

/**
 * Find a plugin.
 *
 * Uses the standard node module loading strategy to find $name
 * in each given `cwd` (and optionally the global node_modules
 * directory).
 *
 * If a prefix is given and $name is not a path, `$prefix-$name`
 * is also searched (preferring these over non-prefixed modules).
 *
 * @see https://docs.npmjs.com/files/folders#node-modules
 * @see https://github.com/sindresorhus/resolve-from
 *
 * @param {string} name - Reference to plugin.
 * @param {Object?} [options] - Configuration,
 * @return {string?} - Path to `name`, if found.
 */
function resolvePlugin(name, options) {
  var settings = options || {};
  var prefix = settings.prefix;
  var cwd = settings.cwd;
  var filePath;
  var sources;
  var length;
  var index;
  var plugin;

  if (cwd && typeof cwd === 'object') {
    sources = cwd.concat();
  } else {
    sources = [cwd || process.cwd()];
  }

  /* Non-path. */
  if (name.indexOf(path.sep) === -1 && name.charAt(0) !== '.') {
    if (settings.global == null ? isGlobal : settings.global) {
      sources.push(globals);
    }

    /* Unprefix module. */
    if (prefix) {
      prefix = prefix.charAt(prefix.length - 1) === '-' ? prefix : prefix + '-';

      if (name.slice(0, prefix.length) !== prefix) {
        plugin = prefix + name;
      }
    }
  }

  length = sources.length;
  index = -1;

  while (++index < length) {
    cwd = sources[index];
    filePath = (plugin && resolve(cwd, plugin)) || resolve(cwd, name);

    if (filePath) {
      return filePath;
    }
  }

  return null;
}
