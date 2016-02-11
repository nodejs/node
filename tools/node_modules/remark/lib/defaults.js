/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:defaults
 * @version 3.2.2
 * @fileoverview Default values for parse and
 *  stringification settings.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Note that `stringify.entities` is a string.
 */

module.exports = {
    'parse': {
        'position': true,
        'gfm': true,
        'yaml': true,
        'commonmark': false,
        'footnotes': false,
        'pedantic': false,
        'breaks': false
    },
    'stringify': {
        'gfm': true,
        'commonmark': false,
        'entities': 'false',
        'setext': false,
        'closeAtx': false,
        'looseTable': false,
        'spacedTable': true,
        'incrementListMarker': true,
        'fences': false,
        'fence': '`',
        'bullet': '-',
        'listItemIndent': 'tab',
        'rule': '*',
        'ruleSpaces': true,
        'ruleRepetition': 3,
        'strong': '*',
        'emphasis': '_'
    }
};
