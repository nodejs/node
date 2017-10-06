/**
 * @fileoverview Rule to flag use of a leading/trailing decimal point in a numeric literal
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow leading or trailing decimal points in numeric literals",
            category: "Best Practices",
            recommended: false
        },

        schema: [],

        fixable: "code"
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        return {
            Literal(node) {

                if (typeof node.value === "number") {
                    if (node.raw.startsWith(".")) {
                        context.report({
                            node,
                            message: "A leading decimal point can be confused with a dot.",
                            fix(fixer) {
                                const tokenBefore = sourceCode.getTokenBefore(node);
                                const needsSpaceBefore = tokenBefore &&
                                    tokenBefore.range[1] === node.range[0] &&
                                    !astUtils.canTokensBeAdjacent(tokenBefore, `0${node.raw}`);

                                return fixer.insertTextBefore(node, needsSpaceBefore ? " 0" : "0");
                            }
                        });
                    }
                    if (node.raw.indexOf(".") === node.raw.length - 1) {
                        context.report({
                            node,
                            message: "A trailing decimal point can be confused with a dot.",
                            fix: fixer => fixer.insertTextAfter(node, "0")
                        });
                    }
                }
            }
        };

    }
};
