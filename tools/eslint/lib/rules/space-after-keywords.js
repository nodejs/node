/**
 * @fileoverview Rule to enforce the number of spaces after certain keywords
 * @author Nick Fisher
 * @copyright 2014 Nick Fisher. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // unless the first option is `"never"`, then a space is required
    var requiresSpace = context.options[0] !== "never";

    /**
     * Check if the separation of two adjacent tokens meets the spacing rules, and report a problem if not.
     *
     * @param {ASTNode} node  The node to which the potential problem belongs.
     * @param {Token} left    The first token.
     * @param {Token} right   The second token
     * @returns {void}
     */
    function checkTokens(node, left, right) {
        var hasSpace = left.range[1] < right.range[0],
            value = left.value;

        if (hasSpace !== requiresSpace) {
            context.report(node, "Keyword \"{{value}}\" must {{not}}be followed by whitespace.", {
                value: value,
                not: requiresSpace ? "" : "not "
            });
        }
    }

    /**
     * Check if the given node (`if`, `for`, `while`, etc), has the correct spacing after it.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function check(node) {
        var tokens = context.getFirstTokens(node, 2);
        checkTokens(node, tokens[0], tokens[1]);
    }

    return {
        "IfStatement": function (node) {
            check(node);
            // check the `else`
            if (node.alternate && node.alternate.type !== "IfStatement") {
                checkTokens(node.alternate, context.getTokenBefore(node.alternate), context.getFirstToken(node.alternate));
            }
        },
        "ForStatement": check,
        "ForOfStatement": check,
        "ForInStatement": check,
        "WhileStatement": check,
        "DoWhileStatement": function (node) {
            check(node);
            // check the `while`
            var whileTokens = context.getTokensBefore(node.test, 2);
            checkTokens(node, whileTokens[0], whileTokens[1]);
        },
        "SwitchStatement": check,
        "TryStatement": function (node) {
            check(node);
            // check the `finally`
            if (node.finalizer) {
                checkTokens(node.finalizer, context.getTokenBefore(node.finalizer), context.getFirstToken(node.finalizer));
            }
        },
        "CatchStatement": check,
        "WithStatement": check
    };
};
