/**
 * @fileoverview Rule to flag use constant conditions
 * @author Christian Schulz <http://rndm.de>
 * @copyright 2014 Christian Schulz. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Checks if a node has a constant truthiness value.
     * @param {ASTNode} node The AST node to check.
     * @returns {Bool} true when node's truthiness is constant
     * @private
     */
    function isConstant(node) {
        switch (node.type) {
            case "Literal":
            case "ArrowFunctionExpression":
            case "FunctionExpression":
            case "ObjectExpression":
            case "ArrayExpression":
                return true;
            case "UnaryExpression":
                return isConstant(node.argument);
            case "BinaryExpression":
            case "LogicalExpression":
                return isConstant(node.left) && isConstant(node.right);
            case "AssignmentExpression":
                return (node.operator === "=") && isConstant(node.right);
            case "SequenceExpression":
                return isConstant(node.expressions[node.expressions.length - 1]);
            // no default
        }
        return false;
    }

    /**
     * Reports when the given node contains a constant condition.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     * @private
     */
    function checkConstantCondition(node) {
        if (node.test && isConstant(node.test)) {
            context.report(node, "Unexpected constant condition.");
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "ConditionalExpression": checkConstantCondition,
        "IfStatement": checkConstantCondition,
        "WhileStatement": checkConstantCondition,
        "DoWhileStatement": checkConstantCondition,
        "ForStatement": checkConstantCondition
    };

};
