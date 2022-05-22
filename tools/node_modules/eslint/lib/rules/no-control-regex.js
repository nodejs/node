/**
 * @fileoverview Rule to forbid control characters from regular expressions.
 * @author Nicholas C. Zakas
 */

"use strict";

const RegExpValidator = require("regexpp").RegExpValidator;
const collector = new (class {
    constructor() {
        this._source = "";
        this._controlChars = [];
        this._validator = new RegExpValidator(this);
    }

    onPatternEnter() {
        this._controlChars = [];
    }

    onCharacter(start, end, cp) {
        if (cp >= 0x00 &&
            cp <= 0x1F &&
            (
                this._source.codePointAt(start) === cp ||
                this._source.slice(start, end).startsWith("\\x") ||
                this._source.slice(start, end).startsWith("\\u")
            )
        ) {
            this._controlChars.push(`\\x${`0${cp.toString(16)}`.slice(-2)}`);
        }
    }

    collectControlChars(regexpStr, flags) {
        const uFlag = typeof flags === "string" && flags.includes("u");

        try {
            this._source = regexpStr;
            this._validator.validatePattern(regexpStr, void 0, void 0, uFlag); // Call onCharacter hook
        } catch {

            // Ignore syntax errors in RegExp.
        }
        return this._controlChars;
    }
})();

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow control characters in regular expressions",
            recommended: true,
            url: "https://eslint.org/docs/rules/no-control-regex"
        },

        schema: [],

        messages: {
            unexpected: "Unexpected control character(s) in regular expression: {{controlChars}}."
        }
    },

    create(context) {

        /**
         * Get the regex expression
         * @param {ASTNode} node `Literal` node to evaluate
         * @returns {{ pattern: string, flags: string | null } | null} Regex if found (the given node is either a regex literal
         * or a string literal that is the pattern argument of a RegExp constructor call). Otherwise `null`. If flags cannot be determined,
         * the `flags` property will be `null`.
         * @private
         */
        function getRegExp(node) {
            if (node.regex) {
                return node.regex;
            }
            if (typeof node.value === "string" &&
                (node.parent.type === "NewExpression" || node.parent.type === "CallExpression") &&
                node.parent.callee.type === "Identifier" &&
                node.parent.callee.name === "RegExp" &&
                node.parent.arguments[0] === node
            ) {
                const pattern = node.value;
                const flags =
                    node.parent.arguments.length > 1 &&
                    node.parent.arguments[1].type === "Literal" &&
                    typeof node.parent.arguments[1].value === "string"
                        ? node.parent.arguments[1].value
                        : null;

                return { pattern, flags };
            }

            return null;
        }

        return {
            Literal(node) {
                const regExp = getRegExp(node);

                if (regExp) {
                    const { pattern, flags } = regExp;
                    const controlCharacters = collector.collectControlChars(pattern, flags);

                    if (controlCharacters.length > 0) {
                        context.report({
                            node,
                            messageId: "unexpected",
                            data: {
                                controlChars: controlCharacters.join(", ")
                            }
                        });
                    }
                }
            }
        };

    }
};
