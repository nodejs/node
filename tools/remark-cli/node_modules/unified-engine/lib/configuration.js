/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:configuration
 * @fileoverview Find rc files.
 */

'use strict';

/* eslint-disable max-params */

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:configuration');
var home = require('user-home');
var yaml = require('js-yaml');
var resolve = require('load-plugin').resolve;
var findUp = require('vfile-find-up');
var json = require('parse-json');

/* Expose. */
module.exports = Configuration;

/* Methods. */
var read = fs.readFileSync;
var exists = fs.existsSync;
var dirname = path.dirname;
var basename = path.basename;
var extname = path.extname;
var concat = [].concat;

/* Constants. */
var PACKAGE_BASENAME = 'package.json';
var SCRIPT_EXTNAME = '.js';
var YAML_EXTNAME = '.yaml';
var PLUGIN_KEY = 'plugins';
var PRESET_KEY = 'presets';

/**
 * Merge two configurations, `configuration` into
 * `target`.
 *
 * @private
 * @param {Object} target - Config to merge into.
 * @param {Object} configuration - Config to merge from.
 * @param {Object} settings - How to merge.
 * @return {Object} - `target`.
 */
function merge(target, configuration, settings, source, options) {
  var pluginOptions = {cwd: settings.cwd, prefix: settings.pluginPrefix};
  var presetOptions = {cwd: settings.cwd, prefix: settings.presetPrefix};
  var key;
  var value;
  var index;
  var length;
  var result;
  var name;
  var plugin;
  var subvalue;

  if (typeof configuration === 'function') {
    configuration = configuration(target, options);
  }

  if (source) {
    /* Do `presets` first. */
    if (configuration[PRESET_KEY]) {
      value = configuration[PRESET_KEY];

      if (typeof value === 'string') {
        value = [value];
      }

      if ('length' in value) {
        length = value.length;
        index = -1;

        while (++index < length) {
          preset(value[index]);
        }
      } else {
        for (name in value) {
          preset(name, value[name]);
        }
      }
    }

    /* Now do plug-ins. */
    if (configuration[PLUGIN_KEY]) {
      value = configuration[PLUGIN_KEY];
      result = target[PLUGIN_KEY];

      if (!result) {
        target[PLUGIN_KEY] = result = {};
      }

      if ('length' in value) {
        index = -1;
        length = value.length;

        while (++index < length) {
          name = value[index];
          plugin = resolve(name, pluginOptions) || name;

          if (!(plugin in result)) {
            log('Configuring plug-in', plugin, {});
            result[plugin] = {};
          }
        }
      } else {
        for (name in value) {
          subvalue = value[name];
          plugin = resolve(name, pluginOptions) || name;

          if (subvalue === false) {
            result[plugin] = false;
            log('Turning off plug-in', plugin);
          } else if (!filled(result[plugin])) {
            result[plugin] = merge({}, subvalue || {}, settings);
            log('Configuring plug-in', plugin, result[plugin]);
          } else if (filled(subvalue)) {
            result[plugin] = merge(result[plugin], subvalue, settings);
            log('Reconfiguring plug-in', plugin, result[plugin]);
          } else {
            log('Not reconfiguring plug-in', plugin);
          }
        }
      }
    }
  }

  /* Walk `configuration`. */
  for (key in configuration) {
    if (source && (key === PRESET_KEY || key === PLUGIN_KEY)) {
      continue;
    }

    value = configuration[key];
    result = target[key];

    if (value && typeof value === 'object') {
      if ('length' in value) {
        target[key] = concat.apply(value);
        log('Setting', key, target[key]);
      } else if (filled(value)) {
        if (result) {
          target[key] = merge(result, value, settings);
          log('Merging', key, target[key]);
        } else {
          target[key] = value;
          log('Setting', key, value);
        }
      }
    } else if (value !== undefined) {
      target[key] = value;
      log('Setting', key, target[key]);
    }
  }

  return target;

  /* Load a preset. */
  function preset(name, value) {
    result = resolve(name, presetOptions) || name;

    log('Loading preset', result, {});
    required(target, result, settings, value);

    debug('Plugins can now load from `%s`', dirname(result));
    settings.cwd.push(dirname(result));
  }

  /**
   * Log a message about setting a value.
   *
   * @param {string} message - Thing done.
   * @param {string} key - Affected key.
   * @param {*} [value] - Set value, if any.
   */
  function log(message, key, value) {
    if (!source) {
      return;
    }

    if (value) {
      debug(message + ' `%s` to `%j` (from `%s`)', key, value, source);
    } else {
      debug(message + ' `%s` (from `%s`)', key, source);
    }
  }
}

/**
 * Parse a JSON configuration object from a file.
 *
 * @throws {Error} - Throws when `filePath` is not found.
 * @param {string} filePath - File location.
 * @return {Object} - Parsed JSON.
 */
function load(filePath, options) {
  var configuration = {};
  var ext = extname(filePath);
  var packageField = options.packageField;
  var configTransform = options.configTransform;
  var doc;

  try {
    if (ext === SCRIPT_EXTNAME) {
      configuration = require(filePath);
    } else {
      doc = read(filePath, 'utf8');

      if (ext === YAML_EXTNAME) {
        configuration = yaml.safeLoad(doc);
      } else {
        configuration = json(doc);

        if (basename(filePath) === PACKAGE_BASENAME) {
          configuration = configuration[packageField] || {};
        }
      }
    }
  } catch (err) {
    err.message = 'Cannot read configuration file: ' +
      filePath + '\n' + err.message;

    throw err;
  }

  if (configTransform) {
    configuration = configTransform(configuration);
  }

  return configuration;
}

