/**
 * @fileoverview Rule to disalow whitespace that is not a tab or space, whitespace inside strings and comments are allowed
 * @author Jonathan Kingston
 * @copyright 2014 Jonathan Kingston. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var irregularWhitespace = /[\u0085\u00A0\ufeff\f\v\u00a0\u1680\u180e\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u200b\u202f\u205f\u3000]+/mg,
        irregularLineTerminators = /[\u2028\u2029]/mg;

    // Module store of errors that we have found
    var errors = [];

    /**
     * Removes errors that occur inside a string node
     * @param {ASTNode} node to check for matching errors.
     * @returns {void}
     * @private
     */
    function removeStringError(node) {
        var locStart = node.loc.start;
        var locEnd = node.loc.end;

        errors = errors.filter(function (error) {
            var errorLoc = error[1];
            if (errorLoc.line >= locStart.line && errorLoc.line <= locEnd.line) {
                if (errorLoc.column >= locStart.column && (errorLoc.column <= locEnd.column || errorLoc.line < locEnd.line)) {
                    return false;
                }
            }
            return true;
        });
    }

    /**
     * Checks nodes for errors that we are choosing to ignore and calls the relevant methods to remove the errors
     * @param {ASTNode} node to check for matching errors.
     * @returns {void}
     * @private
     */
    function removeInvalidNodeErrors(node) {
        if (typeof node.value === "string") {
            // If we have irregular characters remove them from the errors list
            if (node.raw.match(irregularWhitespace) || node.raw.match(irregularLineTerminators)) {
                removeStringError(node);
            }
        }
    }

    /**
     * Checks the program source for irregular whitespace
     * @param {ASTNode} node The program node
     * @returns {void}
     * @private
     */
    function checkForIrregularWhitespace(node) {
        var sourceLines = context.getSourceLines();

        sourceLines.forEach(function (sourceLine, lineIndex) {
            var lineNumber = lineIndex + 1,
                location,
                match;

            while ((match = irregularWhitespace.exec(sourceLine)) !== null) {
                location = {
                    line: lineNumber,
                    column: match.index
                };

                errors.push([node, location, "Irregular whitespace not allowed"]);
            }
        });
    }

    /**
     * Checks the program source for irregular line terminators
     * @param {ASTNode} node The program node
     * @returns {void}
     * @private
     */
    function checkForIrregularLineTerminators(node) {
        var source = context.getSource(),
            sourceLines = context.getSourceLines(),
            linebreaks = source.match(/\r\n|\r|\n|\u2028|\u2029/g),
            lastLineIndex = -1,
            lineIndex,
            location,
            match;

        while ((match = irregularLineTerminators.exec(source)) !== null) {
            lineIndex = linebreaks.indexOf(match[0], lastLineIndex + 1) || 0;

            location = {
                line: lineIndex + 1,
                column: sourceLines[lineIndex].length
            };

            errors.push([node, location, "Irregular whitespace not allowed"]);
            lastLineIndex = lineIndex;
        }
    }

    return {
        "Program": function (node) {
            /**
             * As we can easily fire warnings for all white space issues with all the source its simpler to fire them here
             * This means we can check all the application code without having to worry about issues caused in the parser tokens
             * When writing this code also evaluating per node was missing out connecting tokens in some cases
             * We can later filter the errors when they are found to be not an issue in nodes we don't care about
             */

            checkForIrregularWhitespace(node);
            checkForIrregularLineTerminators(node);
        },

        "Identifier": removeInvalidNodeErrors,
        "Literal": removeInvalidNodeErrors,
        "Program:exit": function () {

            // If we have any errors remaining report on them
            errors.forEach(function (error) {
                context.report.apply(this, error);
            });
        }
    };
};

module.exports.schema = [];
