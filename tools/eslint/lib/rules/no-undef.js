/**
 * @fileoverview Rule to flag references to undeclared variables.
 * @author Mark Macdonald
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Mark Macdonald. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks if the given node is the argument of a typeof operator.
 * @param {ASTNode} node The AST node being checked.
 * @returns {boolean} Whether or not the node is the argument of a typeof operator.
 */
function hasTypeOfOperator(node) {
    var parent = node.parent;

    return parent.type === "UnaryExpression" && parent.operator === "typeof";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options[0];
    var considerTypeOf = options && options.typeof === true || false;

    return {
        "Program:exit": function(/* node */) {
            var globalScope = context.getScope();

            globalScope.through.forEach(function(ref) {
                var identifier = ref.identifier;

                if (!considerTypeOf && hasTypeOfOperator(identifier)) {
                    return;
                }

                context.report({
                    node: identifier,
                    message: "'{{name}}' is not defined.",
                    data: identifier
                });
            });
        }
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "typeof": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
