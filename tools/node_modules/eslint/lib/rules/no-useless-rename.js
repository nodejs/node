/**
 * @fileoverview Disallow renaming import, export, and destructured assignments to the same name.
 * @author Kai Cataldo
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow renaming import, export, and destructured assignments to the same name",
            category: "ECMAScript 6",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-useless-rename"
        },

        fixable: "code",

        schema: [
            {
                type: "object",
                properties: {
                    ignoreDestructuring: { type: "boolean", default: false },
                    ignoreImport: { type: "boolean", default: false },
                    ignoreExport: { type: "boolean", default: false }
                },
                additionalProperties: false
            }
        ],

        messages: {
            unnecessarilyRenamed: "{{type}} {{name}} unnecessarily renamed."
        }
    },

    create(context) {
        const sourceCode = context.getSourceCode(),
            options = context.options[0] || {},
            ignoreDestructuring = options.ignoreDestructuring === true,
            ignoreImport = options.ignoreImport === true,
            ignoreExport = options.ignoreExport === true;

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Reports error for unnecessarily renamed assignments
         * @param {ASTNode} node node to report
         * @param {ASTNode} initial node with initial name value
         * @param {ASTNode} result node with new name value
         * @param {string} type the type of the offending node
         * @returns {void}
         */
        function reportError(node, initial, result, type) {
            const name = initial.type === "Identifier" ? initial.name : initial.value;

            return context.report({
                node,
                messageId: "unnecessarilyRenamed",
                data: {
                    name,
                    type
                },
                fix(fixer) {
                    if (sourceCode.commentsExistBetween(initial, result)) {
                        return null;
                    }

                    const replacementText = result.type === "AssignmentPattern"
                        ? sourceCode.getText(result)
                        : name;

                    return fixer.replaceTextRange([
                        initial.range[0],
                        result.range[1]
                    ], replacementText);
                }
            });
        }

        /**
         * Checks whether a destructured assignment is unnecessarily renamed
         * @param {ASTNode} node node to check
         * @returns {void}
         */
        function checkDestructured(node) {
            if (ignoreDestructuring) {
                return;
            }

            for (const property of node.properties) {

                /*
                 * TODO: Remove after babel-eslint removes ExperimentalRestProperty
                 * https://github.com/eslint/eslint/issues/12335
                 */
                if (property.type === "ExperimentalRestProperty") {
                    continue;
                }

                /**
                 * Properties using shorthand syntax and rest elements can not be renamed.
                 * If the property is computed, we have no idea if a rename is useless or not.
                 */
                if (property.shorthand || property.type === "RestElement" || property.computed) {
                    continue;
                }

                const key = (property.key.type === "Identifier" && property.key.name) || (property.key.type === "Literal" && property.key.value);
                const renamedKey = property.value.type === "AssignmentPattern" ? property.value.left.name : property.value.name;

                if (key === renamedKey) {
                    reportError(property, property.key, property.value, "Destructuring assignment");
                }
            }
        }

        /**
         * Checks whether an import is unnecessarily renamed
         * @param {ASTNode} node node to check
         * @returns {void}
         */
        function checkImport(node) {
            if (ignoreImport) {
                return;
            }

            if (node.imported.name === node.local.name &&
                    node.imported.range[0] !== node.local.range[0]) {
                reportError(node, node.imported, node.local, "Import");
            }
        }

        /**
         * Checks whether an export is unnecessarily renamed
         * @param {ASTNode} node node to check
         * @returns {void}
         */
        function checkExport(node) {
            if (ignoreExport) {
                return;
            }

            if (node.local.name === node.exported.name &&
                    node.local.range[0] !== node.exported.range[0]) {
                reportError(node, node.local, node.exported, "Export");
            }

        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            ObjectPattern: checkDestructured,
            ImportSpecifier: checkImport,
            ExportSpecifier: checkExport
        };
    }
};
