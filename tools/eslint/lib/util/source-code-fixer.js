/**
 * @fileoverview An object that caches and applies source code fixes.
 * @author Nicholas C. Zakas
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug")("eslint:text-fixer");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Compares items in a messages array by line and column.
 * @param {Message} a The first message.
 * @param {Message} b The second message.
 * @returns {int} -1 if a comes before b, 1 if a comes after b, 0 if equal.
 * @private
 */
function compareMessagesByLocation(a, b) {
    var lineDiff = a.line - b.line;

    if (lineDiff === 0) {
        return a.column - b.column;
    } else {
        return lineDiff;
    }
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Utility for apply fixes to source code.
 * @constructor
 */
function SourceCodeFixer() {
    Object.freeze(this);
}

/**
 * Applies the fixes specified by the messages to the given text. Tries to be
 * smart about the fixes and won't apply fixes over the same area in the text.
 * @param {SourceCode} sourceCode The source code to apply the changes to.
 * @param {Message[]} messages The array of messages reported by ESLint.
 * @returns {Object} An object containing the fixed text and any unfixed messages.
 */
SourceCodeFixer.applyFixes = function(sourceCode, messages) {

    debug("Applying fixes");

    if (!sourceCode) {
        debug("No source code to fix");
        return {
            fixed: false,
            messages: messages,
            output: ""
        };
    }

    // clone the array
    var remainingMessages = [],
        fixes = [],
        text = sourceCode.text,
        lastFixPos = text.length + 1;

    messages.forEach(function(problem) {
        if (problem.hasOwnProperty("fix")) {
            fixes.push(problem);
        } else {
            remainingMessages.push(problem);
        }
    });

    if (fixes.length) {
        debug("Found fixes to apply");

        // sort in reverse order of occurrence
        fixes.sort(function(a, b) {
            if (a.fix.range[1] <= b.fix.range[0]) {
                return 1;
            } else {
                return -1;
            }
        });

        // split into array of characters for easier manipulation
        var chars = text.split("");

        fixes.forEach(function(problem) {
            var fix = problem.fix;

            if (fix.range[1] < lastFixPos) {
                chars.splice(fix.range[0], fix.range[1] - fix.range[0], fix.text);
                lastFixPos = fix.range[0];
            } else {
                remainingMessages.push(problem);
            }
        });

        return {
            fixed: true,
            messages: remainingMessages.sort(compareMessagesByLocation),
            output: chars.join("")
        };
    } else {
        debug("No fixes to apply");
        return {
            fixed: false,
            messages: messages,
            output: text
        };
    }
};

module.exports = SourceCodeFixer;
