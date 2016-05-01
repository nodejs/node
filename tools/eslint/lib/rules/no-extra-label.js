/**
 * @fileoverview Rule to disallow unnecessary labels
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow unnecessary labels",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        var scopeInfo = null;

        /**
         * Creates a new scope with a breakable statement.
         *
         * @param {ASTNode} node - A node to create. This is a BreakableStatement.
         * @returns {void}
         */
        function enterBreakableStatement(node) {
            scopeInfo = {
                label: astUtils.getLabel(node),
                breakable: true,
                upper: scopeInfo
            };
        }

        /**
         * Removes the top scope of the stack.
         *
         * @returns {void}
         */
        function exitBreakableStatement() {
            scopeInfo = scopeInfo.upper;
        }

        /**
         * Creates a new scope with a labeled statement.
         *
         * This ignores it if the body is a breakable statement.
         * In this case it's handled in the `enterBreakableStatement` function.
         *
         * @param {ASTNode} node - A node to create. This is a LabeledStatement.
         * @returns {void}
         */
        function enterLabeledStatement(node) {
            if (!astUtils.isBreakableStatement(node.body)) {
                scopeInfo = {
                    label: node.label.name,
                    breakable: false,
                    upper: scopeInfo
                };
            }
        }

        /**
         * Removes the top scope of the stack.
         *
         * This ignores it if the body is a breakable statement.
         * In this case it's handled in the `exitBreakableStatement` function.
         *
         * @param {ASTNode} node - A node. This is a LabeledStatement.
         * @returns {void}
         */
        function exitLabeledStatement(node) {
            if (!astUtils.isBreakableStatement(node.body)) {
                scopeInfo = scopeInfo.upper;
            }
        }

        /**
         * Reports a given control node if it's unnecessary.
         *
         * @param {ASTNode} node - A node. This is a BreakStatement or a
         *      ContinueStatement.
         * @returns {void}
         */
        function reportIfUnnecessary(node) {
            if (!node.label) {
                return;
            }

            var labelNode = node.label;
            var label = labelNode.name;
            var info = scopeInfo;

            while (info) {
                if (info.breakable || info.label === label) {
                    if (info.breakable && info.label === label) {
                        context.report({
                            node: labelNode,
                            message: "This label '{{name}}' is unnecessary.",
                            data: labelNode
                        });
                    }
                    return;
                }

                info = info.upper;
            }
        }

        return {
            WhileStatement: enterBreakableStatement,
            "WhileStatement:exit": exitBreakableStatement,
            DoWhileStatement: enterBreakableStatement,
            "DoWhileStatement:exit": exitBreakableStatement,
            ForStatement: enterBreakableStatement,
            "ForStatement:exit": exitBreakableStatement,
            ForInStatement: enterBreakableStatement,
            "ForInStatement:exit": exitBreakableStatement,
            ForOfStatement: enterBreakableStatement,
            "ForOfStatement:exit": exitBreakableStatement,
            SwitchStatement: enterBreakableStatement,
            "SwitchStatement:exit": exitBreakableStatement,
            LabeledStatement: enterLabeledStatement,
            "LabeledStatement:exit": exitLabeledStatement,
            BreakStatement: reportIfUnnecessary,
            ContinueStatement: reportIfUnnecessary
        };
    }
};
