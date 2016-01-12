/**
 * @fileoverview Require spaces following return, throw, and case
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Check if the node for spaces
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function check(node) {
        var tokens = context.getFirstTokens(node, 2),
            value = tokens[0].value;

        if (tokens[0].range[1] >= tokens[1].range[0]) {
            context.report({
                node: node,
                message: "Keyword \"" + value + "\" must be followed by whitespace.",
                fix: function(fixer) {
                    return fixer.insertTextAfterRange(tokens[0].range, " ");
                }
            });
        }
    }

    return {
        "ReturnStatement": function(node) {
            if (node.argument) {
                check(node);
            }
        },
        "SwitchCase": function(node) {
            if (node.test) {
                check(node);
            }
        },
        "ThrowStatement": check
    };

};

module.exports.schema = [];
