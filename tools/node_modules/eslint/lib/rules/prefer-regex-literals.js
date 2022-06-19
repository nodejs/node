/**
 * @fileoverview Rule to disallow use of the `RegExp` constructor in favor of regular expression literals
 * @author Milos Djermanovic
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("./utils/ast-utils");
const { CALL, CONSTRUCT, ReferenceTracker, findVariable } = require("eslint-utils");
const { RegExpValidator, visitRegExpAST, RegExpParser } = require("regexpp");
const { canTokensBeAdjacent } = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const REGEXPP_LATEST_ECMA_VERSION = 2022;

/**
 * Determines whether the given node is a string literal.
 * @param {ASTNode} node Node to check.
 * @returns {boolean} True if the node is a string literal.
 */
function isStringLiteral(node) {
    return node.type === "Literal" && typeof node.value === "string";
}

/**
 * Determines whether the given node is a regex literal.
 * @param {ASTNode} node Node to check.
 * @returns {boolean} True if the node is a regex literal.
 */
function isRegexLiteral(node) {
    return node.type === "Literal" && Object.prototype.hasOwnProperty.call(node, "regex");
}

/**
 * Determines whether the given node is a template literal without expressions.
 * @param {ASTNode} node Node to check.
 * @returns {boolean} True if the node is a template literal without expressions.
 */
function isStaticTemplateLiteral(node) {
    return node.type === "TemplateLiteral" && node.expressions.length === 0;
}

const validPrecedingTokens = new Set([
    "(",
    ";",
    "[",
    ",",
    "=",
    "+",
    "*",
    "-",
    "?",
    "~",
    "%",
    "**",
    "!",
    "typeof",
    "instanceof",
    "&&",
    "||",
    "??",
    "return",
    "...",
    "delete",
    "void",
    "in",
    "<",
    ">",
    "<=",
    ">=",
    "==",
    "===",
    "!=",
    "!==",
    "<<",
    ">>",
    ">>>",
    "&",
    "|",
    "^",
    ":",
    "{",
    "=>",
    "*=",
    "<<=",
    ">>=",
    ">>>=",
    "^=",
    "|=",
    "&=",
    "??=",
    "||=",
    "&&=",
    "**=",
    "+=",
    "-=",
    "/=",
    "%=",
    "/",
    "do",
    "break",
    "continue",
    "debugger",
    "case",
    "throw"
]);


