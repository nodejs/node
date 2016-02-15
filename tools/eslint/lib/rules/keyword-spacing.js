/**
 * @fileoverview Rule to enforce spacing before and after keywords.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils"),
    keywords = require("../util/keywords");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var PREV_TOKEN = /^[\)\]\}>]$/;
var NEXT_TOKEN = /^(?:[\(\[\{<~!]|\+\+?|--?)$/;
var PREV_TOKEN_M = /^[\)\]\}>*]$/;
var NEXT_TOKEN_M = /^[\{*]$/;
var TEMPLATE_OPEN_PAREN = /\$\{$/;
var TEMPLATE_CLOSE_PAREN = /^\}/;
var CHECK_TYPE = /^(?:JSXElement|RegularExpression|String|Template)$/;
var KEYS = keywords.concat(["as", "await", "from", "get", "let", "of", "set", "yield"]);

// check duplications.
(function() {
    KEYS.sort();
    for (var i = 1; i < KEYS.length; ++i) {
        if (KEYS[i] === KEYS[i - 1]) {
            throw new Error("Duplication was found in the keyword list: " + KEYS[i]);
        }
    }
}());

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given token is a "Template" token ends with "${".
 *
 * @param {Token} token - A token to check.
 * @returns {boolean} `true` if the token is a "Template" token ends with "${".
 */
function isOpenParenOfTemplate(token) {
    return token.type === "Template" && TEMPLATE_OPEN_PAREN.test(token.value);
}

/**
 * Checks whether or not a given token is a "Template" token starts with "}".
 *
 * @param {Token} token - A token to check.
 * @returns {boolean} `true` if the token is a "Template" token starts with "}".
 */
