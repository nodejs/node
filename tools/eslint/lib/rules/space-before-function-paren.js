/**
 * @fileoverview Rule to validate spacing before function paren.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2015 Mathias Schreck
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var configuration = context.options[0],
        sourceCode = context.getSourceCode(),
        requireAnonymousFunctionSpacing = true,
        requireNamedFunctionSpacing = true;

    if (typeof configuration === "object") {
        requireAnonymousFunctionSpacing = configuration.anonymous !== "never";
        requireNamedFunctionSpacing = configuration.named !== "never";
    } else if (configuration === "never") {
        requireAnonymousFunctionSpacing = false;
        requireNamedFunctionSpacing = false;
    }

    /**
     * Determines whether a function has a name.
     * @param {ASTNode} node The function node.
     * @returns {boolean} Whether the function has a name.
     */
    function isNamedFunction(node) {
        var parent;

        if (node.id) {
            return true;
        }

        parent = node.parent;
        return parent.type === "MethodDefinition" ||
            (parent.type === "Property" &&
                (
                    parent.kind === "get" ||
                    parent.kind === "set" ||
                    parent.method
                )
            );
    }

    /**
     * Validates the spacing before function parentheses.
     * @param {ASTNode} node The node to be validated.
     * @returns {void}
     */
    function validateSpacingBeforeParentheses(node) {
        var isNamed = isNamedFunction(node),
            leftToken,
            rightToken,
            location;

        if (node.generator && !isNamed) {
            return;
        }

        rightToken = sourceCode.getFirstToken(node);
        while (rightToken.value !== "(") {
            rightToken = sourceCode.getTokenAfter(rightToken);
        }
        leftToken = context.getTokenBefore(rightToken);
        location = leftToken.loc.end;

        if (sourceCode.isSpaceBetweenTokens(leftToken, rightToken)) {
            if ((isNamed && !requireNamedFunctionSpacing) || (!isNamed && !requireAnonymousFunctionSpacing)) {
                context.report({
                    node: node,
                    loc: location,
                    message: "Unexpected space before function parentheses.",
                    fix: function(fixer) {
                        return fixer.removeRange([leftToken.range[1], rightToken.range[0]]);
                    }
                });
            }
        } else {
            if ((isNamed && requireNamedFunctionSpacing) || (!isNamed && requireAnonymousFunctionSpacing)) {
                context.report({
                    node: node,
                    loc: location,
                    message: "Missing space before function parentheses.",
                    fix: function(fixer) {
                        return fixer.insertTextAfter(leftToken, " ");
                    }
                });
            }
        }
    }

    return {
        "FunctionDeclaration": validateSpacingBeforeParentheses,
        "FunctionExpression": validateSpacingBeforeParentheses
    };
};

module.exports.schema = [
    {
        "oneOf": [
            {
                "enum": ["always", "never"]
            },
            {
                "type": "object",
                "properties": {
                    "anonymous": {
                        "enum": ["always", "never"]
                    },
                    "named": {
                        "enum": ["always", "never"]
                    }
                },
                "additionalProperties": false
            }
        ]
    }
];
