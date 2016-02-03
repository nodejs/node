/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-pipeline:stringify
 * @version 3.2.2
 * @fileoverview Compile an AST into a file.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var debug = require('debug')('remark:cli:file-pipeline:stringify');

/**
 * Stringify an AST.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *   var file = new File({
 *     'contents': '# Hello'
 *   });
 *   fileSet.add(file);
 *
 *   var context = {'processor': remark(), 'file': file, 'fileSet': fileSet};
 *
 *   parse(context);
 *   stringify(context);
 *
 * @param {Object} context - Context object.
 */
function stringify(context) {
    var cli = context.fileSet.cli;
    var file = context.file;
    var value;

    if (!context.processor || context.ast) {
        debug('Not compiling failed document');
        return;
    }

    debug('Compiling document');

    if (cli.ast) {
        file.move({
            'extension': 'json'
        });

        value = JSON.stringify(file.namespace('mdast').tree, null, 2);
    } else {
        value = context.processor.stringify(file, context.settings);
    }

    file.contents = value;

    debug('Compiled document to %s', file.extension || 'markdown');
}

/*
 * Expose.
 */

module.exports = stringify;
