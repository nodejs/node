/**
 * @fileoverview Rule to flag statements without curly braces
 * @author Nicholas C. Zakas
 * @copyright 2015 Ivan Nikulin. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var multiOnly = (context.options[0] === "multi");
    var multiLine = (context.options[0] === "multi-line");
    var multiOrNest = (context.options[0] === "multi-or-nest");
    var consistent = (context.options[1] === "consistent");

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Determines if a given node is a one-liner that's on the same line as it's preceding code.
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node is a one-liner that's on the same line as it's preceding code.
     * @private
     */
    function isCollapsedOneLiner(node) {
        var before = context.getTokenBefore(node),
            last = context.getLastToken(node);

        return before.loc.start.line === last.loc.end.line;
    }

    /**
     * Determines if a given node is a one-liner.
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node is a one-liner.
     * @private
     */
    function isOneLiner(node) {
        var first = context.getFirstToken(node),
            last = context.getLastToken(node);

        return first.loc.start.line === last.loc.end.line;
    }

    /**
     * Gets the `else` keyword token of a given `IfStatement` node.
     * @param {ASTNode} node - A `IfStatement` node to get.
     * @returns {Token} The `else` keyword token.
     */
    function getElseKeyword(node) {
        var sourceCode = context.getSourceCode();
        var token = sourceCode.getTokenAfter(node.consequent);

        while (token.type !== "Keyword" || token.value !== "else") {
            token = sourceCode.getTokenAfter(token);
        }

        return token;
    }

    /**
     * Checks a given IfStatement node requires braces of the consequent chunk.
     * This returns `true` when below:
     *
     * 1. The given node has the `alternate` node.
     * 2. There is a `IfStatement` which doesn't have `alternate` node in the
     *    trailing statement chain of the `consequent` node.
     *
     * @param {ASTNode} node - A IfStatement node to check.
     * @returns {boolean} `true` if the node requires braces of the consequent chunk.
     */
    function requiresBraceOfConsequent(node) {
        if (node.alternate && node.consequent.type === "BlockStatement") {
            if (node.consequent.body.length >= 2) {
                return true;
            }

            node = node.consequent.body[0];
            while (node) {
                if (node.type === "IfStatement" && !node.alternate) {
                    return true;
                }
                node = astUtils.getTrailingStatement(node);
            }
        }

        return false;
    }

    /**
     * Reports "Expected { after ..." error
     * @param {ASTNode} node The node to report.
     * @param {string} name The name to report.
     * @param {string} suffix Additional string to add to the end of a report.
     * @returns {void}
     * @private
     */
    function reportExpectedBraceError(node, name, suffix) {
        context.report({
            node: node,
            loc: (name !== "else" ? node : getElseKeyword(node)).loc.start,
            message: "Expected { after '{{name}}'{{suffix}}.",
            data: {
                name: name,
                suffix: (suffix ? " " + suffix : "")
            }
        });
    }

    /**
     * Reports "Unnecessary { after ..." error
     * @param {ASTNode} node The node to report.
     * @param {string} name The name to report.
     * @param {string} suffix Additional string to add to the end of a report.
     * @returns {void}
     * @private
     */
    function reportUnnecessaryBraceError(node, name, suffix) {
        context.report({
            node: node,
            loc: (name !== "else" ? node : getElseKeyword(node)).loc.start,
            message: "Unnecessary { after '{{name}}'{{suffix}}.",
            data: {
                name: name,
                suffix: (suffix ? " " + suffix : "")
            }
        });
    }

    /**
     * Prepares to check the body of a node to see if it's a block statement.
     * @param {ASTNode} node The node to report if there's a problem.
     * @param {ASTNode} body The body node to check for blocks.
     * @param {string} name The name to report if there's a problem.
     * @param {string} suffix Additional string to add to the end of a report.
     * @returns {object} a prepared check object, with "actual", "expected", "check" properties.
     *   "actual" will be `true` or `false` whether the body is already a block statement.
     *   "expected" will be `true` or `false` if the body should be a block statement or not, or
     *   `null` if it doesn't matter, depending on the rule options. It can be modified to change
     *   the final behavior of "check".
     *   "check" will be a function reporting appropriate problems depending on the other
     *   properties.
     */
    function prepareCheck(node, body, name, suffix) {
        var hasBlock = (body.type === "BlockStatement");
        var expected = null;

        if (node.type === "IfStatement" && node.consequent === body && requiresBraceOfConsequent(node)) {
            expected = true;
        } else if (multiOnly) {
            if (hasBlock && body.body.length === 1) {
                expected = false;
            }
        } else if (multiLine) {
            if (!isCollapsedOneLiner(body)) {
                expected = true;
            }
        } else if (multiOrNest) {
            if (hasBlock && body.body.length === 1 && isOneLiner(body.body[0])) {
                expected = false;
            } else if (!isOneLiner(body)) {
                expected = true;
            }
        } else {
            expected = true;
        }

        return {
            actual: hasBlock,
            expected: expected,
            check: function() {
                if (this.expected !== null && this.expected !== this.actual) {
                    if (this.expected) {
                        reportExpectedBraceError(node, name, suffix);
                    } else {
                        reportUnnecessaryBraceError(node, name, suffix);
                    }
                }
            }
        };
    }

    /**
     * Prepares to check the bodies of a "if", "else if" and "else" chain.
     * @param {ASTNode} node The first IfStatement node of the chain.
     * @returns {object[]} prepared checks for each body of the chain. See `prepareCheck` for more
     *   information.
     */
    function prepareIfChecks(node) {
        var preparedChecks = [];

        do {
            preparedChecks.push(prepareCheck(node, node.consequent, "if", "condition"));
            if (node.alternate && node.alternate.type !== "IfStatement") {
                preparedChecks.push(prepareCheck(node, node.alternate, "else"));
                break;
            }
            node = node.alternate;
        } while (node);

        if (consistent) {

            /*
             * If any node should have or already have braces, make sure they
             * all have braces.
             * If all nodes shouldn't have braces, make sure they don't.
             */
            var expected = preparedChecks.some(function(preparedCheck) {
                if (preparedCheck.expected !== null) {
                    return preparedCheck.expected;
                }
                return preparedCheck.actual;
            });

            preparedChecks.forEach(function(preparedCheck) {
                preparedCheck.expected = expected;
            });
        }

        return preparedChecks;
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "IfStatement": function(node) {
            if (node.parent.type !== "IfStatement") {
                prepareIfChecks(node).forEach(function(preparedCheck) {
                    preparedCheck.check();
                });
            }
        },

        "WhileStatement": function(node) {
            prepareCheck(node, node.body, "while", "condition").check();
        },

        "DoWhileStatement": function(node) {
            prepareCheck(node, node.body, "do").check();
        },

        "ForStatement": function(node) {
            prepareCheck(node, node.body, "for", "condition").check();
        },

        "ForInStatement": function(node) {
            prepareCheck(node, node.body, "for-in").check();
        },

        "ForOfStatement": function(node) {
            prepareCheck(node, node.body, "for-of").check();
        }
    };
};

module.exports.schema = {
    "anyOf": [
        {
            "type": "array",
            "items": [
                {
                    "enum": ["all"]
                }
            ],
            "minItems": 0,
            "maxItems": 1
        },
        {
            "type": "array",
            "items": [
                {
                    "enum": ["multi", "multi-line", "multi-or-nest"]
                },
                {
                    "enum": ["consistent"]
                }
            ],
            "minItems": 0,
            "maxItems": 2
        }
    ]
};
