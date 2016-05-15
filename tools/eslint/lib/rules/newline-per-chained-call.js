/**
 * @fileoverview Rule to ensure newline per method call when chaining calls
 * @author Rajendra Patil
 * @author Burak Yigit Kaya
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require a newline after each call in a method chain",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [{
            type: "object",
            properties: {
                ignoreChainWithDepth: {
                    type: "integer",
                    minimum: 1,
                    maximum: 10
                }
            },
            additionalProperties: false
        }]
    },

    create: function(context) {

        var options = context.options[0] || {},
            ignoreChainWithDepth = options.ignoreChainWithDepth || 2;

        var sourceCode = context.getSourceCode();

        return {
            "CallExpression:exit": function(node) {
                if (!node.callee || node.callee.type !== "MemberExpression") {
                    return;
                }

                var callee = node.callee;
                var parent = callee.object;
                var depth = 1;

                while (parent && parent.callee) {
                    depth += 1;
                    parent = parent.callee.object;
                }

                if (depth > ignoreChainWithDepth && callee.property.loc.start.line === callee.object.loc.end.line) {
                    context.report(
                        callee.property,
                        callee.property.loc.start,
                        "Expected line break after `" + sourceCode.getText(callee.object).replace(/\r\n|\r|\n/g, "\\n") + "`."
                    );
                }
            }
        };
    }
};
