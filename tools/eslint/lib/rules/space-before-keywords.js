/**
 * @fileoverview Require or disallow spaces before keywords
 * @author Marko Raatikka
 * @copyright 2015 Marko Raatikka. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var ERROR_MSG_SPACE_EXPECTED = "Missing space before keyword \"{{keyword}}\".";
var ERROR_MSG_NO_SPACE_EXPECTED = "Unexpected space before keyword \"{{keyword}}\".";

module.exports = function(context) {

    var sourceCode = context.getSourceCode();

    var SPACE_REQUIRED = context.options[0] !== "never";

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Report the error message
     * @param {ASTNode} node node to report
     * @param {string} message Error message to be displayed
     * @param {object} data Data object for the rule message
     * @returns {void}
     */
    function report(node, message, data) {
        context.report({
            node: node,
            message: message,
            data: data,
            fix: function(fixer) {
                if (SPACE_REQUIRED) {
                    return fixer.insertTextBefore(node, " ");
                } else {
                    var tokenBefore = context.getTokenBefore(node);
                    return fixer.removeRange([tokenBefore.range[1], node.range[0]]);
                }
            }
        });
    }

    /**
     * Check if a token meets the criteria
     *
     * @param   {ASTNode} node    The node to check
     * @param   {Object}  left    The left-hand side token of the node
     * @param   {Object}  right   The right-hand side token of the node
     * @param   {Object}  options See check()
     * @returns {void}
     */
    function checkTokens(node, left, right, options) {

        if (!left) {
            return;
        }

        if (left.type === "Keyword") {
            return;
        }

        if (!SPACE_REQUIRED && typeof options.requireSpace === "undefined") {
            return;
        }

        options = options || {};
        options.allowedPrecedingChars = options.allowedPrecedingChars || [ "{" ];
        options.requireSpace = typeof options.requireSpace === "undefined" ? SPACE_REQUIRED : options.requireSpace;

        var hasSpace = sourceCode.isSpaceBetweenTokens(left, right);
        var spaceOk = hasSpace === options.requireSpace;

        if (spaceOk) {
            return;
        }

        if (!astUtils.isTokenOnSameLine(left, right)) {
            if (!options.requireSpace) {
                report(node, ERROR_MSG_NO_SPACE_EXPECTED, { keyword: right.value });
            }
            return;
        }

        if (!options.requireSpace) {
            report(node, ERROR_MSG_NO_SPACE_EXPECTED, { keyword: right.value });
            return;
        }

        if (options.allowedPrecedingChars.indexOf(left.value) !== -1) {
            return;
        }

        report(node, ERROR_MSG_SPACE_EXPECTED, { keyword: right.value });

    }

    /**
     * Get right and left tokens of a node and check to see if they meet the given criteria
     *
     * @param   {ASTNode}  node                          The node to check
     * @param   {Object}   options                       Options to validate the node against
     * @param   {Array}    options.allowedPrecedingChars Characters that can precede the right token
     * @param   {Boolean}  options.requireSpace          Whether or not the right token needs to be
     *                                                   preceded by a space
     * @returns {void}
     */
    function check(node, options) {

        options = options || {};

        var left = context.getTokenBefore(node);
        var right = context.getFirstToken(node);

        checkTokens(node, left, right, options);

    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "IfStatement": function(node) {
            // if
            check(node);
            // else
            if (node.alternate) {
                var tokens = context.getTokensBefore(node.alternate, 2);
                if (tokens[0].value === "}") {
                    check(tokens[1], { requireSpace: SPACE_REQUIRED });
                }
            }
        },
        "ForStatement": check,
        "ForInStatement": check,
        "WhileStatement": check,
        "DoWhileStatement": function(node) {
            var whileNode = context.getTokenAfter(node.body);
            // do
            check(node);
            // while
            check(whileNode, { requireSpace: SPACE_REQUIRED });
        },
        "SwitchStatement": function(node) {
            // switch
            check(node);
            // case/default
            node.cases.forEach(function(caseNode) {
                check(caseNode);
            });
        },
        "ThrowStatement": check,
        "TryStatement": function(node) {
            // try
            check(node);
            // finally
            if (node.finalizer) {
                check(context.getTokenBefore(node.finalizer), { requireSpace: SPACE_REQUIRED });
            }
        },
        "CatchClause": function(node) {
            check(node, { requireSpace: SPACE_REQUIRED });
        },
        "WithStatement": check,
        "VariableDeclaration": function(node) {
            check(node, { allowedPrecedingChars: [ "(", "{" ] });
        },
        "ReturnStatement": check,
        "BreakStatement": check,
        "LabeledStatement": check,
        "ContinueStatement": check,
        "FunctionDeclaration": check,
        "FunctionExpression": function(node) {
            var left = context.getTokenBefore(node);
            var right = context.getFirstToken(node);

            // If it's a method, a getter, or a setter, the first token is not the `function` keyword.
            if (right.type !== "Keyword") {
                return;
            }

            checkTokens(node, left, right, { allowedPrecedingChars: [ "(", "{", "[" ] });
        },
        "YieldExpression": function(node) {
            check(node, { allowedPrecedingChars: [ "(", "{" ] });
        },
        "ForOfStatement": check,
        "ClassBody": function(node) {

            // Find the 'class' token
            while (node.value !== "class") {
                node = context.getTokenBefore(node);
            }

            check(node);
        }
    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    }
];
