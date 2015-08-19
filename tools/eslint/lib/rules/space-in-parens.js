/**
 * @fileoverview Disallows or enforces spaces inside of parentheses.
 * @author Jonathan Rajavuori
 * @copyright 2014 David Clark. All rights reserved.
 * @copyright 2014 Jonathan Rajavuori. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MISSING_SPACE_MESSAGE = "There must be a space inside this paren.",
        REJECTED_SPACE_MESSAGE = "There should be no spaces inside this paren.",
        exceptionsArray = (context.options.length === 2) ? context.options[1].exceptions : [],
        options = {},
        rejectedSpaceRegExp,
        missingSpaceRegExp,
        spaceChecks;

    if (exceptionsArray && exceptionsArray.length) {
        options.braceException = exceptionsArray.indexOf("{}") !== -1 || false;
        options.bracketException = exceptionsArray.indexOf("[]") !== -1 || false;
        options.parenException = exceptionsArray.indexOf("()") !== -1 || false;
        options.empty = exceptionsArray.indexOf("empty") !== -1 || false;
    }

    /**
     * Used with the `never` option to produce, given the exception options,
     * two regular expressions to check for missing and rejected spaces.
     * @param {Object} opts The exception options
     * @returns {Object} `missingSpace` and `rejectedSpace` regular expressions
     * @private
     */
    function getNeverChecks(opts) {
        var missingSpaceOpeners = [],
            missingSpaceClosers = [],
            rejectedSpaceOpeners = ["\\s"],
            rejectedSpaceClosers = ["\\s"],
            missingSpaceCheck,
            rejectedSpaceCheck;

        // Populate openers and closers
        if (opts.braceException) {
            missingSpaceOpeners.push("\\{");
            missingSpaceClosers.push("\\}");
            rejectedSpaceOpeners.push("\\{");
            rejectedSpaceClosers.push("\\}");
        }
        if (opts.bracketException) {
            missingSpaceOpeners.push("\\[");
            missingSpaceClosers.push("\\]");
            rejectedSpaceOpeners.push("\\[");
            rejectedSpaceClosers.push("\\]");
        }
        if (opts.parenException) {
            missingSpaceOpeners.push("\\(");
            missingSpaceClosers.push("\\)");
            rejectedSpaceOpeners.push("\\(");
            rejectedSpaceClosers.push("\\)");
        }
        if (opts.empty) {
            missingSpaceOpeners.push("\\)");
            missingSpaceClosers.push("\\(");
            rejectedSpaceOpeners.push("\\)");
            rejectedSpaceClosers.push("\\(");
        }

        if (missingSpaceOpeners.length) {
            missingSpaceCheck = "\\((" + missingSpaceOpeners.join("|") + ")";
            if (missingSpaceClosers.length) {
                missingSpaceCheck += "|";
            }
        }
        if (missingSpaceClosers.length) {
            missingSpaceCheck += "(" + missingSpaceClosers.join("|") + ")\\)";
        }

        // compose the rejected regexp
        rejectedSpaceCheck = "\\( +[^" + rejectedSpaceOpeners.join("") + "]";
        rejectedSpaceCheck += "|[^" + rejectedSpaceClosers.join("") + "] +\\)";

        return {
            // e.g. \((\{)|(\})\) --- where {} is an exception
            missingSpace: missingSpaceCheck || ".^",
            // e.g. \( +[^ \n\r\{]|[^ \n\r\}] +\) --- where {} is an exception
            rejectedSpace: rejectedSpaceCheck
        };
    }

    /**
     * Used with the `always` option to produce, given the exception options,
     * two regular expressions to check for missing and rejected spaces.
     * @param {Object} opts The exception options
     * @returns {Object} `missingSpace` and `rejectedSpace` regular expressions
     * @private
     */
    function getAlwaysChecks(opts) {
        var missingSpaceOpeners = ["\\s", "\\)"],
            missingSpaceClosers = ["\\s", "\\("],
            rejectedSpaceOpeners = [],
            rejectedSpaceClosers = [],
            missingSpaceCheck,
            rejectedSpaceCheck;

        // Populate openers and closers
        if (opts.braceException) {
            missingSpaceOpeners.push("\\{");
            missingSpaceClosers.push("\\}");
            rejectedSpaceOpeners.push(" \\{");
            rejectedSpaceClosers.push("\\} ");
        }
        if (opts.bracketException) {
            missingSpaceOpeners.push("\\[");
            missingSpaceClosers.push("\\]");
            rejectedSpaceOpeners.push(" \\[");
            rejectedSpaceClosers.push("\\] ");
        }
        if (opts.parenException) {
            missingSpaceOpeners.push("\\(");
            missingSpaceClosers.push("\\)");
            rejectedSpaceOpeners.push(" \\(");
            rejectedSpaceClosers.push("\\) ");
        }
        if (opts.empty) {
            rejectedSpaceOpeners.push(" \\)");
            rejectedSpaceClosers.push("\\( ");
        }

        // compose the allowed regexp
        missingSpaceCheck = "\\([^" + missingSpaceOpeners.join("") + "]";
        missingSpaceCheck += "|[^" + missingSpaceClosers.join("") + "]\\)";

        // compose the rejected regexp
        if (rejectedSpaceOpeners.length) {
            rejectedSpaceCheck = "\\((" + rejectedSpaceOpeners.join("|") + ")";
            if (rejectedSpaceClosers.length) {
                rejectedSpaceCheck += "|";
            }
        }
        if (rejectedSpaceClosers.length) {
            rejectedSpaceCheck += "(" + rejectedSpaceClosers.join("|") + ")\\)";
        }

        return {
            // e.g. \([^ \)\r\n\{]|[^ \(\r\n\}]\) --- where {} is an exception
            missingSpace: missingSpaceCheck,
            // e.g. \(( \{})|(\} )\) --- where {} is an excpetion
            rejectedSpace: rejectedSpaceCheck || ".^"
        };
    }

    spaceChecks = (context.options[0] === "always") ? getAlwaysChecks(options) : getNeverChecks(options);
    missingSpaceRegExp = new RegExp(spaceChecks.missingSpace, "mg");
    rejectedSpaceRegExp = new RegExp(spaceChecks.rejectedSpace, "mg");


    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    var skipRanges = [];

    /**
     * Adds the range of a node to the set to be skipped when checking parens
     * @param {ASTNode} node The node to skip
     * @returns {void}
     * @private
     */
    function addSkipRange(node) {
        skipRanges.push(node.range);
    }

    /**
     * Sorts the skipRanges array. Must be called before shouldSkip
     * @returns {void}
     * @private
     */
    function sortSkipRanges() {
        skipRanges.sort(function (a, b) {
            return a[0] - b[0];
        });
    }

    /**
     * Checks if a certain position in the source should be skipped
     * @param {Number} pos The 0-based index in the source
     * @returns {boolean} whether the position should be skipped
     * @private
     */
    function shouldSkip(pos) {
        var i, len, range;
        for (i = 0, len = skipRanges.length; i < len; i += 1) {
            range = skipRanges[i];
            if (pos < range[0]) {
                break;
            } else if (pos < range[1]) {
                return true;
            }
        }
        return false;
    }


    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "Program:exit": function checkParenSpaces(node) {

            var nextMatch,
                nextLine,
                column,
                line = 1,
                source = context.getSource(),
                pos = 0;

            function checkMatch(match, message) {
                if (source.charAt(match.index) !== "(") {
                    // Matched a closing paren pattern
                    match.index += 1;
                }

                if (!shouldSkip(match.index)) {
                    while ((nextLine = source.indexOf("\n", pos)) !== -1 && nextLine < match.index) {
                        pos = nextLine + 1;
                        line += 1;
                    }
                    column = match.index - pos;

                    context.report(node, { line: line, column: column }, message);
                }
            }

            sortSkipRanges();

            while ((nextMatch = rejectedSpaceRegExp.exec(source)) !== null) {
                checkMatch(nextMatch, REJECTED_SPACE_MESSAGE);
            }

            while ((nextMatch = missingSpaceRegExp.exec(source)) !== null) {
                checkMatch(nextMatch, MISSING_SPACE_MESSAGE);
            }

        },


        // These nodes can contain parentheses that this rule doesn't care about

        LineComment: addSkipRange,

        BlockComment: addSkipRange,

        Literal: addSkipRange

    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    },
    {
        "type": "object",
        "properties": {
            "exceptions": {
                "type": "array",
                "items": {
                    "enum": ["{}", "[]", "()", "empty"]
                },
                "uniqueItems": true
            }
        },
        "additionalProperties": false
    }
];
