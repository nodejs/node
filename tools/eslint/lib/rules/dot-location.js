/**
 * @fileoverview Validates newlines before and after dots
 * @author Greg Cochard
 */

"use strict";

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce consistent newlines before and after dots",
            category: "Best Practices",
            recommended: false
        },

        schema: [
            {
                enum: ["object", "property"]
            }
        ]
    },

    create: function(context) {

        let config = context.options[0];

        // default to onObject if no preference is passed
        let onObject = config === "object" || !config;

        let sourceCode = context.getSourceCode();

        /**
         * Reports if the dot between object and property is on the correct loccation.
         * @param {ASTNode} obj The object owning the property.
         * @param {ASTNode} prop The property of the object.
         * @param {ASTNode} node The corresponding node of the token.
         * @returns {void}
         */
        function checkDotLocation(obj, prop, node) {
            let dot = sourceCode.getTokenBefore(prop);

            if (dot.type === "Punctuator" && dot.value === ".") {
                if (onObject) {
                    if (!astUtils.isTokenOnSameLine(obj, dot)) {
                        context.report(node, dot.loc.start, "Expected dot to be on same line as object.");
                    }
                } else if (!astUtils.isTokenOnSameLine(dot, prop)) {
                    context.report(node, dot.loc.start, "Expected dot to be on same line as property.");
                }
            }
        }

        /**
         * Checks the spacing of the dot within a member expression.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         */
        function checkNode(node) {
            checkDotLocation(node.object, node.property, node);
        }

        return {
            MemberExpression: checkNode
        };
    }
};
