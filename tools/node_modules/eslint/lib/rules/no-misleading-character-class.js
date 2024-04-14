/**
 * @author Toru Nagashima <https://github.com/mysticatea>
 */
"use strict";

const {
    CALL,
    CONSTRUCT,
    ReferenceTracker,
    getStaticValue,
    getStringIfConstant
} = require("@eslint-community/eslint-utils");
const { RegExpParser, visitRegExpAST } = require("@eslint-community/regexpp");
const { isCombiningCharacter, isEmojiModifier, isRegionalIndicatorSymbol, isSurrogatePair } = require("./utils/unicode");
const astUtils = require("./utils/ast-utils.js");
const { isValidWithUnicodeFlag } = require("./utils/regular-expressions");
const { parseStringLiteral, parseTemplateToken } = require("./utils/char-source");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * @typedef {import('@eslint-community/regexpp').AST.Character} Character
 * @typedef {import('@eslint-community/regexpp').AST.CharacterClassElement} CharacterClassElement
 */

/**
 * Iterate character sequences of a given nodes.
 *
 * CharacterClassRange syntax can steal a part of character sequence,
 * so this function reverts CharacterClassRange syntax and restore the sequence.
 * @param {CharacterClassElement[]} nodes The node list to iterate character sequences.
 * @returns {IterableIterator<Character[]>} The list of character sequences.
 */
function *iterateCharacterSequence(nodes) {

    /** @type {Character[]} */
    let seq = [];

    for (const node of nodes) {
        switch (node.type) {
            case "Character":
                seq.push(node);
                break;

            case "CharacterClassRange":
                seq.push(node.min);
                yield seq;
                seq = [node.max];
                break;

            case "CharacterSet":
            case "CharacterClass": // [[]] nesting character class
            case "ClassStringDisjunction": // \q{...}
            case "ExpressionCharacterClass": // [A--B]
                if (seq.length > 0) {
                    yield seq;
                    seq = [];
                }
                break;

            // no default
        }
    }

    if (seq.length > 0) {
        yield seq;
    }
}

/**
 * Checks whether the given character node is a Unicode code point escape or not.
 * @param {Character} char the character node to check.
 * @returns {boolean} `true` if the character node is a Unicode code point escape.
 */
function isUnicodeCodePointEscape(char) {
    return /^\\u\{[\da-f]+\}$/iu.test(char.raw);
}

/**
 * Each function returns matched characters if it detects that kind of problem.
 * @type {Record<string, (chars: Character[]) => IterableIterator<Character[]>>}
 */
const findCharacterSequences = {
    *surrogatePairWithoutUFlag(chars) {
        for (const [index, char] of chars.entries()) {
            if (index === 0) {
                continue;
            }
            const previous = chars[index - 1];

            if (
                isSurrogatePair(previous.value, char.value) &&
                !isUnicodeCodePointEscape(previous) &&
                !isUnicodeCodePointEscape(char)
            ) {
                yield [previous, char];
            }
        }
    },

    *surrogatePair(chars) {
        for (const [index, char] of chars.entries()) {
            if (index === 0) {
                continue;
            }
            const previous = chars[index - 1];

            if (
                isSurrogatePair(previous.value, char.value) &&
                (
                    isUnicodeCodePointEscape(previous) ||
                    isUnicodeCodePointEscape(char)
                )
            ) {
                yield [previous, char];
            }
        }
    },

    *combiningClass(chars) {
        for (const [index, char] of chars.entries()) {
            if (index === 0) {
                continue;
            }
            const previous = chars[index - 1];

            if (
                isCombiningCharacter(char.value) &&
                !isCombiningCharacter(previous.value)
            ) {
                yield [previous, char];
            }
        }
    },

    *emojiModifier(chars) {
        for (const [index, char] of chars.entries()) {
            if (index === 0) {
                continue;
            }
            const previous = chars[index - 1];

            if (
                isEmojiModifier(char.value) &&
                !isEmojiModifier(previous.value)
            ) {
                yield [previous, char];
            }
        }
    },

    *regionalIndicatorSymbol(chars) {
        for (const [index, char] of chars.entries()) {
            if (index === 0) {
                continue;
            }
            const previous = chars[index - 1];

            if (
                isRegionalIndicatorSymbol(char.value) &&
                isRegionalIndicatorSymbol(previous.value)
            ) {
                yield [previous, char];
            }
        }
    },

    *zwj(chars) {
        let sequence = null;

        for (const [index, char] of chars.entries()) {
            if (index === 0 || index === chars.length - 1) {
                continue;
            }
            if (
                char.value === 0x200d &&
                chars[index - 1].value !== 0x200d &&
                chars[index + 1].value !== 0x200d
            ) {
                if (sequence) {
                    if (sequence.at(-1) === chars[index - 1]) {
                        sequence.push(char, chars[index + 1]); // append to the sequence
                    } else {
                        yield sequence;
                        sequence = chars.slice(index - 1, index + 2);
                    }
                } else {
                    sequence = chars.slice(index - 1, index + 2);
                }
            }
        }

        if (sequence) {
            yield sequence;
        }
    }
};

