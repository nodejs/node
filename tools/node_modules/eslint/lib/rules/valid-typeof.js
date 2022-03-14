/**
 * @fileoverview Ensures that the results of typeof are compared against a valid string
 * @author Ian Christian Myers
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "enforce comparing `typeof` expressions against valid strings",
            recommended: true,
            url: "https://eslint.org/docs/rules/valid-typeof"
        },

        hasSuggestions: true,

        schema: [
            {
                type: "object",
                properties: {
                    requireStringLiterals: {
                        type: "boolean",
                        default: false
                    }
                },
                additionalProperties: false
            }
        ],
        messages: {
            invalidValue: "Invalid typeof comparison value.",
            notString: "Typeof comparisons should be to string literals.",
            suggestString: 'Use `"{{type}}"` instead of `{{type}}`.'
        }
    },

    create(context) {

        const VALID_TYPES = ["symbol", "undefined", "object", "boolean", "number", "string", "function", "bigint"],
            OPERATORS = ["==", "===", "!=", "!=="];

        const requireStringLiterals = context.options[0] && context.options[0].requireStringLiterals;

        let globalScope;

        /**
         * Checks whether the given node represents a reference to a global variable that is not declared in the source code.
         * These identifiers will be allowed, as it is assumed that user has no control over the names of external global variables.
         * @param {ASTNode} node `Identifier` node to check.
         * @returns {boolean} `true` if the node is a reference to a global variable.
         */
        function isReferenceToGlobalVariable(node) {
            const variable = globalScope.set.get(node.name);

            return variable && variable.defs.length === 0 &&
                variable.references.some(ref => ref.identifier === node);
        }

        /**
         * Determines whether a node is a typeof expression.
         * @param {ASTNode} node The node
         * @returns {boolean} `true` if the node is a typeof expression
         */
        function isTypeofExpression(node) {
            return node.type === "UnaryExpression" && node.operator === "typeof";
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            Program() {
                globalScope = context.getScope();
            },

            UnaryExpression(node) {
                if (isTypeofExpression(node)) {
                    const parent = context.getAncestors().pop();

                    if (parent.type === "BinaryExpression" && OPERATORS.indexOf(parent.operator) !== -1) {
                        const sibling = parent.left === node ? parent.right : parent.left;

                        if (sibling.type === "Literal" || sibling.type === "TemplateLiteral" && !sibling.expressions.length) {
                            const value = sibling.type === "Literal" ? sibling.value : sibling.quasis[0].value.cooked;

                            if (VALID_TYPES.indexOf(value) === -1) {
                                context.report({ node: sibling, messageId: "invalidValue" });
                            }
                        } else if (sibling.type === "Identifier" && sibling.name === "undefined" && isReferenceToGlobalVariable(sibling)) {
                            context.report({
                                node: sibling,
                                messageId: requireStringLiterals ? "notString" : "invalidValue",
                                suggest: [
                                    {
                                        messageId: "suggestString",
                                        data: { type: "undefined" },
                                        fix(fixer) {
                                            return fixer.replaceText(sibling, '"undefined"');
                                        }
                                    }
                                ]
                            });
                        } else if (requireStringLiterals && !isTypeofExpression(sibling)) {
                            context.report({ node: sibling, messageId: "notString" });
                        }
                    }
                }
            }

        };

    }
};
