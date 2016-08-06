/**
 * @fileoverview Rule to disalow whitespace that is not a tab or space, whitespace inside strings and comments are allowed
 * @author Jonathan Kingston
 * @author Christophe Porteneuve
 */

"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

let ALL_IRREGULARS = /[\f\v\u0085\u00A0\ufeff\u00a0\u1680\u180e\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u200b\u202f\u205f\u3000\u2028\u2029]/;
let IRREGULAR_WHITESPACE = /[\f\v\u0085\u00A0\ufeff\u00a0\u1680\u180e\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u200b\u202f\u205f\u3000]+/mg;
let IRREGULAR_LINE_TERMINATORS = /[\u2028\u2029]/mg;
let LINE_BREAK = /\r\n|\r|\n|\u2028|\u2029/g;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow irregular whitespace outside of strings and comments",
            category: "Possible Errors",
            recommended: true
        },

        schema: [
            {
                type: "object",
                properties: {
                    skipComments: {
                        type: "boolean"
                    },
                    skipStrings: {
                        type: "boolean"
                    },
                    skipTemplates: {
                        type: "boolean"
                    },
                    skipRegExps: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        // Module store of errors that we have found
        let errors = [];

        // Comment nodes.  We accumulate these as we go, so we can be sure to trigger them after the whole `Program` entity is parsed, even for top-of-file comments.
        let commentNodes = [];

        // Lookup the `skipComments` option, which defaults to `false`.
        let options = context.options[0] || {};
        let skipComments = !!options.skipComments;
        let skipStrings = options.skipStrings !== false;
        let skipRegExps = !!options.skipRegExps;
        let skipTemplates = !!options.skipTemplates;

        let sourceCode = context.getSourceCode();

        /**
         * Removes errors that occur inside a string node
         * @param {ASTNode} node to check for matching errors.
         * @returns {void}
         * @private
         */
        function removeWhitespaceError(node) {
            let locStart = node.loc.start;
            let locEnd = node.loc.end;

            errors = errors.filter(function(error) {
                let errorLoc = error[1];

                if (errorLoc.line >= locStart.line && errorLoc.line <= locEnd.line) {
                    if (errorLoc.column >= locStart.column && (errorLoc.column <= locEnd.column || errorLoc.line < locEnd.line)) {
                        return false;
                    }
                }
                return true;
            });
        }

        /**
         * Checks identifier or literal nodes for errors that we are choosing to ignore and calls the relevant methods to remove the errors
         * @param {ASTNode} node to check for matching errors.
         * @returns {void}
         * @private
         */
        function removeInvalidNodeErrorsInIdentifierOrLiteral(node) {
            let shouldCheckStrings = skipStrings && (typeof node.value === "string");
            let shouldCheckRegExps = skipRegExps && (node.value instanceof RegExp);

            if (shouldCheckStrings || shouldCheckRegExps) {

                // If we have irregular characters remove them from the errors list
                if (ALL_IRREGULARS.test(node.raw)) {
                    removeWhitespaceError(node);
                }
            }
        }

        /**
         * Checks template string literal nodes for errors that we are choosing to ignore and calls the relevant methods to remove the errors
         * @param {ASTNode} node to check for matching errors.
         * @returns {void}
         * @private
         */
        function removeInvalidNodeErrorsInTemplateLiteral(node) {
            if (typeof node.value.raw === "string") {
                if (ALL_IRREGULARS.test(node.value.raw)) {
                    removeWhitespaceError(node);
                }
            }
        }

        /**
         * Checks comment nodes for errors that we are choosing to ignore and calls the relevant methods to remove the errors
         * @param {ASTNode} node to check for matching errors.
         * @returns {void}
         * @private
         */
        function removeInvalidNodeErrorsInComment(node) {
            if (ALL_IRREGULARS.test(node.value)) {
                removeWhitespaceError(node);
            }
        }

        /**
         * Checks the program source for irregular whitespace
         * @param {ASTNode} node The program node
         * @returns {void}
         * @private
         */
        function checkForIrregularWhitespace(node) {
            let sourceLines = sourceCode.lines;

            sourceLines.forEach(function(sourceLine, lineIndex) {
                let lineNumber = lineIndex + 1,
                    location,
                    match;

                while ((match = IRREGULAR_WHITESPACE.exec(sourceLine)) !== null) {
                    location = {
                        line: lineNumber,
                        column: match.index
                    };

                    errors.push([node, location, "Irregular whitespace not allowed."]);
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
            let source = sourceCode.getText(),
                sourceLines = sourceCode.lines,
                linebreaks = source.match(LINE_BREAK),
                lastLineIndex = -1,
                lineIndex,
                location,
                match;

            while ((match = IRREGULAR_LINE_TERMINATORS.exec(source)) !== null) {
                lineIndex = linebreaks.indexOf(match[0], lastLineIndex + 1) || 0;

                location = {
                    line: lineIndex + 1,
                    column: sourceLines[lineIndex].length
                };

                errors.push([node, location, "Irregular whitespace not allowed."]);
                lastLineIndex = lineIndex;
            }
        }

        /**
         * Stores a comment node (`LineComment` or `BlockComment`) for later stripping of errors within; a necessary deferring of processing to deal with top-of-file comments.
         * @param {ASTNode} node The comment node
         * @returns {void}
         * @private
         */
        function rememberCommentNode(node) {
            commentNodes.push(node);
        }

        /**
         * A no-op function to act as placeholder for comment accumulation when the `skipComments` option is `false`.
         * @returns {void}
         * @private
         */
        function noop() {}

        let nodes = {};

        if (ALL_IRREGULARS.test(sourceCode.getText())) {
            nodes.Program = function(node) {

                /*
                 * As we can easily fire warnings for all white space issues with
                 * all the source its simpler to fire them here.
                 * This means we can check all the application code without having
                 * to worry about issues caused in the parser tokens.
                 * When writing this code also evaluating per node was missing out
                 * connecting tokens in some cases.
                 * We can later filter the errors when they are found to be not an
                 * issue in nodes we don't care about.
                 */

                checkForIrregularWhitespace(node);
                checkForIrregularLineTerminators(node);
            };

            nodes.Identifier = removeInvalidNodeErrorsInIdentifierOrLiteral;
            nodes.Literal = removeInvalidNodeErrorsInIdentifierOrLiteral;
            nodes.TemplateElement = skipTemplates ? removeInvalidNodeErrorsInTemplateLiteral : noop;
            nodes.LineComment = skipComments ? rememberCommentNode : noop;
            nodes.BlockComment = skipComments ? rememberCommentNode : noop;
            nodes["Program:exit"] = function() {

                if (skipComments) {

                    // First strip errors occurring in comment nodes.  We have to do this post-`Program` to deal with top-of-file comments.
                    commentNodes.forEach(removeInvalidNodeErrorsInComment);
                }

                // If we have any errors remaining report on them
                errors.forEach(function(error) {
                    context.report.apply(context, error);
                });
            };
        } else {
            nodes.Program = noop;
        }

        return nodes;
    }
};
