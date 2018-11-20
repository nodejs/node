/**
 * @fileoverview Rule to flag use of implied eval via setTimeout and setInterval
 * @author James Allardice
 * @copyright 2015 Mathias Schreck. All rights reserved.
 * @copyright 2013 James Allardice. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var IMPLIED_EVAL = /set(?:Timeout|Interval)/;

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Checks if the first argument of a given CallExpression node is a string literal.
     * @param {ASTNode} node The CallExpression node the check.
     * @returns {boolean} True if the first argument is a string literal, false if not.
     */
    function hasStringLiteralArgument(node) {
        var firstArgument = node.arguments[0];

        return firstArgument && firstArgument.type === "Literal" && typeof firstArgument.value === "string";
    }

    /**
     * Checks if the given MemberExpression node is window.setTimeout or window.setInterval.
     * @param {ASTNode} node The MemberExpression node to check.
     * @returns {boolean} Whether or not the given node is window.set*.
     */
    function isSetMemberExpression(node) {
        var object = node.object,
            property = node.property,
            hasSetPropertyName = IMPLIED_EVAL.test(property.name) || IMPLIED_EVAL.test(property.value);

        return object.name === "window" && hasSetPropertyName;

    }

    /**
     * Determines if a node represents a call to setTimeout/setInterval with
     * a string argument.
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node matches, false if not.
     * @private
     */
    function isImpliedEval(node) {
        var isMemberExpression = (node.callee.type === "MemberExpression"),
            isIdentifier = (node.callee.type === "Identifier"),
            isSetMethod = (isIdentifier && IMPLIED_EVAL.test(node.callee.name)) ||
                (isMemberExpression && isSetMemberExpression(node.callee));

        return isSetMethod && hasStringLiteralArgument(node);
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "CallExpression": function(node) {
            if (isImpliedEval(node)) {
                context.report(node, "Implied eval. Consider passing a function instead of a string.");
            }
        }
    };

};
