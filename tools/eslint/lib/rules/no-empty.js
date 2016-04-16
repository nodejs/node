/**
 * @fileoverview Rule to flag use of an empty block statement
 * @author Nicholas C. Zakas
 * @copyright Nicholas C. Zakas. All rights reserved.
 * @copyright 2015 Dieter Oberkofler. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var FUNCTION_TYPE = /^(?:ArrowFunctionExpression|Function(?:Declaration|Expression))$/;

module.exports = function(context) {
    return {
        "BlockStatement": function(node) {

            // if the body is not empty, we can just return immediately
            if (node.body.length !== 0) {
                return;
            }

            // a function is generally allowed to be empty
            if (FUNCTION_TYPE.test(node.parent.type)) {
                return;
            }

            // any other block is only allowed to be empty, if it contains a comment
            if (context.getComments(node).trailing.length > 0) {
                return;
            }

            context.report(node, "Empty block statement.");
        },

        "SwitchStatement": function(node) {

            if (typeof node.cases === "undefined" || node.cases.length === 0) {
                context.report(node, "Empty switch statement.");
            }
        }
    };

};

module.exports.schema = [];
