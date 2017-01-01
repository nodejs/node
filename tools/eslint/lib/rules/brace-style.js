/**
 * @fileoverview Rule to flag block statements that do not use the one true brace style
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce consistent brace style for blocks",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["1tbs", "stroustrup", "allman"]
            },
            {
                type: "object",
                properties: {
                    allowSingleLine: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ],

        fixable: "whitespace"
    },

    create(context) {
        const style = context.options[0] || "1tbs",
            params = context.options[1] || {},
            sourceCode = context.getSourceCode();

        const OPEN_MESSAGE = "Opening curly brace does not appear on the same line as controlling statement.",
            OPEN_MESSAGE_ALLMAN = "Opening curly brace appears on the same line as controlling statement.",
            BODY_MESSAGE = "Statement inside of curly braces should be on next line.",
            CLOSE_MESSAGE = "Closing curly brace does not appear on the same line as the subsequent block.",
            CLOSE_MESSAGE_SINGLE = "Closing curly brace should be on the same line as opening curly brace or on the line after the previous block.",
            CLOSE_MESSAGE_STROUSTRUP_ALLMAN = "Closing curly brace appears on the same line as the subsequent block.";

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Determines if a given node is a block statement.
         * @param {ASTNode} node The node to check.
         * @returns {boolean} True if the node is a block statement, false if not.
         * @private
         */
        function isBlock(node) {
            return node && node.type === "BlockStatement";
        }

        /**
         * Check if the token is an punctuator with a value of curly brace
         * @param {Object} token - Token to check
         * @returns {boolean} true if its a curly punctuator
         * @private
         */
        function isCurlyPunctuator(token) {
            return token.value === "{" || token.value === "}";
        }

        /**
        * Reports a place where a newline unexpectedly appears
        * @param {ASTNode} node The node to report
        * @param {string} message The message to report
        * @param {Token} firstToken The token before the unexpected newline
        * @returns {void}
        */
        function reportExtraNewline(node, message, firstToken) {
            context.report({
                node,
                message,
                fix(fixer) {
                    const secondToken = sourceCode.getTokenAfter(firstToken);
                    const textBetween = sourceCode.getText().slice(firstToken.range[1], secondToken.range[0]);
                    const NEWLINE_REGEX = /\r\n|\r|\n|\u2028|\u2029/g;

                    // Don't do a fix if there is a comment between the tokens.
                    return textBetween.trim() ? null : fixer.replaceTextRange([firstToken.range[1], secondToken.range[0]], textBetween.replace(NEWLINE_REGEX, ""));
                }
            });
        }

        /**
         * Binds a list of properties to a function that verifies that the opening
         * curly brace is on the same line as its controlling statement of a given
         * node.
         * @param {...string} The properties to check on the node.
         * @returns {Function} A function that will perform the check on a node
         * @private
         */
        function checkBlock() {
            const blockProperties = arguments;

            return function(node) {
                Array.prototype.forEach.call(blockProperties, blockProp => {
                    const block = node[blockProp];

                    if (!isBlock(block)) {
                        return;
                    }

                    const previousToken = sourceCode.getTokenBefore(block);
                    const curlyToken = sourceCode.getFirstToken(block);
                    const curlyTokenEnd = sourceCode.getLastToken(block);
                    const allOnSameLine = previousToken.loc.start.line === curlyTokenEnd.loc.start.line;

                    if (allOnSameLine && params.allowSingleLine) {
                        return;
                    }

                    if (style !== "allman" && previousToken.loc.start.line !== curlyToken.loc.start.line) {
                        reportExtraNewline(node, OPEN_MESSAGE, previousToken);
                    } else if (style === "allman" && previousToken.loc.start.line === curlyToken.loc.start.line) {
                        context.report({
                            node,
                            message: OPEN_MESSAGE_ALLMAN,
                            fix: fixer => fixer.insertTextBefore(curlyToken, "\n")
                        });
                    }

                    if (!block.body.length) {
                        return;
                    }

                    if (curlyToken.loc.start.line === block.body[0].loc.start.line) {
                        context.report({
                            node: block.body[0],
                            message: BODY_MESSAGE,
                            fix: fixer => fixer.insertTextAfter(curlyToken, "\n")
                        });
                    }

                    if (curlyTokenEnd.loc.start.line === block.body[block.body.length - 1].loc.end.line) {
                        context.report({
                            node: block.body[block.body.length - 1],
                            message: CLOSE_MESSAGE_SINGLE,
                            fix: fixer => fixer.insertTextBefore(curlyTokenEnd, "\n")
                        });
                    }
                });
            };
        }

        /**
         * Enforces the configured brace style on IfStatements
         * @param {ASTNode} node An IfStatement node.
         * @returns {void}
         * @private
         */
        function checkIfStatement(node) {
            checkBlock("consequent", "alternate")(node);

            if (node.alternate) {

                const tokens = sourceCode.getTokensBefore(node.alternate, 2);

                if (style === "1tbs") {
                    if (tokens[0].loc.start.line !== tokens[1].loc.start.line &&
                        node.consequent.type === "BlockStatement" &&
                        isCurlyPunctuator(tokens[0])) {
                        reportExtraNewline(node.alternate, CLOSE_MESSAGE, tokens[0]);
                    }
                } else if (tokens[0].loc.start.line === tokens[1].loc.start.line) {
                    context.report({
                        node: node.alternate,
                        message: CLOSE_MESSAGE_STROUSTRUP_ALLMAN,
                        fix: fixer => fixer.insertTextAfter(tokens[0], "\n")
                    });
                }

            }
        }

        /**
         * Enforces the configured brace style on TryStatements
         * @param {ASTNode} node A TryStatement node.
         * @returns {void}
         * @private
         */
        function checkTryStatement(node) {
            checkBlock("block", "finalizer")(node);

            if (isBlock(node.finalizer)) {
                const tokens = sourceCode.getTokensBefore(node.finalizer, 2);

                if (style === "1tbs") {
                    if (tokens[0].loc.start.line !== tokens[1].loc.start.line) {
                        reportExtraNewline(node.finalizer, CLOSE_MESSAGE, tokens[0]);
                    }
                } else if (tokens[0].loc.start.line === tokens[1].loc.start.line) {
                    context.report({
                        node: node.finalizer,
                        message: CLOSE_MESSAGE_STROUSTRUP_ALLMAN,
                        fix: fixer => fixer.insertTextAfter(tokens[0], "\n")
                    });
                }
            }
        }

        /**
         * Enforces the configured brace style on CatchClauses
         * @param {ASTNode} node A CatchClause node.
         * @returns {void}
         * @private
         */
        function checkCatchClause(node) {
            const previousToken = sourceCode.getTokenBefore(node),
                firstToken = sourceCode.getFirstToken(node);

            checkBlock("body")(node);

            if (isBlock(node.body)) {
                if (style === "1tbs") {
                    if (previousToken.loc.start.line !== firstToken.loc.start.line) {
                        reportExtraNewline(node, CLOSE_MESSAGE, previousToken);
                    }
                } else {
                    if (previousToken.loc.start.line === firstToken.loc.start.line) {
                        context.report({
                            node,
                            message: CLOSE_MESSAGE_STROUSTRUP_ALLMAN,
                            fix: fixer => fixer.insertTextAfter(previousToken, "\n")
                        });
                    }
                }
            }
        }

        /**
         * Enforces the configured brace style on SwitchStatements
         * @param {ASTNode} node A SwitchStatement node.
         * @returns {void}
         * @private
         */
        function checkSwitchStatement(node) {
            let tokens;

            if (node.cases && node.cases.length) {
                tokens = sourceCode.getTokensBefore(node.cases[0], 2);
            } else {
                tokens = sourceCode.getLastTokens(node, 3);
            }

            if (style !== "allman" && tokens[0].loc.start.line !== tokens[1].loc.start.line) {
                reportExtraNewline(node, OPEN_MESSAGE, tokens[0]);
            } else if (style === "allman" && tokens[0].loc.start.line === tokens[1].loc.start.line) {
                context.report({
                    node,
                    message: OPEN_MESSAGE_ALLMAN,
                    fix: fixer => fixer.insertTextBefore(tokens[1], "\n")
                });
            }
        }

        //--------------------------------------------------------------------------
        // Public API
        //--------------------------------------------------------------------------

        return {
            FunctionDeclaration: checkBlock("body"),
            FunctionExpression: checkBlock("body"),
            ArrowFunctionExpression: checkBlock("body"),
            IfStatement: checkIfStatement,
            TryStatement: checkTryStatement,
            CatchClause: checkCatchClause,
            DoWhileStatement: checkBlock("body"),
            WhileStatement: checkBlock("body"),
            WithStatement: checkBlock("body"),
            ForStatement: checkBlock("body"),
            ForInStatement: checkBlock("body"),
            ForOfStatement: checkBlock("body"),
            SwitchStatement: checkSwitchStatement
        };

    }
};
