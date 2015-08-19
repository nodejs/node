/**
 * @fileoverview Rule to enforce consistent spacing after function names
 * @author Roberto Vidal
 * @copyright 2014 Roberto Vidal. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var requiresSpace = context.options[0] === "always";

    /**
     * Reports if the give named function node has the correct spacing after its name
     *
     * @param {ASTNode} node  The node to which the potential problem belongs.
     * @returns {void}
     */
    function check(node) {
        var tokens = context.getFirstTokens(node, 3),
            hasSpace = tokens[1].range[1] < tokens[2].range[0];

        if (hasSpace !== requiresSpace) {
            context.report(node, "Function name \"{{name}}\" must {{not}}be followed by whitespace.", {
                name: node.id.name,
                not: requiresSpace ? "" : "not "
            });
        }
    }

    return {
        "FunctionDeclaration": check,
        "FunctionExpression": function (node) {
            if (node.id) {
                check(node);
            }
        }
    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    }
];
