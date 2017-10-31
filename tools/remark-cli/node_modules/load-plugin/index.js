'use strict';

var fs = require('fs');
var path = require('path');
var resolve = require('resolve-from');
var npmPrefix = require('npm-prefix')();

module.exports = loadPlugin;
loadPlugin.resolve = resolvePlugin;

var electron = process.versions.electron !== undefined;
var argv = process.argv[1] || /* istanbul ignore next */ '';
var nvm = process.env.NVM_BIN;
var globally = electron || argv.indexOf(npmPrefix) === 0;
var windows = process.platform === 'win32';
var prefix = windows ? /* istanbul ignore next */ '' : 'lib';
var globals = path.resolve(npmPrefix, prefix, 'node_modules');

/* istanbul ignore next - If we’re in Electron, we’re running in a modified
 * Node that cannot really install global node modules.  To find the actual
 * modules, the user has to either set `prefix` in their `.npmrc` (which is
 * picked up by `npm-prefix`).  Most people don’t do that, and some use NVM
 * instead to manage different versions of Node.  Luckily NVM leaks some
 * environment variables that we can pick up on to try and detect the actual
 * modules. */
if (electron && nvm && !fs.existsSync(globals)) {
  globals = path.resolve(nvm, '..', prefix, 'node_modules');
}

/* Load the plug-in found using `resolvePlugin`. */
function loadPlugin(name, options) {
  return require(resolvePlugin(name, options) || name);
}

/* Find a plugin.
 *
 * See also:
 * <https://docs.npmjs.com/files/folders#node-modules>
 * <https://github.com/sindresorhus/resolve-from>
 *
 * Uses the standard node module loading strategy to find $name
 * in each given `cwd` (and optionally the global node_modules
 * directory).
 *
 * If a prefix is given and $name is not a path, `$prefix-$name`
 * is also searched (preferring these over non-prefixed modules). */
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
    if (settings.global == null ? globally : settings.global) {
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
