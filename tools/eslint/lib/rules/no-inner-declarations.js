/**
 * @fileoverview Rule to enforce declarations in program or function body root.
 * @author Brandon Mills
 * @copyright 2014 Brandon Mills. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Find the nearest Program or Function ancestor node.
     * @returns {Object} Ancestor's type and distance from node.
     */
    function nearestBody() {
        var ancestors = context.getAncestors(),
            ancestor = ancestors.pop(),
            generation = 1;

        while (ancestor && ["Program", "FunctionDeclaration",
                "FunctionExpression", "ArrowFunctionExpression"
                ].indexOf(ancestor.type) < 0) {
            generation += 1;
            ancestor = ancestors.pop();
        }

        return {
            // Type of containing ancestor
            type: ancestor.type,
            // Separation between ancestor and node
            distance: generation
        };
    }

    /**
     * Ensure that a given node is at a program or function body's root.
     * @param {ASTNode} node Declaration node to check.
     * @returns {void}
     */
    function check(node) {
        var body = nearestBody(node),
            valid = ((body.type === "Program" && body.distance === 1) ||
                body.distance === 2);

        if (!valid) {
            context.report(node, "Move {{type}} declaration to {{body}} root.",
                {
                    type: (node.type === "FunctionDeclaration" ?
                        "function" : "variable"),
                    body: (body.type === "Program" ?
                        "program" : "function body")
                }
            );
        }
    }

    return {

        "FunctionDeclaration": check,
        "VariableDeclaration": function(node) {
            if (context.options[0] === "both" && node.kind === "var") {
                check(node);
            }
        }

    };

};

module.exports.schema = [
    {
        "enum": ["functions", "both"]
    }
];