function isCloseParenOfTemplate(token) {
    return token.type === "Template" && TEMPLATE_CLOSE_PAREN.test(token.value);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var sourceCode = context.getSourceCode();

    /**
     * Reports a given token if there are not space(s) before the token.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the previous
     *      token to check.
     * @returns {void}
     */
    function expectSpaceBefore(token, pattern) {
        pattern = pattern || PREV_TOKEN;

        var prevToken = sourceCode.getTokenBefore(token);
        if (prevToken &&
            (CHECK_TYPE.test(prevToken.type) || pattern.test(prevToken.value)) &&
            !isOpenParenOfTemplate(prevToken) &&
            astUtils.isTokenOnSameLine(prevToken, token) &&
            !sourceCode.isSpaceBetweenTokens(prevToken, token)
        ) {
            context.report({
                loc: token.loc.start,
                message: "Expected space(s) before \"{{value}}\".",
                data: token,
                fix: function(fixer) {
                    return fixer.insertTextBefore(token, " ");
                }
            });
        }
    }

    /**
     * Reports a given token if there are space(s) before the token.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the previous
     *      token to check.
     * @returns {void}
     */
    function unexpectSpaceBefore(token, pattern) {
        pattern = pattern || PREV_TOKEN;

        var prevToken = sourceCode.getTokenBefore(token);
        if (prevToken &&
            (CHECK_TYPE.test(prevToken.type) || pattern.test(prevToken.value)) &&
            !isOpenParenOfTemplate(prevToken) &&
            astUtils.isTokenOnSameLine(prevToken, token) &&
            sourceCode.isSpaceBetweenTokens(prevToken, token)
        ) {
            context.report({
                loc: token.loc.start,
                message: "Unexpected space(s) before \"{{value}}\".",
                data: token,
                fix: function(fixer) {
                    return fixer.removeRange([prevToken.range[1], token.range[0]]);
                }
            });
        }
    }

    /**
     * Reports a given token if there are not space(s) after the token.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the next
     *      token to check.
     * @returns {void}
     */
    function expectSpaceAfter(token, pattern) {
        pattern = pattern || NEXT_TOKEN;

        var nextToken = sourceCode.getTokenAfter(token);
        if (nextToken &&
            (CHECK_TYPE.test(nextToken.type) || pattern.test(nextToken.value)) &&
            !isCloseParenOfTemplate(nextToken) &&
            astUtils.isTokenOnSameLine(token, nextToken) &&
            !sourceCode.isSpaceBetweenTokens(token, nextToken)
        ) {
            context.report({
                loc: token.loc.start,
                message: "Expected space(s) after \"{{value}}\".",
                data: token,
                fix: function(fixer) {
                    return fixer.insertTextAfter(token, " ");
                }
            });
        }
    }

    /**
     * Reports a given token if there are space(s) after the token.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the next
     *      token to check.
     * @returns {void}
     */
    function unexpectSpaceAfter(token, pattern) {
        pattern = pattern || NEXT_TOKEN;

        var nextToken = sourceCode.getTokenAfter(token);
        if (nextToken &&
            (CHECK_TYPE.test(nextToken.type) || pattern.test(nextToken.value)) &&
            !isCloseParenOfTemplate(nextToken) &&
            astUtils.isTokenOnSameLine(token, nextToken) &&
            sourceCode.isSpaceBetweenTokens(token, nextToken)
        ) {
            context.report({
                loc: token.loc.start,
                message: "Unexpected space(s) after \"{{value}}\".",
                data: token,
                fix: function(fixer) {
                    return fixer.removeRange([token.range[1], nextToken.range[0]]);
                }
            });
        }
    }

    /**
     * Parses the option object and determines check methods for each keyword.
     *
     * @param {object|undefined} options - The option object to parse.
     * @returns {object} - Normalized option object.
     *      Keys are keywords (there are for every keyword).
     *      Values are instances of `{"before": function, "after": function}`.
     */
    function parseOptions(options) {
        var before = !options || options.before !== false;
        var after = !options || options.after !== false;
        var defaultValue = {
            before: before ? expectSpaceBefore : unexpectSpaceBefore,
            after: after ? expectSpaceAfter : unexpectSpaceAfter
        };
        var overrides = (options && options.overrides) || {};
        var retv = Object.create(null);

        for (var i = 0; i < KEYS.length; ++i) {
            var key = KEYS[i];
            var override = overrides[key];

            if (override) {
                var thisBefore = ("before" in override) ? override.before : before;
                var thisAfter = ("after" in override) ? override.after : after;
                retv[key] = {
                    before: thisBefore ? expectSpaceBefore : unexpectSpaceBefore,
                    after: thisAfter ? expectSpaceAfter : unexpectSpaceAfter
                };
            } else {
                retv[key] = defaultValue;
            }
        }

        return retv;
    }

    var checkMethodMap = parseOptions(context.options[0]);

    /**
     * Reports a given token if usage of spacing followed by the token is
     * invalid.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the previous
     *      token to check.
     * @returns {void}
     */
    function checkSpacingBefore(token, pattern) {
        checkMethodMap[token.value].before(token, pattern);
    }

    /**
     * Reports a given token if usage of spacing preceded by the token is
     * invalid.
     *
     * @param {Token} token - A token to report.
     * @param {RegExp|undefined} pattern - Optional. A pattern of the next
     *      token to check.
     * @returns {void}
     */
    function checkSpacingAfter(token, pattern) {
        checkMethodMap[token.value].after(token, pattern);
    }

    /**
     * Reports a given token if usage of spacing around the token is invalid.
     *
     * @param {Token} token - A token to report.
     * @returns {void}
     */
    function checkSpacingAround(token) {
        checkSpacingBefore(token);
        checkSpacingAfter(token);
    }

    /**
     * Reports the first token of a given node if the first token is a keyword
     * and usage of spacing around the token is invalid.
     *
     * @param {ASTNode|null} node - A node to report.
     * @returns {void}
     */
    function checkSpacingAroundFirstToken(node) {
        var firstToken = node && sourceCode.getFirstToken(node);
        if (firstToken && firstToken.type === "Keyword") {
            checkSpacingAround(firstToken);
        }
    }

    /**
     * Reports the first token of a given node if the first token is a keyword
     * and usage of spacing followed by the token is invalid.
     *
     * This is used for unary operators (e.g. `typeof`), `function`, and `super`.
     * Other rules are handling usage of spacing preceded by those keywords.
     *
     * @param {ASTNode|null} node - A node to report.
     * @returns {void}
     */
    function checkSpacingBeforeFirstToken(node) {
        var firstToken = node && sourceCode.getFirstToken(node);
        if (firstToken && firstToken.type === "Keyword") {
            checkSpacingBefore(firstToken);
        }
    }

    /**
     * Reports the previous token of a given node if the token is a keyword and
     * usage of spacing around the token is invalid.
     *
     * @param {ASTNode|null} node - A node to report.
     * @returns {void}
     */
    function checkSpacingAroundTokenBefore(node) {
        if (node) {
            var token = sourceCode.getTokenBefore(node);
            while (token.type !== "Keyword") {
                token = sourceCode.getTokenBefore(token);
            }

            checkSpacingAround(token);
        }
    }

    /**
     * Reports `class` and `extends` keywords of a given node if usage of
     * spacing around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForClass(node) {
        checkSpacingAroundFirstToken(node);
        checkSpacingAroundTokenBefore(node.superClass);
    }

    /**
     * Reports `if` and `else` keywords of a given node if usage of spacing
     * around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForIfStatement(node) {
        checkSpacingAroundFirstToken(node);
        checkSpacingAroundTokenBefore(node.alternate);
    }

    /**
     * Reports `try`, `catch`, and `finally` keywords of a given node if usage
     * of spacing around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForTryStatement(node) {
        checkSpacingAroundFirstToken(node);
        checkSpacingAroundFirstToken(node.handler);
        checkSpacingAroundTokenBefore(node.finalizer);
    }

    /**
     * Reports `do` and `while` keywords of a given node if usage of spacing
     * around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForDoWhileStatement(node) {
        checkSpacingAroundFirstToken(node);
        checkSpacingAroundTokenBefore(node.test);
    }

    /**
     * Reports `for` and `in` keywords of a given node if usage of spacing
     * around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForForInStatement(node) {
        checkSpacingAroundFirstToken(node);
        checkSpacingAroundTokenBefore(node.right);
    }

    /**
     * Reports `for` and `of` keywords of a given node if usage of spacing
     * around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForForOfStatement(node) {
        checkSpacingAroundFirstToken(node);

        // `of` is not a keyword token.
        var token = sourceCode.getTokenBefore(node.right);
        while (token.value !== "of") {
            token = sourceCode.getTokenBefore(token);
        }
        checkSpacingAround(token);
    }

    /**
     * Reports `import`, `export`, `as`, and `from` keywords of a given node if
     * usage of spacing around those keywords is invalid.
     *
     * This rule handles the `*` token in module declarations.
     *
     *     import*as A from "./a"; /*error Expected space(s) after "import".
     *                               error Expected space(s) before "as".
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForModuleDeclaration(node) {
        var firstToken = sourceCode.getFirstToken(node);
        checkSpacingBefore(firstToken, PREV_TOKEN_M);
        checkSpacingAfter(firstToken, NEXT_TOKEN_M);

        if (node.source) {
            var fromToken = sourceCode.getTokenBefore(node.source);
            checkSpacingBefore(fromToken, PREV_TOKEN_M);
            checkSpacingAfter(fromToken, NEXT_TOKEN_M);
        }
    }

    /**
     * Reports `as` keyword of a given node if usage of spacing around this
     * keyword is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForImportNamespaceSpecifier(node) {
        var asToken = sourceCode.getFirstToken(node, 1);
        checkSpacingBefore(asToken, PREV_TOKEN_M);
    }

    /**
     * Reports `static`, `get`, and `set` keywords of a given node if usage of
     * spacing around those keywords is invalid.
     *
     * @param {ASTNode} node - A node to report.
     * @returns {void}
     */
    function checkSpacingForProperty(node) {
        if (node.static) {
            checkSpacingAroundFirstToken(node);
        }
        if (node.kind === "get" || node.kind === "set") {
            var token = sourceCode.getFirstToken(
                node,
                node.static ? 1 : 0
            );
            checkSpacingAround(token);
        }
    }

    return {
        // Statements
        DebuggerStatement: checkSpacingAroundFirstToken,
        WithStatement: checkSpacingAroundFirstToken,

        // Statements - Control flow
        BreakStatement: checkSpacingAroundFirstToken,
        ContinueStatement: checkSpacingAroundFirstToken,
        ReturnStatement: checkSpacingAroundFirstToken,
        ThrowStatement: checkSpacingAroundFirstToken,
        TryStatement: checkSpacingForTryStatement,

        // Statements - Choice
        IfStatement: checkSpacingForIfStatement,
        SwitchStatement: checkSpacingAroundFirstToken,
        SwitchCase: checkSpacingAroundFirstToken,

        // Statements - Loops
        DoWhileStatement: checkSpacingForDoWhileStatement,
        ForInStatement: checkSpacingForForInStatement,
        ForOfStatement: checkSpacingForForOfStatement,
        ForStatement: checkSpacingAroundFirstToken,
        WhileStatement: checkSpacingAroundFirstToken,

        // Statements - Declarations
        ClassDeclaration: checkSpacingForClass,
        ExportNamedDeclaration: checkSpacingForModuleDeclaration,
        ExportDefaultDeclaration: checkSpacingAroundFirstToken,
        ExportAllDeclaration: checkSpacingForModuleDeclaration,
        FunctionDeclaration: checkSpacingBeforeFirstToken,
        ImportDeclaration: checkSpacingForModuleDeclaration,
        VariableDeclaration: checkSpacingAroundFirstToken,

        // Expressions
        ClassExpression: checkSpacingForClass,
        FunctionExpression: checkSpacingBeforeFirstToken,
        NewExpression: checkSpacingBeforeFirstToken,
        Super: checkSpacingBeforeFirstToken,
        ThisExpression: checkSpacingBeforeFirstToken,
        UnaryExpression: checkSpacingBeforeFirstToken,
        YieldExpression: checkSpacingBeforeFirstToken,

        // Others
        ImportNamespaceSpecifier: checkSpacingForImportNamespaceSpecifier,
        MethodDefinition: checkSpacingForProperty,
        Property: checkSpacingForProperty
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "before": {"type": "boolean"},
            "after": {"type": "boolean"},
            "overrides": {
                "type": "object",
                "properties": KEYS.reduce(function(retv, key) {
                    retv[key] = {
                        "type": "object",
                        "properties": {
                            "before": {"type": "boolean"},
                            "after": {"type": "boolean"}
                        },
                        "additionalProperties": false
                    };
                    return retv;
                }, {}),
                "additionalProperties": false
            }
        },
        "additionalProperties": false
    }
];
