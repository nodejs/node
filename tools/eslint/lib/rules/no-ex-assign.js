/**
 * @fileoverview Rule to flag assignment of the exception parameter
 * @author Stephen Murray <spmurrayzzz>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var catchStack = [];

    return {

        "CatchClause": function(node) {
            catchStack.push(node.param.name);
        },

        "CatchClause:exit": function() {
            catchStack.pop();
        },

        "AssignmentExpression": function(node) {

            if (catchStack.length > 0) {

                var exceptionName = catchStack[catchStack.length - 1];

                if (node.left.name && node.left.name === exceptionName) {
                    context.report(node, "Do not assign to the exception parameter.");
                }
            }
        }

    };

};
