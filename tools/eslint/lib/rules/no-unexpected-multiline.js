/**
 * @fileoverview Rule to spot scenarios where a newline looks like it is ending a statement, but is not.
 * @author Glen Mailer
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
module.exports = {
    meta: {
        docs: {
            description: "disallow confusing multiline expressions",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        var FUNCTION_MESSAGE = "Unexpected newline between function and ( of function call.";
        var PROPERTY_MESSAGE = "Unexpected newline between object and [ of property access.";
        var TAGGED_TEMPLATE_MESSAGE = "Unexpected newline between template tag and template literal.";

        /**
         * Check to see if there is a newline between the node and the following open bracket
         * line's expression
         * @param {ASTNode} node The node to check.
         * @param {string} msg The error message to use.
         * @returns {void}
         * @private
         */
        function checkForBreakAfter(node, msg) {
            var nodeExpressionEnd = node;
            var openParen = context.getTokenAfter(node);

            // Move along until the end of the wrapped expression
            while (openParen.value === ")") {
                nodeExpressionEnd = openParen;
                openParen = context.getTokenAfter(nodeExpressionEnd);
            }

            if (openParen.loc.start.line !== nodeExpressionEnd.loc.end.line) {
                context.report(node, openParen.loc.start, msg, { char: openParen.value });
            }
        }

        //--------------------------------------------------------------------------
        // Public API
        //--------------------------------------------------------------------------

        return {

            MemberExpression: function(node) {
                if (!node.computed) {
                    return;
                }
                checkForBreakAfter(node.object, PROPERTY_MESSAGE);
            },

            TaggedTemplateExpression: function(node) {
                if (node.tag.loc.end.line === node.quasi.loc.start.line) {
                    return;
                }
                context.report(node, node.loc.start, TAGGED_TEMPLATE_MESSAGE);
            },

            CallExpression: function(node) {
                if (node.arguments.length === 0) {
                    return;
                }
                checkForBreakAfter(node.callee, FUNCTION_MESSAGE);
            }
        };

    }
};