//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow use of the `RegExp` constructor in favor of regular expression literals",
            recommended: false,
            url: "https://eslint.org/docs/rules/prefer-regex-literals"
        },

        hasSuggestions: true,

        schema: [
            {
                type: "object",
                properties: {
                    disallowRedundantWrapping: {
                        type: "boolean",
                        default: false
                    }
                },
                additionalProperties: false
            }
        ],

        messages: {
            unexpectedRegExp: "Use a regular expression literal instead of the 'RegExp' constructor.",
            replaceWithLiteral: "Replace with an equivalent regular expression literal.",
            unexpectedRedundantRegExp: "Regular expression literal is unnecessarily wrapped within a 'RegExp' constructor.",
            unexpectedRedundantRegExpWithFlags: "Use regular expression literal with flags instead of the 'RegExp' constructor."
        }
    },

    create(context) {
        const [{ disallowRedundantWrapping = false } = {}] = context.options;
        const sourceCode = context.getSourceCode();

        /**
         * Determines whether the given identifier node is a reference to a global variable.
         * @param {ASTNode} node `Identifier` node to check.
         * @returns {boolean} True if the identifier is a reference to a global variable.
         */
        function isGlobalReference(node) {
            const scope = context.getScope();
            const variable = findVariable(scope, node);

            return variable !== null && variable.scope.type === "global" && variable.defs.length === 0;
        }

        /**
         * Determines whether the given node is a String.raw`` tagged template expression
         * with a static template literal.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} True if the node is String.raw`` with a static template.
         */
        function isStringRawTaggedStaticTemplateLiteral(node) {
            return node.type === "TaggedTemplateExpression" &&
                astUtils.isSpecificMemberAccess(node.tag, "String", "raw") &&
                isGlobalReference(astUtils.skipChainExpression(node.tag).object) &&
                isStaticTemplateLiteral(node.quasi);
        }

        /**
         * Gets the value of a string
         * @param {ASTNode} node The node to get the string of.
         * @returns {string|null} The value of the node.
         */
        function getStringValue(node) {
            if (isStringLiteral(node)) {
                return node.value;
            }

            if (isStaticTemplateLiteral(node)) {
                return node.quasis[0].value.cooked;
            }

            if (isStringRawTaggedStaticTemplateLiteral(node)) {
                return node.quasi.quasis[0].value.raw;
            }

            return null;
        }

        /**
         * Determines whether the given node is considered to be a static string by the logic of this rule.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} True if the node is a static string.
         */
        function isStaticString(node) {
            return isStringLiteral(node) ||
                isStaticTemplateLiteral(node) ||
                isStringRawTaggedStaticTemplateLiteral(node);
        }

        /**
         * Determines whether the relevant arguments of the given are all static string literals.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} True if all arguments are static strings.
         */
        function hasOnlyStaticStringArguments(node) {
            const args = node.arguments;

            if ((args.length === 1 || args.length === 2) && args.every(isStaticString)) {
                return true;
            }

            return false;
        }

        /**
         * Determines whether the arguments of the given node indicate that a regex literal is unnecessarily wrapped.
         * @param {ASTNode} node Node to check.
         * @returns {boolean} True if the node already contains a regex literal argument.
         */
        function isUnnecessarilyWrappedRegexLiteral(node) {
            const args = node.arguments;

            if (args.length === 1 && isRegexLiteral(args[0])) {
                return true;
            }

            if (args.length === 2 && isRegexLiteral(args[0]) && isStaticString(args[1])) {
                return true;
            }

            return false;
        }

        /**
         * Returns a ecmaVersion compatible for regexpp.
         * @param {any} ecmaVersion The ecmaVersion to convert.
         * @returns {import("regexpp/ecma-versions").EcmaVersion} The resulting ecmaVersion compatible for regexpp.
         */
        function getRegexppEcmaVersion(ecmaVersion) {
            if (typeof ecmaVersion !== "number" || ecmaVersion <= 5) {
                return 5;
            }
            return Math.min(ecmaVersion + 2009, REGEXPP_LATEST_ECMA_VERSION);
        }

        /**
         * Makes a character escaped or else returns null.
         * @param {string} character The character to escape.
         * @returns {string} The resulting escaped character.
         */
        function resolveEscapes(character) {
            switch (character) {
                case "\n":
                case "\\\n":
                    return "\\n";

                case "\r":
                case "\\\r":
                    return "\\r";

                case "\t":
                case "\\\t":
                    return "\\t";

                case "\v":
                case "\\\v":
                    return "\\v";

                case "\f":
                case "\\\f":
                    return "\\f";

                case "/":
                    return "\\/";

                default:
                    return null;
            }
        }

        return {
            Program() {
                const scope = context.getScope();
                const tracker = new ReferenceTracker(scope);
                const traceMap = {
                    RegExp: {
                        [CALL]: true,
                        [CONSTRUCT]: true
                    }
                };

                for (const { node } of tracker.iterateGlobalReferences(traceMap)) {
                    if (disallowRedundantWrapping && isUnnecessarilyWrappedRegexLiteral(node)) {
                        if (node.arguments.length === 2) {
                            context.report({ node, messageId: "unexpectedRedundantRegExpWithFlags" });
                        } else {
                            context.report({ node, messageId: "unexpectedRedundantRegExp" });
                        }
                    } else if (hasOnlyStaticStringArguments(node)) {
                        let regexContent = getStringValue(node.arguments[0]);
                        let noFix = false;
                        let flags;

                        if (node.arguments[1]) {
                            flags = getStringValue(node.arguments[1]);
                        }

                        const regexppEcmaVersion = getRegexppEcmaVersion(context.parserOptions.ecmaVersion);
                        const RegExpValidatorInstance = new RegExpValidator({ ecmaVersion: regexppEcmaVersion });

                        try {
                            RegExpValidatorInstance.validatePattern(regexContent, 0, regexContent.length, flags ? flags.includes("u") : false);
                            if (flags) {
                                RegExpValidatorInstance.validateFlags(flags);
                            }
                        } catch {
                            noFix = true;
                        }

                        const tokenBefore = sourceCode.getTokenBefore(node);

                        if (tokenBefore && !validPrecedingTokens.has(tokenBefore.value)) {
                            noFix = true;
                        }

                        if (!/^[-a-zA-Z0-9\\[\](){} \t\r\n\v\f!@#$%^&*+^_=/~`.><?,'"|:;]*$/u.test(regexContent)) {
                            noFix = true;
                        }

                        if (sourceCode.getCommentsInside(node).length > 0) {
                            noFix = true;
                        }

                        if (regexContent && !noFix) {
                            let charIncrease = 0;

                            const ast = new RegExpParser({ ecmaVersion: regexppEcmaVersion }).parsePattern(regexContent, 0, regexContent.length, flags ? flags.includes("u") : false);

                            visitRegExpAST(ast, {
                                onCharacterEnter(characterNode) {
                                    const escaped = resolveEscapes(characterNode.raw);

                                    if (escaped) {
                                        regexContent =
                                            regexContent.slice(0, characterNode.start + charIncrease) +
                                            escaped +
                                            regexContent.slice(characterNode.end + charIncrease);

                                        if (characterNode.raw.length === 1) {
                                            charIncrease += 1;
                                        }
                                    }
                                }
                            });
                        }

                        const newRegExpValue = `/${regexContent || "(?:)"}/${flags || ""}`;

                        context.report({
                            node,
                            messageId: "unexpectedRegExp",
                            suggest: noFix ? [] : [{
                                messageId: "replaceWithLiteral",
                                fix(fixer) {
                                    const tokenAfter = sourceCode.getTokenAfter(node);

                                    return fixer.replaceText(
                                        node,
                                        (tokenBefore && !canTokensBeAdjacent(tokenBefore, newRegExpValue) && tokenBefore.range[1] === node.range[0] ? " " : "") +
                                            newRegExpValue +
                                            (tokenAfter && !canTokensBeAdjacent(newRegExpValue, tokenAfter) && node.range[1] === tokenAfter.range[0] ? " " : "")
                                    );
                                }
                            }]
                        });
                    }
                }
            }
        };
    }
};
