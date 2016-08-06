/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:lint:library
 * @fileoverview remark plug-in providing warnings when
 *   detecting style violations.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var decamelize = require('decamelize');
var sort = require('vfile-sort');
var control = require('remark-message-control');
var loadPlugin = require('load-plugin');
var trough = require('trough');
var wrapped = require('wrapped');
var internals = require('./rules');

/* Expose. */
module.exports = lint;

/* Constants. */
var SOURCE = 'remark-lint';

/**
 * Lint attacher.
 *
 * All rules are turned off, unless when given
 * a non-nully and non-false value.
 *
 * @example
 *   var processor = lint(remark, {
 *     html: false // Ignore HTML warnings.
 *   });
 *
 * @param {Remark} remark - Host object.
 * @param {Object?} options - Hash of rule names mapping to
 *   rule options.
 */
function lint(remark, options) {
  var settings = decamelizeSettings(options || {});
  var rules = loadExternals(settings.external);
  var enable = [];
  var disable = [];
  var known = [];
  var pipeline = trough();
  var config;
  var id;

  /* Add each rule. */
  for (id in rules) {
    known.push(id);
    config = coerce(id, settings[id]);

    (config[0] ? enable : disable).push(id);

    pipeline.use(ruleFactory(id, rules[id], config[0], config[1]));
  }

  /* Run all rules. */
  remark.use(function () {
    return function (node, file, next) {
      pipeline.run(node, file, next);
    };
  });

  /* Allow comments to toggle messages. */
  remark.use(control, {
    name: 'lint',
    source: SOURCE,
    known: known,
    enable: enable,
    disable: disable
  });

  /**
   * Transformer to sort messages.
   *
   * @param {Node} node - Syntax tree.
   * @param {VFile} file - Virtual file.
   */
  return function (node, file) {
    sort(file);
  };
}

/**
 * Load all externals.  Merges them into a single rule
 * object.
 *
 * In node, accepts externals as strings, otherwise,
 * externals should be a list of objects.
 *
 * @param {Array.<string|Object>} externals - List of
 *   paths to look for externals (only works in Node),
 *   or a list of rule objects.
 * @return {Array.<Object>} - Rule object.
 * @throws {Error} - When an external cannot be resolved.
 */
function loadExternals(externals) {
  var index = -1;
  var rules = {};
  var external;
  var ruleId;
  var mapping = externals ? externals.concat() : [];
  var length;

  mapping.push(internals);
  length = mapping.length;

  while (++index < length) {
    external = mapping[index];

    if (typeof external === 'string') {
      external = loadPlugin(external, {
        prefix: 'remark-lint-'
      });
    }

    for (ruleId in external) {
      rules[ruleId] = external[ruleId];
    }
  }

  return rules;
}

/**
 * Factory to create a plugin from a rule.
 *
 * @example
 *   attachFactory('foo', console.log, false)() // null
 *   attachFactory('foo', console.log, {})() // plugin
 *
 * @param {string} id - Identifier.
 * @param {Function} rule - Rule
 * @param {number} severity - Severity.
 * @param {*} options - Options for respective rule.
 * @return {Function} - Trough ware.
 */
function ruleFactory(id, rule, severity, options) {
  var fn = wrapped(rule);
  var fatal = severity === 2;

  return function (ast, file, next) {
    var scope = file.data;

    /* Track new messages per file. */
    scope.remarkLintIndex = file.messages.length;

    fn(ast, file, options, function (err) {
      var messages = file.messages;
      var message;

      while (scope.remarkLintIndex < messages.length) {
        message = messages[scope.remarkLintIndex];
        message.ruleId = id;
        message.source = SOURCE;
        message.fatal = fatal;

        scope.remarkLintIndex++;
      }

      next(err);
    });
  };
}

/* Ensure ruleId’s are dash-cased. */
function decamelizeSettings(source) {
  var result = {};
  var key;

  for (key in source) {
    result[decamelize(key, '-')] = source[key];
  }

  return result;
}

/* Coerce a value to a severity--options tuple. */
function coerce(name, value) {
  var def = 0;
  var result;

  if (value == null) {
    result = [def];
  } else if (typeof value === 'boolean') {
    result = [value];
  } else if (
    typeof value === 'object' &&
    (typeof value[0] === 'number' || typeof value[0] === 'boolean')
  ) {
    result = value.concat();
  } else {
    result = [1, value];
  }

  if (typeof result[0] === 'boolean') {
    result[0] = result[0] ? 1 : 0;
  }

  if (result[0] < 0 || result[0] > 2) {
    throw new Error(
      'Invalid severity `' + result[0] + '` for `' + name + '`, ' +
      'expected 0, 1, or 2'
    );
  }

  return result;
}
