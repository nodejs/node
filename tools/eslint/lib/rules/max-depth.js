/**
 * @fileoverview A rule to set the maximum depth block can be nested in a function.
 * @author Ian Christian Myers
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    var functionStack = [],
        maxDepth = context.options[0] || 4;

    /**
     * When parsing a new function, store it in our function stack
     * @returns {void}
     * @private
     */
    function startFunction() {
        functionStack.push(0);
    }

    /**
     * When parsing is done then pop out the reference
     * @returns {void}
     * @private
     */
    function endFunction() {
        functionStack.pop();
    }

    /**
     * Save the block and Evaluate the node
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function pushBlock(node) {
        var len = ++functionStack[functionStack.length - 1];

        if (len > maxDepth) {
            context.report(node, "Blocks are nested too deeply ({{depth}}).",
                    { depth: len });
        }
    }

    /**
     * Pop the saved block
     * @returns {void}
     * @private
     */
    function popBlock() {
        functionStack[functionStack.length - 1]--;
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "Program": startFunction,
        "FunctionDeclaration": startFunction,
        "FunctionExpression": startFunction,
        "ArrowFunctionExpression": startFunction,

        "IfStatement": function(node) {
            if (node.parent.type !== "IfStatement") {
                pushBlock(node);
            }
        },
        "SwitchStatement": pushBlock,
        "TryStatement": pushBlock,
        "DoWhileStatement": pushBlock,
        "WhileStatement": pushBlock,
        "WithStatement": pushBlock,
        "ForStatement": pushBlock,
        "ForInStatement": pushBlock,
        "ForOfStatement": pushBlock,

        "IfStatement:exit": popBlock,
        "SwitchStatement:exit": popBlock,
        "TryStatement:exit": popBlock,
        "DoWhileStatement:exit": popBlock,
        "WhileStatement:exit": popBlock,
        "WithStatement:exit": popBlock,
        "ForStatement:exit": popBlock,
        "ForInStatement:exit": popBlock,
        "ForOfStatement:exit": popBlock,

        "FunctionDeclaration:exit": endFunction,
        "FunctionExpression:exit": endFunction,
        "ArrowFunctionExpression:exit": endFunction,
        "Program:exit": endFunction
    };

};

module.exports.schema = [
    {
        "type": "integer"
    }
];
