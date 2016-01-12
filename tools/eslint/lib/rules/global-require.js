/**
 * @fileoverview Rule for disallowing require() outside of the top-level module context
 * @author Jamund Ferguson
 * @copyright 2015 Jamund Ferguson. All rights reserved.
 */

"use strict";

var ACCEPTABLE_PARENTS = [
    "AssignmentExpression",
    "VariableDeclarator",
    "MemberExpression",
    "ExpressionStatement",
    "CallExpression",
    "ConditionalExpression",
    "Program",
    "VariableDeclaration"
];

module.exports = function(context) {
    return {
        "CallExpression": function(node) {
            if (node.callee.name === "require") {
                var isGoodRequire = context.getAncestors().every(function(parent) {
                    return ACCEPTABLE_PARENTS.indexOf(parent.type) > -1;
                });
                if (!isGoodRequire) {
                    context.report(node, "Unexpected require().");
                }
            }
        }
    };
};

module.exports.schema = [];
