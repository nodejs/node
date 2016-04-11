/**
 * @fileoverview Rule to disallow whitespace before properties
 * @author Kai Cataldo
 * @copyright 2015 Kai Cataldo. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var sourceCode = context.getSourceCode();

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Finds opening bracket token of node's computed property
     * @param {ASTNode} node - the node to check
     * @returns {Token} opening bracket token of node's computed property
     * @private
     */
    function findOpeningBracket(node) {
        var token = sourceCode.getTokenBefore(node.property);

        while (token.value !== "[") {
            token = sourceCode.getTokenBefore(token);
        }
        return token;
    }

    /**
     * Reports whitespace before property token
     * @param {ASTNode} node - the node to report in the event of an error
     * @returns {void}
     * @private
     */
    function reportError(node) {
        context.report({
            node: node,
            message: "Unexpected whitespace before property {{propName}}.",
            data: {
                propName: sourceCode.getText(node.property)
            }
        });
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        MemberExpression: function(node) {
            var rightToken;
            var leftToken;

            if (!astUtils.isTokenOnSameLine(node.object, node.property)) {
                return;
            }

            if (node.computed) {
                rightToken = findOpeningBracket(node);
                leftToken = sourceCode.getTokenBefore(rightToken);
            } else {
                rightToken = sourceCode.getFirstToken(node.property);
                leftToken = sourceCode.getTokenBefore(rightToken, 1);
            }

            if (sourceCode.isSpaceBetweenTokens(leftToken, rightToken)) {
                reportError(node);
            }
        }
    };
};

module.exports.schema = [];