/**
 * Get personal configuration object from `~`.
 * Loads `rcName` and `rcName.js`.
 *
 * @param {Object} config - Config to load into.
 * @param {Object} options - Load configuration.
 */
function loadUserConfiguration(config, options) {
  var name = options.rcName;

  /* istanbul ignore next - not really testable. */
  if (home) {
    optional(config, path.join(home, name), options);
    optional(config, path.join(home, [name, SCRIPT_EXTNAME].join('')), options);
    optional(config, path.join(home, [name, YAML_EXTNAME].join('')), options);
  }
}

function optional(config, filePath, settings) {
  /* istanbul ignore if - not really testable
   * as this loads files outside this project. */
  if (exists(filePath)) {
    required(config, filePath, settings);
  }
}

function required(config, filePath, settings, options) {
  merge(config, load(filePath, settings), {
    packageField: settings.packageField,
    pluginPrefix: settings.pluginPrefix,
    presetPrefix: settings.presetPrefix,
    cwd: settings.cwd.concat(dirname(filePath))
  }, filePath, options);
}

/**
 * Get a local configuration object, by walking from
 * `directory` upwards and merging all configurations.
 * If no configuration was found by walking upwards, the
 * current user's config (at `~`) is used.
 *
 * @param {string} directory - Location to search.
 * @param {Object} options - Load options.
 * @param {Function} callback - Invoked with `files`.
 */
function getLocalConfiguration(directory, options, callback) {
  var rcName = options.rcName;
  var packageField = options.packageField;
  var search = [];

  if (rcName) {
    search.push(
      rcName,
      [rcName, SCRIPT_EXTNAME].join(''),
      [rcName, YAML_EXTNAME].join('')
    );

    debug('Looking for `%s` configuration files', search);
  }

  if (packageField) {
    search.push(PACKAGE_BASENAME);
    debug('Looking for `%s` fields in `package.json` files', packageField);
  }

  if (!search.length || !options.detectConfig) {
    debug('Not looking for configuration files');
    return callback(null, {});
  }

  findUp.all(search, directory, function (err, files) {
    var configuration = {};
    var index = files && files.length;

    while (index--) {
      try {
        required(configuration, files[index].path, options);
      } catch (err) {
        return callback(err);
      }
    }

    if (!filled(configuration)) {
      debug('Using personal configuration');

      loadUserConfiguration(configuration, options);
    }

    callback(err, configuration);
  });
}

/**
 * Configuration.
 *
 * @constructor
 * @class Configuration
 * @param {Object} settings - Options to be passed in.
 */
function Configuration(settings) {
  var rcPath = settings.rcPath;

  this.settings = settings;
  this.cache = {};

  if (rcPath) {
    rcPath = path.resolve(settings.cwd, rcPath);
    this.rcPath = rcPath;

    /* Load it so we can fail early (we don’t do anything with
     * the return value though). */
    load(rcPath, settings);
  }
}

Configuration.prototype.getConfiguration = getConfiguration;

/**
 * Build a configuration object.
 *
 * @param {string} filePath - File location.
 * @param {Function} callback - Callback invoked with
 *   configuration.
 */
function getConfiguration(filePath, callback) {
  var self = this;
  var settings = self.settings;
  var directory = dirname(path.resolve(settings.cwd, filePath || 'noop'));
  var configuration = self.cache[directory];
  var options = {
    rcName: settings.rcName,
    configTransform: settings.configTransform,
    packageField: settings.packageField,
    detectConfig: settings.detectConfig,
    pluginPrefix: settings.pluginPrefix,
    presetPrefix: settings.presetPrefix,
    cwd: [settings.cwd]
  };

  debug('Constructing configuration for `' + filePath + '`');

  /**
   * Handle (possible) local config result.
   *
   * @param {Error?} [err] - Loading error.
   * @param {Object?} [localConfiguration] - Configuration.
   */
  function handleLocalConfiguration(config) {
    var rcPath = self.rcPath;

    if (rcPath) {
      debug('Using command line configuration `' + rcPath + '`');
      required(config, rcPath, options);
    }

    merge(config, {
      settings: settings.settings,
      plugins: settings.plugins,
      presets: settings.presets,
      output: settings.output
    }, options, 'settings');

    self.cache[directory] = config;

    return config;
  }

  /* List of callbacks. */
  if (configuration && callbacks(configuration)) {
    configuration.push(callback);
    return;
  }

  if (configuration) {
    /* istanbul ignore else - only occurs if many files are
     * checked, which is hard to reproduce. */
    if (configuration instanceof Error) {
      debug('Previously loaded config gave an error');
      callback(configuration);
    } else {
      debug('Using configuration from cache');
      return callback(null, configuration);
    }
  }

  self.cache[directory] = [callback];

  getLocalConfiguration(directory, options, function (err, localConfiguration) {
    var current = self.cache[directory];
    var exception = err;
    var config;

    if (!exception) {
      try {
        config = handleLocalConfiguration(localConfiguration);
      } catch (err) {
        exception = err;
      }
    }

    if (exception) {
      self.cache[directory] = exception;
    }

    current.forEach(function (callback) {
      callback(exception, exception ? null : config);
    });
  });
}

/**
 * Check if `value` is an object with keys.
 *
 * @param {*} value - Value to check.
 * @return {boolean} - Whether `value` is an object with keys.
 */
function filled(value) {
  return value && typeof value === 'object' && Object.keys(value).length;
}

/**
 * Check if `value` is a list of callbacks.
 *
 * @param {*} value - Value to check.
 * @return {boolean} - Whether `value` is a list of
 *   callbacks.
 */
function callbacks(value) {
  return value && value.length && typeof value[0] === 'function';
}
