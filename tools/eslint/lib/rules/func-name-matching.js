/**
 * @fileoverview Rule to require function names to match the name of the variable or property to which they are assigned.
 * @author Annie Zhang, Pavel Strashkin
 */

"use strict";

//--------------------------------------------------------------------------
// Requirements
//--------------------------------------------------------------------------

const astUtils = require("../ast-utils");
const esutils = require("esutils");

//--------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------

/**
 * Determines if a pattern is `module.exports` or `module["exports"]`
 * @param {ASTNode} pattern The left side of the AssignmentExpression
 * @returns {boolean} True if the pattern is `module.exports` or `module["exports"]`
 */
function isModuleExports(pattern) {
    if (pattern.type === "MemberExpression" && pattern.object.type === "Identifier" && pattern.object.name === "module") {

        // module.exports
        if (pattern.property.type === "Identifier" && pattern.property.name === "exports") {
            return true;
        }

        // module["exports"]
        if (pattern.property.type === "Literal" && pattern.property.value === "exports") {
            return true;
        }
    }
    return false;
}

/**
 * Determines if a string name is a valid identifier
 * @param {string} name The string to be checked
 * @param {int} ecmaVersion The ECMAScript version if specified in the parserOptions config
 * @returns {boolean} True if the string is a valid identifier
 */
function isIdentifier(name, ecmaVersion) {
    if (ecmaVersion >= 6) {
        return esutils.keyword.isIdentifierES6(name);
    }
    return esutils.keyword.isIdentifierES5(name);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require function names to match the name of the variable or property to which they are assigned",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    includeCommonJSModuleExports: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create(context) {

        const includeModuleExports = context.options[0] && context.options[0].includeCommonJSModuleExports;
        const ecmaVersion = context.parserOptions && context.parserOptions.ecmaVersion ? context.parserOptions.ecmaVersion : 5;

        /**
         * Reports
         * @param {ASTNode} node The node to report
         * @param {string} name The variable or property name
         * @param {string} funcName The function name
         * @param {boolean} isProp True if the reported node is a property assignment
         * @returns {void}
         */
        function report(node, name, funcName, isProp) {
            context.report({
                node,
                message: isProp ? "Function name `{{funcName}}` should match property name `{{name}}`"
                                : "Function name `{{funcName}}` should match variable name `{{name}}`",
                data: {
                    name,
                    funcName
                }
            });
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            VariableDeclarator(node) {
                if (!node.init || node.init.type !== "FunctionExpression") {
                    return;
                }
                if (node.init.id && node.id.name !== node.init.id.name) {
                    report(node, node.id.name, node.init.id.name, false);
                }
            },

            AssignmentExpression(node) {
                if (node.right.type !== "FunctionExpression" ||
                   (node.left.computed && node.left.property.type !== "Literal") ||
                    (!includeModuleExports && isModuleExports(node.left))
                   ) {
                    return;
                }

                const isProp = node.left.type === "MemberExpression" ? true : false;
                const name = isProp ? astUtils.getStaticPropertyName(node.left) : node.left.name;

                if (node.right.id && isIdentifier(name) && name !== node.right.id.name) {
                    report(node, name, node.right.id.name, isProp);
                }
            },

            Property(node) {
                if (node.value.type !== "FunctionExpression" || !node.value.id || node.computed && node.key.type !== "Literal") {
                    return;
                }
                if (node.key.type === "Identifier" && node.key.name !== node.value.id.name) {
                    report(node, node.key.name, node.value.id.name, true);
                } else if (node.key.type === "Literal" &&
                           isIdentifier(node.key.value, ecmaVersion) &&
                           node.key.value !== node.value.id.name
                          ) {
                    report(node, node.key.value, node.value.id.name, true);
                }
            }
        };
    }
};
