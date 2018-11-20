/**
 * @fileoverview Rule to flag `else` after a `return` in `if`
 * @author Ian Christian Myers
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
     * Display the context report if rule is violated
     *
     * @param {Node} node The 'else' node
     * @returns {void}
     */
    function displayReport(node) {
        context.report(node, "Unexpected 'else' after 'return'.");
    }

    /**
     * Check to see if the node is a ReturnStatement
     *
     * @param {Node} node The node being evaluated
     * @returns {boolean} True if node is a return
     */
    function checkForReturn(node) {
        return node.type === "ReturnStatement";
    }

    /**
     * Naive return checking, does not iterate through the whole
     * BlockStatement because we make the assumption that the ReturnStatement
     * will be the last node in the body of the BlockStatement.
     *
     * @param {Node} node The consequent/alternate node
     * @returns {boolean} True if it has a return
     */
    function naiveHasReturn(node) {
        if (node.type === "BlockStatement") {
            var body = node.body,
                lastChildNode = body[body.length - 1];

            return lastChildNode && checkForReturn(lastChildNode);
        }
        return checkForReturn(node);
    }

    /**
     * Check to see if the node is valid for evaluation,
     * meaning it has an else and not an else-if
     *
     * @param {Node} node The node being evaluated
     * @returns {boolean} True if the node is valid
     */
    function hasElse(node) {
        return node.alternate && node.consequent && node.alternate.type !== "IfStatement";
    }

    /**
     * If the consequent is an IfStatement, check to see if it has an else
     * and both its consequent and alternate path return, meaning this is
     * a nested case of rule violation.  If-Else not considered currently.
     *
     * @param {Node} node The consequent node
     * @returns {boolean} True if this is a nested rule violation
     */
    function checkForIf(node) {
        return node.type === "IfStatement" && hasElse(node) &&
            naiveHasReturn(node.alternate) && naiveHasReturn(node.consequent);
    }

    /**
     * Check the consequent/body node to make sure it is not
     * a ReturnStatement or an IfStatement that returns on both
     * code paths.  If it is, display the context report.
     *
     * @param {Node} node The consequent or body node
     * @param {Node} alternate The alternate node
     * @returns {void}
     */
    function checkForReturnOrIf(node, alternate) {
        if (checkForReturn(node) || checkForIf(node)) {
            displayReport(alternate);
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "IfStatement": function (node) {
            // Don't bother finding a ReturnStatement, if there's no `else`
            // or if the alternate is also an if (indicating an else if).
            if (hasElse(node)) {
                var consequent = node.consequent,
                    alternate = node.alternate;
                // If we have a BlockStatement, check each consequent body node.
                if (consequent.type === "BlockStatement") {
                    var body = consequent.body;
                    body.forEach(function (bodyNode) {
                        checkForReturnOrIf(bodyNode, alternate);
                    });
                // If not a block statement, make sure the consequent isn't a ReturnStatement
                // or an IfStatement with returns on both paths
                } else {
                    checkForReturnOrIf(consequent, alternate);
                }
            }
        }

    };

};
