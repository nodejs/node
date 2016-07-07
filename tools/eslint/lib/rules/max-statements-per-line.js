/**
 * @fileoverview Specify the maximum number of statements allowed per line.
 * @author Kenneth Williams
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce a maximum number of statements allowed per line",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    max: {
                        type: "integer",
                        minimum: 0
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        var sourceCode = context.getSourceCode(),
            options = context.options[0] || {},
            lastStatementLine = 0,
            numberOfStatementsOnThisLine = 0,
            maxStatementsPerLine = typeof options.max !== "undefined" ? options.max : 1,
            message = "This line has too many statements. Maximum allowed is " + maxStatementsPerLine + ".";

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        var SINGLE_CHILD_ALLOWED = /^(?:(?:DoWhile|For|ForIn|ForOf|If|Labeled|While)Statement|Export(?:Default|Named)Declaration)$/;

        /**
         * Gets the actual last token of a given node.
         *
         * @param {ASTNode} node - A node to get. This is a node except EmptyStatement.
         * @returns {Token} The actual last token.
         */
        function getActualLastToken(node) {
            var lastToken = sourceCode.getLastToken(node);

            if (lastToken.value === ";") {
                lastToken = sourceCode.getTokenBefore(lastToken);
            }
            return lastToken;
        }

        /**
         * Addresses a given node.
         * It updates the state of this rule, then reports the node if the node violated this rule.
         *
         * @param {ASTNode} node - A node to check.
         * @returns {void}
         */
        function enterStatement(node) {
            var line = node.loc.start.line;

            // Skip to allow non-block statements if this is direct child of control statements.
            // `if (a) foo();` is counted as 1.
            // But `if (a) foo(); else foo();` should be counted as 2.
            if (SINGLE_CHILD_ALLOWED.test(node.parent.type) &&
                node.parent.alternate !== node
            ) {
                return;
            }

            // Update state.
            if (line === lastStatementLine) {
                numberOfStatementsOnThisLine += 1;
            } else {
                numberOfStatementsOnThisLine = 1;
                lastStatementLine = line;
            }

            // Reports if the node violated this rule.
            if (numberOfStatementsOnThisLine === maxStatementsPerLine + 1) {
                context.report({node: node, message: message});
            }
        }

        /**
         * Updates the state of this rule with the end line of leaving node to check with the next statement.
         *
         * @param {ASTNode} node - A node to check.
         * @returns {void}
         */
        function leaveStatement(node) {
            var line = getActualLastToken(node).loc.end.line;

            // Update state.
            if (line !== lastStatementLine) {
                numberOfStatementsOnThisLine = 1;
                lastStatementLine = line;
            }
        }

        //--------------------------------------------------------------------------
        // Public API
        //--------------------------------------------------------------------------

        return {
            BreakStatement: enterStatement,
            ClassDeclaration: enterStatement,
            ContinueStatement: enterStatement,
            DebuggerStatement: enterStatement,
            DoWhileStatement: enterStatement,
            ExpressionStatement: enterStatement,
            ForInStatement: enterStatement,
            ForOfStatement: enterStatement,
            ForStatement: enterStatement,
            FunctionDeclaration: enterStatement,
            IfStatement: enterStatement,
            ImportDeclaration: enterStatement,
            LabeledStatement: enterStatement,
            ReturnStatement: enterStatement,
            SwitchStatement: enterStatement,
            ThrowStatement: enterStatement,
            TryStatement: enterStatement,
            VariableDeclaration: enterStatement,
            WhileStatement: enterStatement,
            WithStatement: enterStatement,
            ExportNamedDeclaration: enterStatement,
            ExportDefaultDeclaration: enterStatement,
            ExportAllDeclaration: enterStatement,

            "BreakStatement:exit": leaveStatement,
            "ClassDeclaration:exit": leaveStatement,
            "ContinueStatement:exit": leaveStatement,
            "DebuggerStatement:exit": leaveStatement,
            "DoWhileStatement:exit": leaveStatement,
            "ExpressionStatement:exit": leaveStatement,
            "ForInStatement:exit": leaveStatement,
            "ForOfStatement:exit": leaveStatement,
            "ForStatement:exit": leaveStatement,
            "FunctionDeclaration:exit": leaveStatement,
            "IfStatement:exit": leaveStatement,
            "ImportDeclaration:exit": leaveStatement,
            "LabeledStatement:exit": leaveStatement,
            "ReturnStatement:exit": leaveStatement,
            "SwitchStatement:exit": leaveStatement,
            "ThrowStatement:exit": leaveStatement,
            "TryStatement:exit": leaveStatement,
            "VariableDeclaration:exit": leaveStatement,
            "WhileStatement:exit": leaveStatement,
            "WithStatement:exit": leaveStatement,
            "ExportNamedDeclaration:exit": leaveStatement,
            "ExportDefaultDeclaration:exit": leaveStatement,
            "ExportAllDeclaration:exit": leaveStatement,

            // For backward compatibility.
            // Empty blocks should be warned if `{max: 0}` was given.
            BlockStatement: function reportIfZero(node) {
                if (maxStatementsPerLine === 0 && node.body.length === 0) {
                    context.report({node: node, message: message});
                }
            }
        };
    }
};
