/**
 * @fileoverview Rule to validate spacing before function paren.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2015 Mathias Schreck
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var configuration = context.options[0],
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
     * Determines whether two adjacent tokens are have whitespace between them.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not there is space between the tokens.
     */
    function isSpaced(left, right) {
        return left.range[1] < right.range[0];
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

        parent = context.getAncestors().pop();
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
            tokens,
            leftToken,
            rightToken,
            location;

        if (node.generator && !isNamed) {
            return;
        }

        tokens = context.getTokens(node);

        if (node.generator) {
            if (node.id) {
                leftToken = tokens[2];
                rightToken = tokens[3];
            } else {
                // Object methods are named but don't have an id
                leftToken = context.getTokenBefore(node);
                rightToken = tokens[0];
            }
        } else if (isNamed) {
            if (node.id) {
                leftToken = tokens[1];
                rightToken = tokens[2];
            } else {
                // Object methods are named but don't have an id
                leftToken = context.getTokenBefore(node);
                rightToken = tokens[0];
            }
        } else {
            leftToken = tokens[0];
            rightToken = tokens[1];
        }

        location = leftToken.loc.end;

        if (isSpaced(leftToken, rightToken)) {
            if ((isNamed && !requireNamedFunctionSpacing) || (!isNamed && !requireAnonymousFunctionSpacing)) {
                context.report(node, location, "Unexpected space before function parentheses.");
            }
        } else {
            if ((isNamed && requireNamedFunctionSpacing) || (!isNamed && requireAnonymousFunctionSpacing)) {
                context.report(node, location, "Missing space before function parentheses.");
            }
        }
    }

    return {
        "FunctionDeclaration": validateSpacingBeforeParentheses,
        "FunctionExpression": validateSpacingBeforeParentheses
    };
};
