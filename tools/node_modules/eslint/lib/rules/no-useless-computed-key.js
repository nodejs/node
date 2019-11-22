/**
 * @fileoverview Rule to disallow unnecessary computed property keys in object literals
 * @author Burak Yigit Kaya
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const lodash = require("lodash");
const astUtils = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const MESSAGE_UNNECESSARY_COMPUTED = "Unnecessarily computed property [{{property}}] found.";

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow unnecessary computed property keys in objects and classes",
            category: "ECMAScript 6",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-useless-computed-key"
        },

        schema: [{
            type: "object",
            properties: {
                enforceForClassMembers: {
                    type: "boolean",
                    default: false
                }
            },
            additionalProperties: false
        }],
        fixable: "code"
    },
    create(context) {
        const sourceCode = context.getSourceCode();
        const enforceForClassMembers = context.options[0] && context.options[0].enforceForClassMembers;

        /**
         * Reports a given node if it violated this rule.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         */
        function check(node) {
            if (!node.computed) {
                return;
            }

            const key = node.key,
                nodeType = typeof key.value;

            let allowedKey;

            if (node.type === "MethodDefinition") {
                allowedKey = node.static ? "prototype" : "constructor";
            } else {
                allowedKey = "__proto__";
            }

            if (key.type === "Literal" && (nodeType === "string" || nodeType === "number") && key.value !== allowedKey) {
                context.report({
                    node,
                    message: MESSAGE_UNNECESSARY_COMPUTED,
                    data: { property: sourceCode.getText(key) },
                    fix(fixer) {
                        const leftSquareBracket = sourceCode.getFirstToken(node, astUtils.isOpeningBracketToken);
                        const rightSquareBracket = sourceCode.getFirstTokenBetween(node.key, node.value, astUtils.isClosingBracketToken);
                        const tokensBetween = sourceCode.getTokensBetween(leftSquareBracket, rightSquareBracket, 1);

                        if (tokensBetween.slice(0, -1).some((token, index) =>
                            sourceCode.getText().slice(token.range[1], tokensBetween[index + 1].range[0]).trim())) {

                            // If there are comments between the brackets and the property name, don't do a fix.
                            return null;
                        }

                        const tokenBeforeLeftBracket = sourceCode.getTokenBefore(leftSquareBracket);

                        // Insert a space before the key to avoid changing identifiers, e.g. ({ get[2]() {} }) to ({ get2() {} })
                        const needsSpaceBeforeKey = tokenBeforeLeftBracket.range[1] === leftSquareBracket.range[0] &&
                            !astUtils.canTokensBeAdjacent(tokenBeforeLeftBracket, sourceCode.getFirstToken(key));

                        const replacementKey = (needsSpaceBeforeKey ? " " : "") + key.raw;

                        return fixer.replaceTextRange([leftSquareBracket.range[0], rightSquareBracket.range[1]], replacementKey);
                    }
                });
            }
        }

        return {
            Property: check,
            MethodDefinition: enforceForClassMembers ? check : lodash.noop
        };
    }
};
