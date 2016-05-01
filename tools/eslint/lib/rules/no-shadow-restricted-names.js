/**
 * @fileoverview Disallow shadowing of NaN, undefined, and Infinity (ES5 section 15.1.1)
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow identifiers from shadowing restricted names",
            category: "Variables",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        var RESTRICTED = ["undefined", "NaN", "Infinity", "arguments", "eval"];

        /**
         * Check if the node name is present inside the restricted list
         * @param {ASTNode} id id to evaluate
         * @returns {void}
         * @private
         */
        function checkForViolation(id) {
            if (RESTRICTED.indexOf(id.name) > -1) {
                context.report(id, "Shadowing of global property '" + id.name + "'.");
            }
        }

        return {
            VariableDeclarator: function(node) {
                checkForViolation(node.id);
            },
            ArrowFunctionExpression: function(node) {
                [].map.call(node.params, checkForViolation);
            },
            FunctionExpression: function(node) {
                if (node.id) {
                    checkForViolation(node.id);
                }
                [].map.call(node.params, checkForViolation);
            },
            FunctionDeclaration: function(node) {
                if (node.id) {
                    checkForViolation(node.id);
                    [].map.call(node.params, checkForViolation);
                }
            },
            CatchClause: function(node) {
                checkForViolation(node.param);
            }
        };

    }
};
