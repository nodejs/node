/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-set-pipeline:traverse
 * @version 3.2.2
 * @fileoverview Find files from the file-system.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var Ignore = require('../ignore');
var Finder = require('../finder');

/**
 * Find files from the file-system.
 *
 * @example
 *   traverse({
 *     'extensions': ['markdown']
 *   });
 *
 * @param {Object} context - Context object.
 * @param {function(Error?)} done - Callback invoked when
 *   done.
 */
function traverse(context, done) {
    var ignore;

    /** Traverse. */
    function next() {
        /*
         * Use the files when they were injected,
         * which we now because there are no globs.
         */

        if (context.files && !context.globs.length) {
            done();
            return;
        }

        context.traverser.find(context.globs, function (err, files) {
            context.files = files || [];
            done(err);
        });
    }

    if (context.traverser) {
        next();
    } else {
        ignore = new Ignore({
            'file': context.ignorePath,
            'detectIgnore': context.detectIgnore
        });

        context.traverser = new Finder({
            'extensions': context.extensions,
            'ignore': ignore
        });

        ignore.loadPatterns(next);
    }
}

/*
 * Expose.
 */

module.exports = traverse;
