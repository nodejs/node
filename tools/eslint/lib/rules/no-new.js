/**
 * @fileoverview Rule to flag statements with function invocation preceded by
 * "new" and not part of assignment
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "ExpressionStatement": function(node) {

            if (node.expression.type === "NewExpression") {
                context.report(node, "Do not use 'new' for side effects.");
            }
        }
    };

};

module.exports.schema = [];
