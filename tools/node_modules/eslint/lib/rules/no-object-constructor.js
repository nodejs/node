/**
 * @fileoverview Rule to disallow calls to the `Object` constructor without an argument
 * @author Francesco Trotta
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const { getVariableByName, isArrowToken, isClosingBraceToken, isClosingParenToken } = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const BREAK_OR_CONTINUE = new Set(["BreakStatement", "ContinueStatement"]);

// Declaration types that must contain a string Literal node at the end.
const DECLARATIONS = new Set(["ExportAllDeclaration", "ExportNamedDeclaration", "ImportDeclaration"]);

const IDENTIFIER_OR_KEYWORD = new Set(["Identifier", "Keyword"]);

// Keywords that can immediately precede an ExpressionStatement node, mapped to the their node types.
const NODE_TYPES_BY_KEYWORD = {
    __proto__: null,
    break: "BreakStatement",
    continue: "ContinueStatement",
    debugger: "DebuggerStatement",
    do: "DoWhileStatement",
    else: "IfStatement",
    return: "ReturnStatement",
    yield: "YieldExpression"
};

/*
 * Before an opening parenthesis, `>` (for JSX), and postfix `++` and `--` always trigger ASI;
 * the tokens `:`, `;`, `{` and `=>` don't expect a semicolon, as that would count as an empty statement.
 */
const PUNCTUATORS = new Set([":", ";", ">", "{", "=>", "++", "--"]);

/*
 * Statements that can contain an `ExpressionStatement` after a closing parenthesis.
 * DoWhileStatement is an exception in that it always triggers ASI after the closing parenthesis.
 */
const STATEMENTS = new Set([
    "DoWhileStatement",
    "ForInStatement",
    "ForOfStatement",
    "ForStatement",
    "IfStatement",
    "WhileStatement",
    "WithStatement"
]);

/**
 * Tests if a node appears at the beginning of an ancestor ExpressionStatement node.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} Whether the node appears at the beginning of an ancestor ExpressionStatement node.
 */
function isStartOfExpressionStatement(node) {
    const start = node.range[0];
    let ancestor = node;

    while ((ancestor = ancestor.parent) && ancestor.range[0] === start) {
        if (ancestor.type === "ExpressionStatement") {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "Disallow calls to the `Object` constructor without an argument",
            recommended: false,
            url: "https://eslint.org/docs/latest/rules/no-object-constructor"
        },

        hasSuggestions: true,

        schema: [],

        messages: {
            preferLiteral: "The object literal notation {} is preferable.",
            useLiteral: "Replace with '{{replacement}}'.",
            useLiteralAfterSemicolon: "Replace with '{{replacement}}', add preceding semicolon."
        }
    },

    create(context) {

        const sourceCode = context.sourceCode;

        /**
         * Determines whether or not an object literal that replaces a specified node needs to be enclosed in parentheses.
         * @param {ASTNode} node The node to be replaced.
         * @returns {boolean} Whether or not parentheses around the object literal are required.
         */
        function needsParentheses(node) {
            if (isStartOfExpressionStatement(node)) {
                return true;
            }

            const prevToken = sourceCode.getTokenBefore(node);

            if (prevToken && isArrowToken(prevToken)) {
                return true;
            }

            return false;
        }

        /**
         * Determines whether a parenthesized object literal that replaces a specified node needs to be preceded by a semicolon.
         * @param {ASTNode} node The node to be replaced. This node should be at the start of an `ExpressionStatement` or at the start of the body of an `ArrowFunctionExpression`.
         * @returns {boolean} Whether a semicolon is required before the parenthesized object literal.
         */
        function needsSemicolon(node) {
            const prevToken = sourceCode.getTokenBefore(node);

            if (!prevToken || prevToken.type === "Punctuator" && PUNCTUATORS.has(prevToken.value)) {
                return false;
            }

            const prevNode = sourceCode.getNodeByRangeIndex(prevToken.range[0]);

            if (isClosingParenToken(prevToken)) {
                return !STATEMENTS.has(prevNode.type);
            }

            if (isClosingBraceToken(prevToken)) {
                return (
                    prevNode.type === "BlockStatement" && prevNode.parent.type === "FunctionExpression" ||
                    prevNode.type === "ClassBody" && prevNode.parent.type === "ClassExpression" ||
                    prevNode.type === "ObjectExpression"
                );
            }

            if (IDENTIFIER_OR_KEYWORD.has(prevToken.type)) {
                if (BREAK_OR_CONTINUE.has(prevNode.parent.type)) {
                    return false;
                }

                const keyword = prevToken.value;
                const nodeType = NODE_TYPES_BY_KEYWORD[keyword];

                return prevNode.type !== nodeType;
            }

            if (prevToken.type === "String") {
                return !DECLARATIONS.has(prevNode.parent.type);
            }

            return true;
        }

        /**
         * Reports on nodes where the `Object` constructor is called without arguments.
         * @param {ASTNode} node The node to evaluate.
         * @returns {void}
         */
        function check(node) {
            if (node.callee.type !== "Identifier" || node.callee.name !== "Object" || node.arguments.length) {
                return;
            }

            const variable = getVariableByName(sourceCode.getScope(node), "Object");

            if (variable && variable.identifiers.length === 0) {
                let replacement;
                let fixText;
                let messageId = "useLiteral";

                if (needsParentheses(node)) {
                    replacement = "({})";
                    if (needsSemicolon(node)) {
                        fixText = ";({})";
                        messageId = "useLiteralAfterSemicolon";
                    } else {
                        fixText = "({})";
                    }
                } else {
                    replacement = fixText = "{}";
                }

                context.report({
                    node,
                    messageId: "preferLiteral",
                    suggest: [
                        {
                            messageId,
                            data: { replacement },
                            fix: fixer => fixer.replaceText(node, fixText)
                        }
                    ]
                });
            }
        }

        return {
            CallExpression: check,
            NewExpression: check
        };

    }
};
