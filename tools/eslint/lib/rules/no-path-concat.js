/**
 * @fileoverview Disallow string concatenation when using __dirname and __filename
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow string concatenation with `__dirname` and `__filename`",
            category: "Node.js and CommonJS",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        let MATCHER = /^__(?:dir|file)name$/;

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            BinaryExpression: function(node) {

                let left = node.left,
                    right = node.right;

                if (node.operator === "+" &&
                        ((left.type === "Identifier" && MATCHER.test(left.name)) ||
                        (right.type === "Identifier" && MATCHER.test(right.name)))
                ) {

                    context.report(node, "Use path.join() or path.resolve() instead of + to create paths.");
                }
            }

        };

    }
};
