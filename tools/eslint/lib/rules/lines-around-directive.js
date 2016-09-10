/**
 * @fileoverview Require or disallow newlines around directives.
 * @author Kai Cataldo
 */

"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require or disallow newlines around directives",
            category: "Stylistic Issues",
            recommended: false
        },
        schema: [{
            oneOf: [
                {
                    enum: ["always", "never"]
                },
                {
                    type: "object",
                    properties: {
                        before: {
                            enum: ["always", "never"]
                        },
                        after: {
                            enum: ["always", "never"]
                        },
                    },
                    additionalProperties: false,
                    minProperties: 2
                }
            ]
        }]
    },

    create(context) {
        const sourceCode = context.getSourceCode();
        const config = context.options[0] || "always";
        const expectLineBefore = typeof config === "string" ? config : config.before;
        const expectLineAfter = typeof config === "string" ? config : config.after;

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Check if node is preceded by a blank newline.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} Whether or not the passed in node is preceded by a blank newline.
         */
        function hasNewlineBefore(node) {
            const tokenBefore = sourceCode.getTokenOrCommentBefore(node);
            const tokenLineBefore = tokenBefore ? tokenBefore.loc.end.line : 0;

            return node.loc.start.line - tokenLineBefore >= 2;
        }

        /**
         * Check if node is followed by a blank newline.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} Whether or not the passed in node is followed by a blank newline.
         */
        function hasNewlineAfter(node) {
            const tokenAfter = sourceCode.getTokenOrCommentAfter(node);

            return tokenAfter.loc.start.line - node.loc.end.line >= 2;
        }

        /**
         * Report errors for newlines around directives.
         * @param {ASTNode} node Node to check.
         * @param {string} location Whether the error was found before or after the directive.
         * @param {boolean} expected Whether or not a newline was expected or unexpected.
         * @returns {void}
         */
        function reportError(node, location, expected) {
            context.report({
                node,
                message: "{{expected}} newline {{location}} \"{{value}}\" directive.",
                data: {
                    expected: expected ? "Expected" : "Unexpected",
                    value: node.expression.value,
                    location
                }
            });
        }

        /**
         * Check lines around directives in node
         * @param {ASTNode} node - node to check
         * @returns {void}
         */
        function checkDirectives(node) {
            const directives = astUtils.getDirectivePrologue(node);

            if (!directives.length) {
                return;
            }

            const firstDirective = directives[0];
            const hasTokenOrCommentBefore = !!sourceCode.getTokenOrCommentBefore(firstDirective);

            // Only check before the first directive if it is preceded by a comment or if it is at the top of
            // the file and expectLineBefore is set to "never". This is to not force a newline at the top of
            // the file if there are no comments as well as for compatibility with padded-blocks.
            if (
                firstDirective.leadingComments && firstDirective.leadingComments.length ||

                // Shebangs are not added to leading comments but are accounted for by the following.
                node.type === "Program" && hasTokenOrCommentBefore
            ) {
                if (expectLineBefore === "always" && !hasNewlineBefore(firstDirective)) {
                    reportError(firstDirective, "before", true);
                }

                if (expectLineBefore === "never" && hasNewlineBefore(firstDirective)) {
                    reportError(firstDirective, "before", false);
                }
            } else if (
                node.type === "Program" &&
                expectLineBefore === "never" &&
                !hasTokenOrCommentBefore &&
                hasNewlineBefore(firstDirective)
            ) {
                reportError(firstDirective, "before", false);
            }

            const lastDirective = directives[directives.length - 1];
            const statements = node.type === "Program" ? node.body : node.body.body;

            // Do not check after the last directive if the body only
            // contains a directive prologue and isn't followed by a comment to ensure
            // this rule behaves well with padded-blocks.
            if (lastDirective === statements[statements.length - 1] && !lastDirective.trailingComments) {
                return;
            }

            if (expectLineAfter === "always" && !hasNewlineAfter(lastDirective)) {
                reportError(lastDirective, "after", true);
            }

            if (expectLineAfter === "never" && hasNewlineAfter(lastDirective)) {
                reportError(lastDirective, "after", false);
            }
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            Program: checkDirectives,
            FunctionDeclaration: checkDirectives,
            FunctionExpression: checkDirectives,
            ArrowFunctionExpression: checkDirectives
        };
    }
};
