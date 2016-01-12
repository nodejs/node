/**
 * @fileoverview Rule to flag use of certain node types
 * @author Burak Yigit Kaya
 * @copyright 2015 Burak Yigit Kaya. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var nodeTypes = require("espree").Syntax;

module.exports = function(context) {
    /**
     * Generates a warning from the provided node, saying that node type is not allowed.
     * @param {ASTNode} node The node to warn on
     * @returns {void}
     */
    function warn(node) {
        context.report(node, "Using \"{{type}}\" is not allowed.", node);
    }

    return context.options.reduce(function(result, nodeType) {
        result[nodeType] = warn;

        return result;
    }, {});

};

module.exports.schema = {
    "type": "array",
    "items": [
        {
            "enum": [0, 1, 2]
        },
        {
            "enum": Object.keys(nodeTypes).map(function(k) {
                return nodeTypes[k];
            })
        }
    ],
    "uniqueItems": true,
    "minItems": 1
};
