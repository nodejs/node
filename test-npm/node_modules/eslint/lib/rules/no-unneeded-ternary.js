/**
 * @fileoverview Rule to flag no-unneeded-ternary
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 * @copyright 2015 Michael Ficarra. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options[0] || {};
    var defaultAssignment = options.defaultAssignment !== false;

    /**
     * Test if the node is a boolean literal
     * @param {ASTNode} node - The node to report.
     * @returns {boolean} True if the its a boolean literal
     * @private
     */
    function isBooleanLiteral(node) {
        return node.type === "Literal" && typeof node.value === "boolean";
    }

    /**
     * Test if the node matches the pattern id ? id : expression
     * @param {ASTNode} node - The ConditionalExpression to check.
     * @returns {boolean} True if the pattern is matched, and false otherwise
     * @private
     */
    function matchesDefaultAssignment(node) {
        return node.test.type === "Identifier" &&
               node.consequent.type === "Identifier" &&
               node.test.name === node.consequent.name;
    }

    return {

        "ConditionalExpression": function(node) {
            if (isBooleanLiteral(node.alternate) && isBooleanLiteral(node.consequent)) {
                context.report(node, node.consequent.loc.start, "Unnecessary use of boolean literals in conditional expression");
            } else if (!defaultAssignment && matchesDefaultAssignment(node)) {
                context.report(node, node.consequent.loc.start, "Unnecessary use of conditional expression for default assignment");
            }
        }
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "defaultAssignment": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
