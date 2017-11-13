/**
 * @fileoverview Restrict usage of specified node imports.
 * @author Guy Ellis
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const DEFAULT_MESSAGE_TEMPLATE = "'{{importName}}' import is restricted from being used.";
const CUSTOM_MESSAGE_TEMPLATE = "'{{importName}}' import is restricted from being used. {{customMessage}}";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const ignore = require("ignore");

const arrayOfStrings = {
    type: "array",
    items: { type: "string" },
    uniqueItems: true
};

const arrayOfStringsOrObjects = {
    type: "array",
    items: {
        anyOf: [
            { type: "string" },
            {
                type: "object",
                properties: {
                    name: { type: "string" },
                    message: {
                        type: "string",
                        minLength: 1
                    }
                },
                additionalProperties: false,
                required: ["name"]
            }
        ]
    },
    uniqueItems: true
};

module.exports = {
    meta: {
        docs: {
            description: "disallow specified modules when loaded by `import`",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: {
            anyOf: [
                arrayOfStringsOrObjects,
                {
                    type: "array",
                    items: {
                        type: "object",
                        properties: {
                            paths: arrayOfStringsOrObjects,
                            patterns: arrayOfStrings
                        },
                        additionalProperties: false
                    },
                    additionalItems: false
                }
            ]
        }
    },

    create(context) {
        const options = Array.isArray(context.options) ? context.options : [];
        const isPathAndPatternsObject =
            typeof options[0] === "object" &&
            (options[0].hasOwnProperty("paths") || options[0].hasOwnProperty("patterns"));

        const restrictedPaths = (isPathAndPatternsObject ? options[0].paths : context.options) || [];
        const restrictedPatterns = (isPathAndPatternsObject ? options[0].patterns : []) || [];

        const restrictedPathMessages = restrictedPaths.reduce((memo, importName) => {
            if (typeof importName === "string") {
                memo[importName] = null;
            } else {
                memo[importName.name] = importName.message;
            }
            return memo;
        }, {});

        // if no imports are restricted we don"t need to check
        if (Object.keys(restrictedPaths).length === 0 && restrictedPatterns.length === 0) {
            return {};
        }

        const ig = ignore().add(restrictedPatterns);

        /**
         * Report a restricted path.
         * @param {node} node representing the restricted path reference
         * @returns {void}
         * @private
         */
        function reportPath(node) {
            const importName = node.source.value.trim();
            const customMessage = restrictedPathMessages[importName];
            const message = customMessage
                ? CUSTOM_MESSAGE_TEMPLATE
                : DEFAULT_MESSAGE_TEMPLATE;

            context.report({
                node,
                message,
                data: {
                    importName,
                    customMessage
                }
            });
        }

        /**
         * Check if the given name is a restricted path name.
         * @param {string} name name of a variable
         * @returns {boolean} whether the variable is a restricted path or not
         * @private
         */
        function isRestrictedPath(name) {
            return Object.prototype.hasOwnProperty.call(restrictedPathMessages, name);
        }

        return {
            ImportDeclaration(node) {
                if (node && node.source && node.source.value) {
                    const importName = node.source.value.trim();

                    if (isRestrictedPath(importName)) {
                        reportPath(node);
                    }
                    if (restrictedPatterns.length > 0 && ig.ignores(importName)) {
                        context.report({
                            node,
                            message: "'{{importName}}' import is restricted from being used by a pattern.",
                            data: {
                                importName
                            }
                        });
                    }
                }
            }
        };
    }
};
