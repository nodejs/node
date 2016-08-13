/**
 * @fileoverview Rule to enforce a particular function style
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce the consistent use of either `function` declarations or expressions",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["declaration", "expression"]
            },
            {
                type: "object",
                properties: {
                    allowArrowFunctions: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        const style = context.options[0],
            allowArrowFunctions = context.options[1] && context.options[1].allowArrowFunctions === true,
            enforceDeclarations = (style === "declaration"),
            stack = [];

        const nodesToCheck = {
            FunctionDeclaration: function(node) {
                stack.push(false);

                if (!enforceDeclarations && node.parent.type !== "ExportDefaultDeclaration") {
                    context.report(node, "Expected a function expression.");
                }
            },
            "FunctionDeclaration:exit": function() {
                stack.pop();
            },

            FunctionExpression: function(node) {
                stack.push(false);

                if (enforceDeclarations && node.parent.type === "VariableDeclarator") {
                    context.report(node.parent, "Expected a function declaration.");
                }
            },
            "FunctionExpression:exit": function() {
                stack.pop();
            },

            ThisExpression: function() {
                if (stack.length > 0) {
                    stack[stack.length - 1] = true;
                }
            }
        };

        if (!allowArrowFunctions) {
            nodesToCheck.ArrowFunctionExpression = function() {
                stack.push(false);
            };

            nodesToCheck["ArrowFunctionExpression:exit"] = function(node) {
                const hasThisExpr = stack.pop();

                if (enforceDeclarations && !hasThisExpr && node.parent.type === "VariableDeclarator") {
                    context.report(node.parent, "Expected a function declaration.");
                }
            };
        }

        return nodesToCheck;

    }
};
