/**
 * @fileoverview A rule to disallow calls to the Object constructor
 * @author Matt DuVall <http://www.mattduvall.com/>
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "Disallow `Object` constructors",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-new-object"
        },

        schema: [],

        messages: {
            preferLiteral: "The object literal notation {} is preferable."
        }
    },

    create(context) {
        return {
            NewExpression(node) {
                const variable = astUtils.getVariableByName(
                    context.getScope(),
                    node.callee.name
                );

                if (variable && variable.identifiers.length > 0) {
                    return;
                }

                if (node.callee.name === "Object") {
                    context.report({
                        node,
                        messageId: "preferLiteral"
                    });
                }
            }
        };
    }
};
