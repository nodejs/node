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
        ]
    },

    create: function(context) {
        let style = context.options[0] || "1tbs",
            params = context.options[1] || {},
            sourceCode = context.getSourceCode();

        let OPEN_MESSAGE = "Opening curly brace does not appear on the same line as controlling statement.",
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
         * Binds a list of properties to a function that verifies that the opening
         * curly brace is on the same line as its controlling statement of a given
         * node.
         * @param {...string} The properties to check on the node.
         * @returns {Function} A function that will perform the check on a node
         * @private
         */
        function checkBlock() {
            let blockProperties = arguments;

            return function(node) {
                Array.prototype.forEach.call(blockProperties, function(blockProp) {
                    let block = node[blockProp],
                        previousToken,
                        curlyToken,
                        curlyTokenEnd,
                        allOnSameLine;

                    if (!isBlock(block)) {
                        return;
                    }

                    previousToken = sourceCode.getTokenBefore(block);
                    curlyToken = sourceCode.getFirstToken(block);
                    curlyTokenEnd = sourceCode.getLastToken(block);
                    allOnSameLine = previousToken.loc.start.line === curlyTokenEnd.loc.start.line;

                    if (allOnSameLine && params.allowSingleLine) {
                        return;
                    }

                    if (style !== "allman" && previousToken.loc.start.line !== curlyToken.loc.start.line) {
                        context.report(node, OPEN_MESSAGE);
                    } else if (style === "allman" && previousToken.loc.start.line === curlyToken.loc.start.line) {
                        context.report(node, OPEN_MESSAGE_ALLMAN);
                    }

                    if (!block.body.length) {
                        return;
                    }

                    if (curlyToken.loc.start.line === block.body[0].loc.start.line) {
                        context.report(block.body[0], BODY_MESSAGE);
                    }

                    if (curlyTokenEnd.loc.start.line === block.body[block.body.length - 1].loc.start.line) {
                        context.report(block.body[block.body.length - 1], CLOSE_MESSAGE_SINGLE);
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
            let tokens;

            checkBlock("consequent", "alternate")(node);

            if (node.alternate) {

                tokens = sourceCode.getTokensBefore(node.alternate, 2);

                if (style === "1tbs") {
                    if (tokens[0].loc.start.line !== tokens[1].loc.start.line &&
                        node.consequent.type === "BlockStatement" &&
                        isCurlyPunctuator(tokens[0])) {
                        context.report(node.alternate, CLOSE_MESSAGE);
                    }
                } else if (tokens[0].loc.start.line === tokens[1].loc.start.line) {
                    context.report(node.alternate, CLOSE_MESSAGE_STROUSTRUP_ALLMAN);
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
            let tokens;

            checkBlock("block", "finalizer")(node);

            if (isBlock(node.finalizer)) {
                tokens = sourceCode.getTokensBefore(node.finalizer, 2);
                if (style === "1tbs") {
                    if (tokens[0].loc.start.line !== tokens[1].loc.start.line) {
                        context.report(node.finalizer, CLOSE_MESSAGE);
                    }
                } else if (tokens[0].loc.start.line === tokens[1].loc.start.line) {
                    context.report(node.finalizer, CLOSE_MESSAGE_STROUSTRUP_ALLMAN);
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
            let previousToken = sourceCode.getTokenBefore(node),
                firstToken = sourceCode.getFirstToken(node);

            checkBlock("body")(node);

            if (isBlock(node.body)) {
                if (style === "1tbs") {
                    if (previousToken.loc.start.line !== firstToken.loc.start.line) {
                        context.report(node, CLOSE_MESSAGE);
                    }
                } else {
                    if (previousToken.loc.start.line === firstToken.loc.start.line) {
                        context.report(node, CLOSE_MESSAGE_STROUSTRUP_ALLMAN);
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
                context.report(node, OPEN_MESSAGE);
            } else if (style === "allman" && tokens[0].loc.start.line === tokens[1].loc.start.line) {
                context.report(node, OPEN_MESSAGE_ALLMAN);
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
