/**
 * @fileoverview Rule to require braces in arrow function body.
 * @author Alberto Rodríguez
 * @copyright 2015 Alberto Rodríguez. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var always = context.options[0] === "always";
    var asNeeded = !context.options[0] || context.options[0] === "as-needed";

    /**
     * Determines whether a arrow function body needs braces
     * @param {ASTNode} node The arrow function node.
     * @returns {void}
     */
    function validate(node) {
        var arrowBody = node.body;

        if (arrowBody.type === "BlockStatement") {
            var blockBody = arrowBody.body;

            if (blockBody.length !== 1) {
                return;
            }

            if (asNeeded && blockBody[0].type === "ReturnStatement") {
                context.report({
                    node: node,
                    loc: arrowBody.loc.start,
                    message: "Unexpected block statement surrounding arrow body."
                });
            }
        } else {
            if (always) {
                context.report({
                    node: node,
                    loc: arrowBody.loc.start,
                    message: "Expected block statement surrounding arrow body."
                });
            }
        }
    }

    return {
        "ArrowFunctionExpression": validate
    };
};

module.exports.schema = [
    {
        "enum": ["always", "as-needed"]
    }
];
