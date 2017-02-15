/**
 * @fileoverview Rule to flag use of certain node types
 * @author Burak Yigit Kaya
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const nodeTypes = require("espree").Syntax;

module.exports = {
    meta: {
        docs: {
            description: "disallow specified syntax",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: {
            type: "array",
            items: [
                {
                    enum: Object.keys(nodeTypes).map(k => nodeTypes[k])
                }
            ],
            uniqueItems: true,
            minItems: 0
        }
    },

    create(context) {

        /**
         * Generates a warning from the provided node, saying that node type is not allowed.
         * @param {ASTNode} node The node to warn on
         * @returns {void}
         */
        function warn(node) {
            context.report({ node, message: "Using '{{type}}' is not allowed.", data: node });
        }

        return context.options.reduce((result, nodeType) => {
            result[nodeType] = warn;

            return result;
        }, {});

    }
};
