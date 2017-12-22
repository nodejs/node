/**
 * @fileoverview disallow use of the Buffer() constructor
 * @author Teddy Katz
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow use of the Buffer() constructor",
            category: "Node.js and CommonJS",
            recommended: false
        },
        schema: []
    },

    create(context) {

        //----------------------------------------------------------------------
        // Public
        //----------------------------------------------------------------------

        return {
            "CallExpression[callee.name='Buffer'], NewExpression[callee.name='Buffer']"(node) {
                context.report({
                    node,
                    message: "{{example}} is deprecated. Use Buffer.from(), Buffer.alloc(), or Buffer.allocUnsafe() instead.",
                    data: { example: node.type === "CallExpression" ? "Buffer()" : "new Buffer()" }
                });
            }
        };
    }
};
