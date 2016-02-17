/**
 * @fileoverview Rule to check for max length on a line.
 * @author Matt DuVall <http://www.mattduvall.com>
 * @copyright 2013 Matt DuVall. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    // takes some ideas from http://tools.ietf.org/html/rfc3986#appendix-B, however:
    // - They're matching an entire string that we know is a URI
    // - We're matching part of a string where we think there *might* be a URL
    // - We're only concerned about URLs, as picking out any URI would cause too many false positives
    // - We don't care about matching the entire URL, any small segment is fine
    var URL_REGEXP = /[^:/?#]:\/\/[^?#]/;

    /**
     * Creates a string that is made up of repeating a given string a certain
     * number of times. This uses exponentiation of squares to achieve significant
     * performance gains over the more traditional implementation of such
     * functionality.
     * @param {string} str The string to repeat.
     * @param {int} num The number of times to repeat the string.
     * @returns {string} The created string.
     * @private
     */
    function stringRepeat(str, num) {
        var result = "";
        for (num |= 0; num > 0; num >>>= 1, str += str) {
            if (num & 1) {
                result += str;
            }
        }
        return result;
    }

    var maxLength = context.options[0] || 80,
        tabWidth = context.options[1] || 4,
        ignoreOptions = context.options[2] || {},
        ignorePattern = ignoreOptions.ignorePattern || null,
        ignoreComments = ignoreOptions.ignoreComments || false,
        ignoreUrls = ignoreOptions.ignoreUrls || false,
        tabString = stringRepeat(" ", tabWidth);

    if (ignorePattern) {
        ignorePattern = new RegExp(ignorePattern);
    }

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Tells if a given comment is trailing: it starts on the current line and
     * extends to or past the end of the current line.
     * @param {string} line The source line we want to check for a trailing comment on
     * @param {number} lineNumber The one-indexed line number for line
     * @param {ASTNode} comment The comment to inspect
     * @returns {boolean} If the comment is trailing on the given line
     */
    function isTrailingComment(line, lineNumber, comment) {
        return comment &&
            (comment.loc.start.line <= lineNumber && lineNumber <= comment.loc.end.line) &&
            (comment.loc.end.line > lineNumber || comment.loc.end.column === line.length);
    }

    /**
     * Gets the line after the comment and any remaining trailing whitespace is
     * stripped.
     * @param {string} line The source line with a trailing comment
     * @param {number} lineNumber The one-indexed line number this is on
     * @param {ASTNode} comment The comment to remove
     * @returns {string} Line without comment and trailing whitepace
     */
    function stripTrailingComment(line, lineNumber, comment) {
        if (comment.loc.start.line < lineNumber) {
            // this entire line is a comment
            return "";
        } else {
            // loc.column is zero-indexed
            return line.slice(0, comment.loc.start.column).replace(/\s+$/, "");
        }
    }

    /**
     * Check the program for max length
     * @param {ASTNode} node Node to examine
     * @returns {void}
     * @private
     */
    function checkProgramForMaxLength(node) {
        // split (honors line-ending)
        var lines = context.getSourceLines(),
            // list of comments to ignore
            comments = ignoreComments ? context.getAllComments() : [],
            // we iterate over comments in parallel with the lines
            commentsIndex = 0;

        lines.forEach(function(line, i) {
            // i is zero-indexed, line numbers are one-indexed
            var lineNumber = i + 1;
            // we can short-circuit the comment checks if we're already out of comments to check
            if (commentsIndex < comments.length) {
                // iterate over comments until we find one past the current line
                do {
                    var comment = comments[++commentsIndex];
                } while (comment && comment.loc.start.line <= lineNumber);
                // and step back by one
                comment = comments[--commentsIndex];
                if (isTrailingComment(line, lineNumber, comment)) {
                    line = stripTrailingComment(line, lineNumber, comment);
                }
            }
            if (ignorePattern && ignorePattern.test(line) ||
                ignoreUrls && URL_REGEXP.test(line)) {
                // ignore this line
                return;
            }
            // replace the tabs
            if (line.replace(/\t/g, tabString).length > maxLength) {
                context.report(node, { line: lineNumber, column: 0 }, "Line " + (i + 1) + " exceeds the maximum line length of " + maxLength + ".");
            }
        });
    }


    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "Program": checkProgramForMaxLength
    };

};

module.exports.schema = [
    {
        "type": "integer",
        "minimum": 0
    },
    {
        "type": "integer",
        "minimum": 0
    },
    {
        "type": "object",
        "properties": {
            "ignorePattern": {
                "type": "string"
            },
            "ignoreComments": {
                "type": "boolean"
            },
            "ignoreUrls": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
