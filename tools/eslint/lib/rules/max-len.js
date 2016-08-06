/**
 * @fileoverview Rule to check for max length on a line.
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

let OPTIONS_SCHEMA = {
    type: "object",
    properties: {
        code: {
            type: "integer",
            minimum: 0
        },
        comments: {
            type: "integer",
            minimum: 0
        },
        tabWidth: {
            type: "integer",
            minimum: 0
        },
        ignorePattern: {
            type: "string"
        },
        ignoreComments: {
            type: "boolean"
        },
        ignoreUrls: {
            type: "boolean"
        },
        ignoreTrailingComments: {
            type: "boolean"
        }
    },
    additionalProperties: false
};

let OPTIONS_OR_INTEGER_SCHEMA = {
    anyOf: [
        OPTIONS_SCHEMA,
        {
            type: "integer",
            minimum: 0
        }
    ]
};

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce a maximum line length",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            OPTIONS_OR_INTEGER_SCHEMA,
            OPTIONS_OR_INTEGER_SCHEMA,
            OPTIONS_SCHEMA
        ]
    },

    create: function(context) {

        /*
         * Inspired by http://tools.ietf.org/html/rfc3986#appendix-B, however:
         * - They're matching an entire string that we know is a URI
         * - We're matching part of a string where we think there *might* be a URL
         * - We're only concerned about URLs, as picking out any URI would cause
         *   too many false positives
         * - We don't care about matching the entire URL, any small segment is fine
         */
        let URL_REGEXP = /[^:/?#]:\/\/[^?#]/;

        let sourceCode = context.getSourceCode();

        /**
         * Computes the length of a line that may contain tabs. The width of each
         * tab will be the number of spaces to the next tab stop.
         * @param {string} line The line.
         * @param {int} tabWidth The width of each tab stop in spaces.
         * @returns {int} The computed line length.
         * @private
         */
        function computeLineLength(line, tabWidth) {
            let extraCharacterCount = 0;

            line.replace(/\t/g, function(match, offset) {
                let totalOffset = offset + extraCharacterCount,
                    previousTabStopOffset = tabWidth ? totalOffset % tabWidth : 0,
                    spaceCount = tabWidth - previousTabStopOffset;

                extraCharacterCount += spaceCount - 1;  // -1 for the replaced tab
            });
            return line.length + extraCharacterCount;
        }

        // The options object must be the last option specified…
        let lastOption = context.options[context.options.length - 1];
        let options = typeof lastOption === "object" ? Object.create(lastOption) : {};

        // …but max code length…
        if (typeof context.options[0] === "number") {
            options.code = context.options[0];
        }

        // …and tabWidth can be optionally specified directly as integers.
        if (typeof context.options[1] === "number") {
            options.tabWidth = context.options[1];
        }

        let maxLength = options.code || 80,
            tabWidth = options.tabWidth || 4,
            ignorePattern = options.ignorePattern || null,
            ignoreComments = options.ignoreComments || false,
            ignoreTrailingComments = options.ignoreTrailingComments || options.ignoreComments || false,
            ignoreUrls = options.ignoreUrls || false,
            maxCommentLength = options.comments;

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
                (comment.loc.start.line === lineNumber && lineNumber <= comment.loc.end.line) &&
                (comment.loc.end.line > lineNumber || comment.loc.end.column === line.length);
        }

        /**
         * Tells if a comment encompasses the entire line.
         * @param {string} line The source line with a trailing comment
         * @param {number} lineNumber The one-indexed line number this is on
         * @param {ASTNode} comment The comment to remove
         * @returns {boolean} If the comment covers the entire line
         */
        function isFullLineComment(line, lineNumber, comment) {
            let start = comment.loc.start,
                end = comment.loc.end,
                isFirstTokenOnLine = !line.slice(0, comment.loc.start.column).trim();

            return comment &&
                (start.line < lineNumber || (start.line === lineNumber && isFirstTokenOnLine)) &&
                (end.line > lineNumber || (end.line === lineNumber && end.column === line.length));
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

            // loc.column is zero-indexed
            return line.slice(0, comment.loc.start.column).replace(/\s+$/, "");
        }

        /**
         * Check the program for max length
         * @param {ASTNode} node Node to examine
         * @returns {void}
         * @private
         */
        function checkProgramForMaxLength(node) {

            // split (honors line-ending)
            let lines = sourceCode.lines,

                // list of comments to ignore
                comments = ignoreComments || maxCommentLength || ignoreTrailingComments ? sourceCode.getAllComments() : [],

                // we iterate over comments in parallel with the lines
                commentsIndex = 0;

            lines.forEach(function(line, i) {

                // i is zero-indexed, line numbers are one-indexed
                let lineNumber = i + 1;

                /*
                 * if we're checking comment length; we need to know whether this
                 * line is a comment
                 */
                let lineIsComment = false;

                /*
                 * We can short-circuit the comment checks if we're already out of
                 * comments to check.
                 */
                if (commentsIndex < comments.length) {
                    let comment = null;

                    // iterate over comments until we find one past the current line
                    do {
                        comment = comments[++commentsIndex];
                    } while (comment && comment.loc.start.line <= lineNumber);

                    // and step back by one
                    comment = comments[--commentsIndex];

                    if (isFullLineComment(line, lineNumber, comment)) {
                        lineIsComment = true;
                    } else if (ignoreTrailingComments && isTrailingComment(line, lineNumber, comment)) {
                        line = stripTrailingComment(line, lineNumber, comment);
                    }
                }
                if (ignorePattern && ignorePattern.test(line) ||
                    ignoreUrls && URL_REGEXP.test(line)) {

                    // ignore this line
                    return;
                }

                let lineLength = computeLineLength(line, tabWidth);

                if (lineIsComment && ignoreComments) {
                    return;
                }

                if (lineIsComment && lineLength > maxCommentLength) {
                    context.report(node, { line: lineNumber, column: 0 }, "Line " + (i + 1) + " exceeds the maximum comment line length of " + maxCommentLength + ".");
                } else if (lineLength > maxLength) {
                    context.report(node, { line: lineNumber, column: 0 }, "Line " + (i + 1) + " exceeds the maximum line length of " + maxLength + ".");
                }
            });
        }


        //--------------------------------------------------------------------------
        // Public API
        //--------------------------------------------------------------------------

        return {
            Program: checkProgramForMaxLength
        };

    }
};
