/**
 * @fileoverview Rule to require braces in arrow function body.
 * @author Alberto Rodríguez
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require braces around arrow function bodies",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: {
            anyOf: [
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "never"]
                        }
                    ],
                    minItems: 0,
                    maxItems: 1
                },
                {
                    type: "array",
                    items: [
                        {
                            enum: ["as-needed"]
                        },
                        {
                            type: "object",
                            properties: {
                                requireReturnForObjectLiteral: {type: "boolean"}
                            },
                            additionalProperties: false
                        }
                    ],
                    minItems: 0,
                    maxItems: 2
                }
            ]
        }
    },

    create: function(context) {
        const options = context.options;
        const always = options[0] === "always";
        const asNeeded = !options[0] || options[0] === "as-needed";
        const never = options[0] === "never";
        const requireReturnForObjectLiteral = options[1] && options[1].requireReturnForObjectLiteral;

        /**
         * Determines whether a arrow function body needs braces
         * @param {ASTNode} node The arrow function node.
         * @returns {void}
         */
        function validate(node) {
            const arrowBody = node.body;

            if (arrowBody.type === "BlockStatement") {
                if (never) {
                    context.report({
                        node: node,
                        loc: arrowBody.loc.start,
                        message: "Unexpected block statement surrounding arrow body."
                    });
                } else {
                    const blockBody = arrowBody.body;

                    if (blockBody.length !== 1) {
                        return;
                    }

                    if (asNeeded && requireReturnForObjectLiteral && blockBody[0].type === "ReturnStatement" &&
                        blockBody[0].argument.type === "ObjectExpression") {
                        return;
                    }

                    if (asNeeded && blockBody[0].type === "ReturnStatement") {
                        context.report({
                            node: node,
                            loc: arrowBody.loc.start,
                            message: "Unexpected block statement surrounding arrow body."
                        });
                    }
                }
            } else {
                if (always || (asNeeded && requireReturnForObjectLiteral && arrowBody.type === "ObjectExpression")) {
                    context.report({
                        node: node,
                        loc: arrowBody.loc.start,
                        message: "Expected block statement surrounding arrow body."
                    });
                }
            }
        }

        return {
            ArrowFunctionExpression: validate
        };
    }
};
