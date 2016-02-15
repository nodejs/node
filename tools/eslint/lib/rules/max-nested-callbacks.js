/**
 * @fileoverview Rule to enforce a maximum number of nested callbacks.
 * @author Ian Christian Myers
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------

    var THRESHOLD = context.options[0] || 10;

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    var callbackStack = [];

    /**
     * Checks a given function node for too many callbacks.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkFunction(node) {
        var parent = node.parent;

        if (parent.type === "CallExpression") {
            callbackStack.push(node);
        }

        if (callbackStack.length > THRESHOLD) {
            var opts = {num: callbackStack.length, max: THRESHOLD};
            context.report(node, "Too many nested callbacks ({{num}}). Maximum allowed is {{max}}.", opts);
        }
    }

    /**
     * Pops the call stack.
     * @returns {void}
     * @private
     */
    function popStack() {
        callbackStack.pop();
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "ArrowFunctionExpression": checkFunction,
        "ArrowFunctionExpression:exit": popStack,

        "FunctionExpression": checkFunction,
        "FunctionExpression:exit": popStack
    };

};

module.exports.schema = [
    {
        "type": "integer",
        "minimum": 0
    }
];
