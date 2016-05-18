/**
 * @fileoverview Rule to disallow unused labels.
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow unused labels",
            category: "Best Practices",
            recommended: true
        },

        schema: []
    },

    create: function(context) {
        var scopeInfo = null;

        /**
         * Adds a scope info to the stack.
         *
         * @param {ASTNode} node - A node to add. This is a LabeledStatement.
         * @returns {void}
         */
        function enterLabeledScope(node) {
            scopeInfo = {
                label: node.label.name,
                used: false,
                upper: scopeInfo
            };
        }

        /**
         * Removes the top of the stack.
         * At the same time, this reports the label if it's never used.
         *
         * @param {ASTNode} node - A node to report. This is a LabeledStatement.
         * @returns {void}
         */
        function exitLabeledScope(node) {
            if (!scopeInfo.used) {
                context.report({
                    node: node.label,
                    message: "'{{name}}:' is defined but never used.",
                    data: node.label
                });
            }

            scopeInfo = scopeInfo.upper;
        }

        /**
         * Marks the label of a given node as used.
         *
         * @param {ASTNode} node - A node to mark. This is a BreakStatement or
         *      ContinueStatement.
         * @returns {void}
         */
        function markAsUsed(node) {
            if (!node.label) {
                return;
            }

            var label = node.label.name;
            var info = scopeInfo;

            while (info) {
                if (info.label === label) {
                    info.used = true;
                    break;
                }
                info = info.upper;
            }
        }

        return {
            LabeledStatement: enterLabeledScope,
            "LabeledStatement:exit": exitLabeledScope,
            BreakStatement: markAsUsed,
            ContinueStatement: markAsUsed
        };
    }
};
