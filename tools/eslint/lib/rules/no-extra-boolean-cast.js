/**
 * @fileoverview Rule to flag unnecessary double negation in Boolean contexts
 * @author Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "UnaryExpression": function(node) {
            var ancestors = context.getAncestors(),
                parent = ancestors.pop(),
                grandparent = ancestors.pop();

            // Exit early if it's guaranteed not to match
            if (node.operator !== "!" ||
                    parent.type !== "UnaryExpression" ||
                    parent.operator !== "!") {
                return;
            }

            // if (<bool>) ...
            if (grandparent.type === "IfStatement") {
                context.report(node, "Redundant double negation in an if statement condition.");

            // do ... while (<bool>)
            } else if (grandparent.type === "DoWhileStatement") {
                context.report(node, "Redundant double negation in a do while loop condition.");

            // while (<bool>) ...
            } else if (grandparent.type === "WhileStatement") {
                context.report(node, "Redundant double negation in a while loop condition.");

            // <bool> ? ... : ...
            } else if ((grandparent.type === "ConditionalExpression" &&
                    parent === grandparent.test)) {
                context.report(node, "Redundant double negation in a ternary condition.");

            // for (...; <bool>; ...) ...
            } else if ((grandparent.type === "ForStatement" &&
                    parent === grandparent.test)) {
                context.report(node, "Redundant double negation in a for loop condition.");

            // !<bool>
            } else if ((grandparent.type === "UnaryExpression" &&
                    grandparent.operator === "!")) {
                context.report(node, "Redundant multiple negation.");

            // Boolean(<bool>)
            } else if ((grandparent.type === "CallExpression" &&
                    grandparent.callee.type === "Identifier" &&
                    grandparent.callee.name === "Boolean")) {
                context.report(node, "Redundant double negation in call to Boolean().");

            // new Boolean(<bool>)
            } else if ((grandparent.type === "NewExpression" &&
                    grandparent.callee.type === "Identifier" &&
                    grandparent.callee.name === "Boolean")) {
                context.report(node, "Redundant double negation in Boolean constructor call.");
            }
        }
    };

};

module.exports.schema = [];
