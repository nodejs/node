/**
 * @fileoverview Rule to enforce the position of line comments
 * @author Alberto Rodríguez
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce position of line comments",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                oneOf: [
                    {
                        enum: ["above", "beside"]
                    },
                    {
                        type: "object",
                        properties: {
                            position: {
                                enum: ["above", "beside"]
                            },
                            ignorePattern: {
                                type: "string"
                            },
                            applyDefaultPatterns: {
                                type: "boolean"
                            }
                        },
                        additionalProperties: false
                    }
                ]
            }
        ]
    },

    create(context) {
        const DEFAULT_IGNORE_PATTERN = "^\\s*(?:eslint|jshint\\s+|jslint\\s+|istanbul\\s+|globals?\\s+|exported\\s+|jscs|falls?\\s?through)";
        const options = context.options[0];

        let above,
            ignorePattern,
            applyDefaultPatterns = true;

        if (!options || typeof options === "string") {
            above = !options || options === "above";

        } else {
            above = options.position === "above";
            ignorePattern = options.ignorePattern;
            applyDefaultPatterns = options.applyDefaultPatterns !== false;
        }

        const defaultIgnoreRegExp = new RegExp(DEFAULT_IGNORE_PATTERN);
        const customIgnoreRegExp = new RegExp(ignorePattern);
        const sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            LineComment(node) {
                if (applyDefaultPatterns && defaultIgnoreRegExp.test(node.value)) {
                    return;
                }

                if (ignorePattern && customIgnoreRegExp.test(node.value)) {
                    return;
                }

                const previous = sourceCode.getTokenOrCommentBefore(node);
                const isOnSameLine = previous && previous.loc.end.line === node.loc.start.line;

                if (above) {
                    if (isOnSameLine) {
                        context.report({
                            node,
                            message: "Expected comment to be above code."
                        });
                    }
                } else {
                    if (!isOnSameLine) {
                        context.report({
                            node,
                            message: "Expected comment to be beside code."
                        });
                    }
                }
            }
        };
    }
};
