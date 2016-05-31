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
        var allowSameLine = context.options[0] && Boolean(context.options[0].allowMultiplePropertiesPerLine);
        var errorMessage = allowSameLine ?
            "Object properties must go on a new line if they aren't all on the same line" :
            "Object properties must go on a new line";

        var sourceCode = context.getSourceCode();

        return {
            ObjectExpression: function(node) {
                var lastTokenOfPreviousValue, firstTokenOfCurrentKey;

                if (allowSameLine) {
                    if (node.properties.length > 1) {
                        var firstToken = sourceCode.getFirstToken(node.properties[0].key);
                        var lastToken = sourceCode.getLastToken(node.properties[node.properties.length - 1].value);

                        if (firstToken.loc.end.line === lastToken.loc.start.line) {

                            // All keys and values are on the same line
                            return;
                        }
                    }
                }

                for (var i = 1; i < node.properties.length; i++) {
                    lastTokenOfPreviousValue = sourceCode.getLastToken(node.properties[i - 1].value);
                    firstTokenOfCurrentKey = sourceCode.getFirstToken(node.properties[i].key);

                    if (lastTokenOfPreviousValue.loc.end.line === firstTokenOfCurrentKey.loc.start.line) {
                        context.report({
                            node: node,
                            loc: firstTokenOfCurrentKey.loc.start,
                            message: errorMessage
                        });
                    }
                }
            }
        };
    }
};
