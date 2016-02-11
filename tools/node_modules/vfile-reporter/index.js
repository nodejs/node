/**
 * @author Titus Wormer
 * @author Sindre Sorhus
 * @copyright 2015 Titus Wormer
 * @copyright 2013 Nicholas C. Zakas
 * @license MIT
 * @module vfile:reporter
 * @fileoverview Stylish reporter for virtual files.
 */

'use strict';

/*
 * Dependencies.
 */

var pluralize = require('plur');
var width = require('string-width');
var symbols = require('log-symbols');
var chalk = require('chalk');
var table = require('text-table');
var repeat = require('repeat-string');
var sort = require('vfile-sort');

/*
 * Map of no-warning messages, where `true` refers to
 * `compile: true`.
 */

var SUCCESS = {
    'true': chalk.yellow('written'),
    'false': 'no issues found'
};

/*
 * List of probabbly lengths of messages.
 */

var POSITION_LENGTH = '00:0-00:0'.length;
var LABEL_LENGTH = 'message'.length;
var MESSAGE_LENGTH = 'this is an average message'.length;

/**
 * Reject messages without `fatal: true`.
 *
 * NOTE: Modifies the given files.
 *
 * @param {Array.<VFile>} files - List of files.
 */
function removeNonFatalMessages(files) {
    files.forEach(function (file) {
        file.messages = file.messages.filter(function (message) {
            return message.fatal === true;
        })
    });
}

/**
 * Reject files without messages.
 *
 * @param {Array.<VFile>} files - List of files.
 * @return {Array.<VFile>} - `files` without non-failed messages.
 */
function removeNonFailedFiles(files) {
    return files.filter(function (file) {
        return Boolean(file.messages.length);
    });
}

/**
 * Get the length of `value`, ignoring ANSI sequences.
 *
 * @param {string} value - Value to `pad`.
 * @return {number} - Length of `value`.
 */
function realLength(value) {
    var index = value.indexOf('\n');

    if (index !== -1) {
        value = value.slice(0, index);
    }

    return width(value);
}

/**
 * Pad `value` on the `side` (where truthy means left and
 * falsey means right).
 *
 * @param {string} value - Value to `pad`.
 * @param {number} minimum - Pad to `minimum`.
 * @param {boolean?} [side] - Side to pad on.
 * @return {string} - Right-padded `value`.
 */
function pad(value, minimum, side) {
    var padding = repeat(' ', minimum - realLength(value));
    return side ? padding + value : value + padding;
}

/**
 * Pad `value` on the left.
 *
 * @param {string} value - Value to `pad`.
 * @param {number} minimum - Pad to `minimum`.
 * @return {string} - Left-padded `value`.
 */
function padLeft(value, minimum) {
    return pad(value, minimum, true);
}

/**
 * Pad `value` on the right.
 *
 * @param {string} value - Value to `pad`.
 * @param {number} minimum - Pad to `minimum`.
 * @return {string} - Right-padded `value`.
 */
function padRight(value, minimum) {
    return pad(value, minimum, false);
}

/**
 * @param {VFile|Array.<VFile>} files - One or more virtual
 *   files.
 * @param {Object} [options] - Configuration.
 * @param {Object} [options.quiet=false] - Do not output
 *   anything for a file which has no messages. The default
 *   behaviour is to show a success message.
 * @param {Object} [options.silent=false] - Do not output
 *   messages without `fatal` set to true. Also sets
 *   `quiet` to `true`.
 * @param {Object} [options.verbose=false] - Output notes.
 * @return {string} - Formatted files.
 */
function reporter(files, options) {
    var total = 0;
    var errors = 0;
    var warnings = 0;
    var result = [];
    var listing = false;
    var summaryColor;
    var summary;
    var verbose;

    if (!files) {
        return '';
    }

    if (!('length' in files)) {
        files = [files];
    }

    if (!options) {
        options = {};
    }

    verbose = options.verbose || false;

    if (options.silent) {
        removeNonFatalMessages(files);
    }

    if (options.silent || options.quiet) {
        files = removeNonFailedFiles(files);
    }

    files.forEach(function (file, position) {
        var destination = file.filePath();
        var filePath = file.history[0] || destination;
        var stored = Boolean(file.stored);
        var moved = stored && destination !== filePath;
        var name = filePath || '<stdin>';
        var output = '';
        var messages;
        var fileColor;

        sort(file);

        messages = file.messages;

        total += messages.length;

        messages = messages.map(function (message) {
            var color = 'yellow';
            var location = message.name;
            var label;
            var reason;

            if (filePath) {
                location = location.slice(location.indexOf(':') + 1);
            }

            if (message.fatal) {
                color = fileColor = summaryColor = 'red';
                label = 'error';
                errors++;
            } else if (message.fatal === false) {
                label = 'warning';
                warnings++;

                if (!summaryColor) {
                    summaryColor = color;
                }

                if (!fileColor) {
                    fileColor = color;
                }
            } else {
                label = 'message';
                color = 'gray';
            }

            reason = message.stack || message.message;

            if (verbose && message.note) {
                reason += '\n' + message.note;
            }

            return [
                '',
                padLeft(location, POSITION_LENGTH),
                padRight(chalk[color](label), LABEL_LENGTH),
                padRight(reason, MESSAGE_LENGTH),
                message.ruleId || ''
            ];
        });

        if (listing || (messages.length && position !== 0)) {
            output += '\n';
        }

        output += chalk.underline[fileColor || 'green'](name);

        if (moved) {
            output += ' > ' + destination;
        }

        listing = Boolean(messages.length);

        if (!listing) {
            output += ': ' + SUCCESS[stored];
        } else {
            output += '\n' + table(messages, {
                'align': ['', 'l', 'l', 'l'],
                'stringLength': realLength
            });
        }

        result.push(output);
    });

    if (errors || warnings) {
        summary = [];

        if (errors) {
            summary.push([
                symbols.error,
                errors,
                pluralize('error', errors)
            ].join(' '));
        }

        if (warnings) {
            summary.push([
                symbols.warning,
                warnings,
                pluralize('warning', warnings)
            ].join(' '));
        }

        summary = summary.join(', ');

        if (errors && warnings) {
            summary = total + ' messages (' + summary + ')';
        }

        result.push('\n' + summary);
    }

    return result.length ? result.join('\n') : '';
}

/*
 * Expose.
 */

module.exports = reporter;
