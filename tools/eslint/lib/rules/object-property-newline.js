/**
 * @fileoverview Rule to enforce placing object properties on separate lines.
 * @author Vitor Balocco
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce placing object properties on separate lines",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    allowMultiplePropertiesPerLine: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        let allowSameLine = context.options[0] && Boolean(context.options[0].allowMultiplePropertiesPerLine);
        let errorMessage = allowSameLine ?
            "Object properties must go on a new line if they aren't all on the same line." :
            "Object properties must go on a new line.";

        let sourceCode = context.getSourceCode();

        return {
            ObjectExpression: function(node) {
                let lastTokenOfPreviousProperty, firstTokenOfCurrentProperty;

                if (allowSameLine) {
                    if (node.properties.length > 1) {
                        let firstTokenOfFirstProperty = sourceCode.getFirstToken(node.properties[0]);
                        let lastTokenOfLastProperty = sourceCode.getLastToken(node.properties[node.properties.length - 1]);

                        if (firstTokenOfFirstProperty.loc.end.line === lastTokenOfLastProperty.loc.start.line) {

                            // All keys and values are on the same line
                            return;
                        }
                    }
                }

                for (let i = 1; i < node.properties.length; i++) {
                    lastTokenOfPreviousProperty = sourceCode.getLastToken(node.properties[i - 1]);
                    firstTokenOfCurrentProperty = sourceCode.getFirstToken(node.properties[i]);

                    if (lastTokenOfPreviousProperty.loc.end.line === firstTokenOfCurrentProperty.loc.start.line) {
                        context.report({
                            node: node,
                            loc: firstTokenOfCurrentProperty.loc.start,
                            message: errorMessage
                        });
                    }
                }
            }
        };
    }
};
