/**
 * @fileoverview Rule to flag when using new Function
 * @author Ilya Volodin
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
     * Checks if the callee is the Function constructor, and if so, reports an issue.
     * @param {ASTNode} node The node to check and report on
     * @returns {void}
     * @private
     */
    function validateCallee(node) {
        if (node.callee.name === "Function") {
            context.report(node, "The Function constructor is eval.");
        }
    }

    return {
        "NewExpression": validateCallee,
        "CallExpression": validateCallee
    };

};

module.exports.schema = [];
