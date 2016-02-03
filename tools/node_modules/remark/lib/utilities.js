/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:utilities
 * @version 3.2.2
 * @fileoverview Collection of tiny helpers useful for
 *   both parsing and compiling markdown.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var collapseWhiteSpace = require('collapse-white-space');

/*
 * Expressions.
 */

var EXPRESSION_LINE_BREAKS = /\r\n|\r/g;
var EXPRESSION_SYMBOL_FOR_NEW_LINE = /\u2424/g;
var EXPRESSION_BOM = /^\ufeff/;

/**
 * Throw an exception with in its `message` `value`
 * and `name`.
 *
 * @param {*} value - Invalid value.
 * @param {string} name - Setting name.
 */
function raise(value, name) {
    throw new Error(
        'Invalid value `' + value + '` ' +
        'for setting `' + name + '`'
    );
}

/**
 * Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean.
 *
 * @example
 *   validateBoolean({foo: null}, 'foo', true) // true
 *   validateBoolean({foo: false}, 'foo', true) // false
 *   validateBoolean({foo: 'bar'}, 'foo', true) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   a boolean.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {boolean} def - Default value.
 */
function validateBoolean(context, name, def) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (typeof value !== 'boolean') {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/**
 * Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean.
 *
 * @example
 *   validateNumber({foo: null}, 'foo', 1) // 1
 *   validateNumber({foo: 2}, 'foo', 1) // 2
 *   validateNumber({foo: 'bar'}, 'foo', 1) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   a number.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {number} def - Default value.
 */
function validateNumber(context, name, def) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (typeof value !== 'number' || value !== value) {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/**
 * Validate a value to be in `map`. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * not in `map`.
 *
 * @example
 *   var map = {bar: true, baz: true};
 *   validateString({foo: null}, 'foo', 'bar', map) // 'bar'
 *   validateString({foo: 'baz'}, 'foo', 'bar', map) // 'baz'
 *   validateString({foo: true}, 'foo', 'bar', map) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   in `map`.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {string} def - Default value.
 * @param {Object} map - Enum.
 */
function validateString(context, name, def, map) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (!(value in map)) {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/**
 * Clean a string in preperation of parsing.
 *
 * @example
 *   clean('\ufefffoo'); // 'foo'
 *   clean('foo\r\nbar'); // 'foo\nbar'
 *   clean('foo\u2424bar'); // 'foo\nbar'
 *
 * @param {string} value - Content to clean.
 * @return {string} - Cleaned content.
 */
function clean(value) {
    return String(value)
        .replace(EXPRESSION_BOM, '')
        .replace(EXPRESSION_LINE_BREAKS, '\n')
        .replace(EXPRESSION_SYMBOL_FOR_NEW_LINE, '\n');
}

/**
 * Normalize an identifier.  Collapses multiple white space
 * characters into a single space, and removes casing.
 *
 * @example
 *   normalizeIdentifier('FOO\t bar'); // 'foo bar'
 *
 * @param {string} value - Content to normalize.
 * @return {string} - Normalized content.
 */
function normalizeIdentifier(value) {
    return collapseWhiteSpace(value).toLowerCase();
}

/**
 * Construct a state `toggler`: a function which inverses
 * `property` in context based on its current value.
 * The by `toggler` returned function restores that value.
 *
 * @example
 *   var context = {};
 *   var key = 'foo';
 *   var val = true;
 *   context[key] = val;
 *   context.enter = stateToggler(key, val);
 *   context[key]; // true
 *   var exit = context.enter();
 *   context[key]; // false
 *   var nested = context.enter();
 *   context[key]; // false
 *   nested();
 *   context[key]; // false
 *   exit();
 *   context[key]; // true
 *
 * @param {string} key - Property to toggle.
 * @param {boolean} state - It's default state.
 * @return {function(): function()} - Enter.
 */
function stateToggler(key, state) {
    /**
     * Construct a toggler for the bound `key`.
     *
     * @return {Function} - Exit state.
     */
    function enter() {
        var self = this;
        var current = self[key];

        self[key] = !state;

        /**
         * State canceler, cancels the state, if allowed.
         */
        function exit() {
            self[key] = current;
        }

        return exit;
    }

    return enter;
}

/*
 * Define nodes of a type which can be merged.
 */

var MERGEABLE_NODES = {};

/**
 * Check whether a node is mergeable with adjacent nodes.
 *
 * @param {Object} node - Node to check.
 * @return {boolean} - Whether `node` is mergable.
 */
function mergeable(node) {
    var start;
    var end;

    if (node.type !== 'text' || !node.position) {
        return true;
    }

    start = node.position.start;
    end = node.position.end;

    /*
     * Only merge nodes which occupy the same size as their
     * `value`.
     */

    return start.line !== end.line ||
        end.column - start.column === node.value.length;
}

/**
 * Merge two text nodes: `node` into `prev`.
 *
 * @param {Object} prev - Preceding sibling.
 * @param {Object} node - Following sibling.
 * @return {Object} - `prev`.
 */
MERGEABLE_NODES.text = function (prev, node) {
    prev.value += node.value;

    return prev;
};

/**
 * Merge two blockquotes: `node` into `prev`, unless in
 * CommonMark mode.
 *
 * @param {Object} prev - Preceding sibling.
 * @param {Object} node - Following sibling.
 * @return {Object} - `prev`, or `node` in CommonMark mode.
 */
MERGEABLE_NODES.blockquote = function (prev, node) {
    if (this.options.commonmark) {
        return node;
    }

    prev.children = prev.children.concat(node.children);

    return prev;
};

/*
 * Expose `validate`.
 */

exports.validate = {
    'boolean': validateBoolean,
    'string': validateString,
    'number': validateNumber
};

/*
 * Expose.
 */

exports.normalizeIdentifier = normalizeIdentifier;
exports.clean = clean;
exports.raise = raise;
exports.stateToggler = stateToggler;
exports.mergeable = mergeable;
exports.MERGEABLE_NODES = MERGEABLE_NODES;
