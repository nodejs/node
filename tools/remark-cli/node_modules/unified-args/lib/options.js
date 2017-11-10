/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-args:options
 * @fileoverview Parse arguments.
 */

'use strict';

/* Dependencies. */
var table = require('text-table');
var camelcase = require('camelcase');
var minimist = require('minimist');
var schema = require('./schema');

/* Expose. */
module.exports = options;

/* Schema for `minimist`. */
var minischema = {
  unknown: handleUnknownArgument,
  default: {},
  alias: {},
  string: [],
  boolean: []
};

schema.forEach(function (option) {
  var value = option.default;

  minischema.default[option.long] = value == null ? null : value;

  if (option.type === 'string') {
    minischema.string.push(option.long);
  }

  if (option.type === 'boolean') {
    minischema.boolean.push(option.long);
  }

  if (option.short) {
    minischema.alias[option.short] = option.long;
  }
});

/**
 * Inspect one `option`.
 *
 * @param {Object} option - Option to inspect.
 * @return {Array.<string>} - Columns of information.
 */
function inspect(option) {
  var description = option.description;
  var long = option.long;

  if (option.default === true || option.truelike) {
    description += ' (on by default)';
    long = '[no-]' + long;
  }

  return [
    '',
    option.short ? '-' + option.short : '',
    '--' + long + (option.value ? ' ' + option.value : ''),
    description
  ];
}

/**
 * Inspect all `options`.
 *
 * @param {Array.<Object>} options - Options to inspect.
 * @return {string} - Formatted table of `options`.
 */
function inspectAll(options) {
  return table(options.map(inspect));
}

/**
 * Handle an unknown flag.
 *
 * @param {string} flag - With prefix.
 * @throws {Error} - If unknown.
 */
function handleUnknownArgument(flag) {
  /* Glob. */
  if (flag.charAt(0) !== '-') {
    return;
  }

  /* Long options. Always unknown. */
  if (flag.charAt(1) === '-') {
    throw new Error(
      'Unknown option `' + flag + '`, ' +
      'expected:\n' +
      inspectAll(schema)
    );
  }

  /* Short options. Can be grouped. */
  flag.slice(1).split('').forEach(function (key) {
    var length = schema.length;
    var index = -1;
    var option;

    while (++index < length) {
      option = schema[index];

      if (option.short === key) {
        return;
      }
    }

    throw new Error(
      'Unknown short option `-' + key + '`, ' +
      'expected:\n' +
      inspectAll(schema.filter(function (option) {
        return option.short;
      }))
    );
  });
}

/**
 * Normalize `value`.
 *
 * @param {*?} value - Optional value or values.
 * @return {Array} - List.
 */
function normalize(value) {
  if (!value) {
    return [];
  }

  if (typeof value === 'string') {
    return [value];
  }

  return flatten(value.map(normalize));
}

/**
 * Flatten `values`.
 *
 * @param {Array} values - List of (lists of) values.
 * @return {Array} - Flattened `values`.
 */
function flatten(values) {
  return [].concat.apply([], values);
}

/**
 * Parse a (lazy?) JSON config.
 *
 * @param {string} value - JSON, but keys do not need to be
 *   quotesd, and surrounding braces are disallowed.
 * @return {Object} - Parsed `value`.
 */
function parseJSON(value) {
  /* Quote unquoted property keys. */
  value = value.replace(/([a-zA-Z0-9\-/]+)(?=\s*:\s*(?:null|true|false|-?\d|"|{|\[))/g, '"$&"');

  return JSON.parse('{' + value + '}');
}

/**
 * Transform the keys on an object to camel-case,
 * recursivly.
 *
 * @param {Object} object - Object to transform.
 * @return {Object} - New object, with camel-case keys.
 */
function toCamelCase(object) {
  var result = {};
  var value;
  var key;

  for (key in object) {
    value = object[key];

    if (value && typeof value === 'object' && !('length' in value)) {
      value = toCamelCase(value);
    }

    result[camelcase(key)] = value;
  }

  return result;
}

/**
 * Parse configuration.
 *
 * @param {string} flags - See `parseJSON`.
 * @param {Object} cache - Store.
 * @return {Object} - `cache`.
 */
function parseConfig(flags, cache) {
  var flag;
  var message;

  try {
    flags = toCamelCase(parseJSON(flags));
  } catch (err) {
    /* Fix position */
    message = err.message.replace(/at(?= position)/, 'around');

    throw new Error('Cannot parse `' + flags + '` as JSON: ' + message);
  }

  for (flag in flags) {
    cache[flag] = flags[flag];
  }

  return cache;
}

/**
 * Parse `extensions`.
 *
 * @param {*} value - Raw extensions.
 * @return {Array.<string>} - Parsed extensions.
 */
function extensions(value) {
  return flatten(normalize(value).map(function (value) {
    return value.split(',');
  }));
}

/**
 * Parse `plugins` or `presets`.
 *
 * @param {*} value - Raw.
 * @return {Object} - Parsed.
 */
function plugins(value) {
  var result = {};

  normalize(value)
    .map(function (value) {
      return value.split('=');
    })
    .forEach(function (value) {
      result[value[0]] = value[1] ? parseConfig(value[1], {}) : null;
    });

  return result;
}

/**
 * Parse `settings`.
 *
 * @param {*} value - Raw settings.
 * @return {Object} - Parsed settings.
 */
function settings(value) {
  var cache = {};

  normalize(value).forEach(function (value) {
    parseConfig(value, cache);
  });

  return cache;
}

/**
 * Parse CLI options.
 *
 * @param {Array.<string>} flags - CLI arguments.
 * @param {Object} configuration - Configuration.
 * @return {Object} - Parsed configuration.
 */
function options(flags, configuration) {
  var extension = configuration.extensions[0];
  var name = configuration.name;
  var config = toCamelCase(minimist(flags, minischema));
  var help;
  var ext;

  schema.forEach(function (option) {
    if (option.type === 'string' && config[option.long] === '') {
      throw new Error('Missing value:' + inspect(option).join(' '));
    }
  });

  ext = extensions(config.ext);

  help = inspectAll(schema) + [
    '',
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
  ].join('\n');

  return {
    helpMessage: help,
    /* “hidden” feature, makes testing easier. */
    cwd: configuration.cwd,
    processor: configuration.processor,
    help: config.help,
    version: config.version,
    files: config._,
    watch: config.watch,
    extensions: ext.length ? ext : configuration.extensions,
    output: config.output,
    out: config.stdout,
    tree: config.tree,
    treeIn: config.treeIn,
    treeOut: config.treeOut,
    rcName: configuration.rcName,
    packageField: configuration.packageField,
    rcPath: config.rcPath,
    detectConfig: config.config,
    settings: settings(config.setting),
    ignoreName: configuration.ignoreName,
    ignorePath: config.ignorePath,
    detectIgnore: config.ignore,
    pluginPrefix: configuration.pluginPrefix,
    plugins: plugins(config.use),
    color: config.color,
    silent: config.silent,
    quiet: config.quiet,
    frail: config.frail
  };
}