const kinds = Object.keys(findCharacterSequences);

/**
 * Gets the value of the given node if it's a static value other than a regular expression object,
 * or the node's `regex` property.
 * The purpose of this method is to provide a replacement for `getStaticValue` in environments where certain regular expressions cannot be evaluated.
 * A known example is Node.js 18 which does not support the `v` flag.
 * Calling `getStaticValue` on a regular expression node with the `v` flag on Node.js 18 always returns `null`.
 * A limitation of this method is that it can only detect a regular expression if the specified node is itself a regular expression literal node.
 * @param {ASTNode | undefined} node The node to be inspected.
 * @param {Scope} initialScope Scope to start finding variables. This function tries to resolve identifier references which are in the given scope.
 * @returns {{ value: any } | { regex: { pattern: string, flags: string } } | null} The static value of the node, or `null`.
 */
function getStaticValueOrRegex(node, initialScope) {
    if (!node) {
        return null;
    }
    if (node.type === "Literal" && node.regex) {
        return { regex: node.regex };
    }

    const staticValue = getStaticValue(node, initialScope);

    if (staticValue?.value instanceof RegExp) {
        return null;
    }
    return staticValue;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "Disallow characters which are made with multiple code points in character class syntax",
            recommended: true,
            url: "https://eslint.org/docs/latest/rules/no-misleading-character-class"
        },

        hasSuggestions: true,

        schema: [],

        messages: {
            surrogatePairWithoutUFlag: "Unexpected surrogate pair in character class. Use 'u' flag.",
            surrogatePair: "Unexpected surrogate pair in character class.",
            combiningClass: "Unexpected combined character in character class.",
            emojiModifier: "Unexpected modified Emoji in character class.",
            regionalIndicatorSymbol: "Unexpected national flag in character class.",
            zwj: "Unexpected joined character sequence in character class.",
            suggestUnicodeFlag: "Add unicode 'u' flag to regex."
        }
    },
    create(context) {
        const sourceCode = context.sourceCode;
        const parser = new RegExpParser();
        const checkedPatternNodes = new Set();

        /**
         * Verify a given regular expression.
         * @param {Node} node The node to report.
         * @param {string} pattern The regular expression pattern to verify.
         * @param {string} flags The flags of the regular expression.
         * @param {Function} unicodeFixer Fixer for missing "u" flag.
         * @returns {void}
         */
        function verify(node, pattern, flags, unicodeFixer) {
            let patternNode;

            try {
                patternNode = parser.parsePattern(
                    pattern,
                    0,
                    pattern.length,
                    {
                        unicode: flags.includes("u"),
                        unicodeSets: flags.includes("v")
                    }
                );
            } catch {

                // Ignore regular expressions with syntax errors
                return;
            }

            const foundKindMatches = new Map();

            visitRegExpAST(patternNode, {
                onCharacterClassEnter(ccNode) {
                    for (const chars of iterateCharacterSequence(ccNode.elements)) {
                        for (const kind of kinds) {
                            if (foundKindMatches.has(kind)) {
                                foundKindMatches.get(kind).push(...findCharacterSequences[kind](chars));
                            } else {
                                foundKindMatches.set(kind, [...findCharacterSequences[kind](chars)]);
                            }
                        }
                    }
                }
            });

            let codeUnits = null;

            /**
             * Finds the report loc(s) for a range of matches.
             * Only literals and expression-less templates generate granular errors.
             * @param {Character[][]} matches Lists of individual characters being reported on.
             * @returns {Location[]} locs for context.report.
             * @see https://github.com/eslint/eslint/pull/17515
             */
            function getNodeReportLocations(matches) {
                if (!astUtils.isStaticTemplateLiteral(node) && node.type !== "Literal") {
                    return matches.length ? [node.loc] : [];
                }
                return matches.map(chars => {
                    const firstIndex = chars[0].start;
                    const lastIndex = chars.at(-1).end - 1;
                    let start;
                    let end;

                    if (node.type === "TemplateLiteral") {
                        const source = sourceCode.getText(node);
                        const offset = node.range[0];

                        codeUnits ??= parseTemplateToken(source);
                        start = offset + codeUnits[firstIndex].start;
                        end = offset + codeUnits[lastIndex].end;
                    } else if (typeof node.value === "string") { // String Literal
                        const source = node.raw;
                        const offset = node.range[0];

                        codeUnits ??= parseStringLiteral(source);
                        start = offset + codeUnits[firstIndex].start;
                        end = offset + codeUnits[lastIndex].end;
                    } else { // RegExp Literal
                        const offset = node.range[0] + 1; // Add 1 to skip the leading slash.

                        start = offset + firstIndex;
                        end = offset + lastIndex + 1;
                    }

                    return {
                        start: sourceCode.getLocFromIndex(start),
                        end: sourceCode.getLocFromIndex(end)
                    };
                });
            }

            for (const [kind, matches] of foundKindMatches) {
                let suggest;

                if (kind === "surrogatePairWithoutUFlag") {
                    suggest = [{
                        messageId: "suggestUnicodeFlag",
                        fix: unicodeFixer
                    }];
                }

                const locs = getNodeReportLocations(matches);

                for (const loc of locs) {
                    context.report({
                        node,
                        loc,
                        messageId: kind,
                        suggest
                    });
                }
            }
        }

        return {
            "Literal[regex]"(node) {
                if (checkedPatternNodes.has(node)) {
                    return;
                }
                verify(node, node.regex.pattern, node.regex.flags, fixer => {
                    if (!isValidWithUnicodeFlag(context.languageOptions.ecmaVersion, node.regex.pattern)) {
                        return null;
                    }

                    return fixer.insertTextAfter(node, "u");
                });
            },
            "Program"(node) {
                const scope = sourceCode.getScope(node);
                const tracker = new ReferenceTracker(scope);

                /*
                 * Iterate calls of RegExp.
                 * E.g., `new RegExp()`, `RegExp()`, `new window.RegExp()`,
                 *       `const {RegExp: a} = window; new a()`, etc...
                 */
                for (const { node: refNode } of tracker.iterateGlobalReferences({
                    RegExp: { [CALL]: true, [CONSTRUCT]: true }
                })) {
                    let pattern, flags;
                    const [patternNode, flagsNode] = refNode.arguments;
                    const evaluatedPattern = getStaticValueOrRegex(patternNode, scope);

                    if (!evaluatedPattern) {
                        continue;
                    }
                    if (flagsNode) {
                        if (evaluatedPattern.regex) {
                            pattern = evaluatedPattern.regex.pattern;
                            checkedPatternNodes.add(patternNode);
                        } else {
                            pattern = String(evaluatedPattern.value);
                        }
                        flags = getStringIfConstant(flagsNode, scope);
                    } else {
                        if (evaluatedPattern.regex) {
                            continue;
                        }
                        pattern = String(evaluatedPattern.value);
                        flags = "";
                    }

                    if (typeof flags === "string") {
                        verify(patternNode, pattern, flags, fixer => {

                            if (!isValidWithUnicodeFlag(context.languageOptions.ecmaVersion, pattern)) {
                                return null;
                            }

                            if (refNode.arguments.length === 1) {
                                const penultimateToken = sourceCode.getLastToken(refNode, { skip: 1 }); // skip closing parenthesis

                                return fixer.insertTextAfter(
                                    penultimateToken,
                                    astUtils.isCommaToken(penultimateToken)
                                        ? ' "u",'
                                        : ', "u"'
                                );
                            }

                            if ((flagsNode.type === "Literal" && typeof flagsNode.value === "string") || flagsNode.type === "TemplateLiteral") {
                                const range = [flagsNode.range[0], flagsNode.range[1] - 1];

                                return fixer.insertTextAfterRange(range, "u");
                            }

                            return null;
                        });
                    }
                }
            }
        };
    }
};
