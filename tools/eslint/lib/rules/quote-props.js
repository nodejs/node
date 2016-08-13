/**
 * @fileoverview Rule to flag non-quoted property names in object literals.
 * @author Mathias Bynens <http://mathiasbynens.be/>
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const espree = require("espree"),
    keywords = require("../util/keywords");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require quotes around object literal property names",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: {
            anyOf: [
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "as-needed", "consistent", "consistent-as-needed"]
                        }
                    ],
                    minItems: 0,
                    maxItems: 1
                },
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "as-needed", "consistent", "consistent-as-needed"]
                        },
                        {
                            type: "object",
                            properties: {
                                keywords: {
                                    type: "boolean"
                                },
                                unnecessary: {
                                    type: "boolean"
                                },
                                numbers: {
                                    type: "boolean"
                                }
                            },
                            additionalProperties: false
                        }
                    ],
                    minItems: 0,
                    maxItems: 2
                }
            ]
        }
    },

    create: function(context) {

        const MODE = context.options[0],
            KEYWORDS = context.options[1] && context.options[1].keywords,
            CHECK_UNNECESSARY = !context.options[1] || context.options[1].unnecessary !== false,
            NUMBERS = context.options[1] && context.options[1].numbers,

            MESSAGE_UNNECESSARY = "Unnecessarily quoted property '{{property}}' found.",
            MESSAGE_UNQUOTED = "Unquoted property '{{property}}' found.",
            MESSAGE_NUMERIC = "Unquoted number literal '{{property}}' used as key.",
            MESSAGE_RESERVED = "Unquoted reserved word '{{property}}' used as key.";


        /**
         * Checks whether a certain string constitutes an ES3 token
         * @param   {string} tokenStr - The string to be checked.
         * @returns {boolean} `true` if it is an ES3 token.
         */
        function isKeyword(tokenStr) {
            return keywords.indexOf(tokenStr) >= 0;
        }

        /**
         * Checks if an espree-tokenized key has redundant quotes (i.e. whether quotes are unnecessary)
         * @param   {string} rawKey The raw key value from the source
         * @param   {espreeTokens} tokens The espree-tokenized node key
         * @param   {boolean} [skipNumberLiterals=false] Indicates whether number literals should be checked
         * @returns {boolean} Whether or not a key has redundant quotes.
         * @private
         */
        function areQuotesRedundant(rawKey, tokens, skipNumberLiterals) {
            return tokens.length === 1 && tokens[0].start === 0 && tokens[0].end === rawKey.length &&
                (["Identifier", "Keyword", "Null", "Boolean"].indexOf(tokens[0].type) >= 0 ||
                (tokens[0].type === "Numeric" && !skipNumberLiterals && "" + +tokens[0].value === tokens[0].value));
        }

        /**
         * Ensures that a property's key is quoted only when necessary
         * @param   {ASTNode} node Property AST node
         * @returns {void}
         */
        function checkUnnecessaryQuotes(node) {
            const key = node.key;
            let tokens;

            if (node.method || node.computed || node.shorthand) {
                return;
            }

            if (key.type === "Literal" && typeof key.value === "string") {
                try {
                    tokens = espree.tokenize(key.value);
                } catch (e) {
                    return;
                }

                if (tokens.length !== 1) {
                    return;
                }

                const isKeywordToken = isKeyword(tokens[0].value);

                if (isKeywordToken && KEYWORDS) {
                    return;
                }

                if (CHECK_UNNECESSARY && areQuotesRedundant(key.value, tokens, NUMBERS)) {
                    context.report(node, MESSAGE_UNNECESSARY, {property: key.value});
                }
            } else if (KEYWORDS && key.type === "Identifier" && isKeyword(key.name)) {
                context.report(node, MESSAGE_RESERVED, {property: key.name});
            } else if (NUMBERS && key.type === "Literal" && typeof key.value === "number") {
                context.report(node, MESSAGE_NUMERIC, {property: key.value});
            }
        }

        /**
         * Ensures that a property's key is quoted
         * @param   {ASTNode} node Property AST node
         * @returns {void}
         */
        function checkOmittedQuotes(node) {
            const key = node.key;

            if (!node.method && !node.computed && !node.shorthand && !(key.type === "Literal" && typeof key.value === "string")) {
                context.report(node, MESSAGE_UNQUOTED, {
                    property: key.name || key.value
                });
            }
        }

        /**
         * Ensures that an object's keys are consistently quoted, optionally checks for redundancy of quotes
         * @param   {ASTNode} node Property AST node
         * @param   {boolean} checkQuotesRedundancy Whether to check quotes' redundancy
         * @returns {void}
         */
        function checkConsistency(node, checkQuotesRedundancy) {
            let quotes = false,
                lackOfQuotes = false,
                necessaryQuotes = false;

            node.properties.forEach(function(property) {
                const key = property.key;
                let tokens;

                if (!key || property.method || property.computed || property.shorthand) {
                    return;
                }

                if (key.type === "Literal" && typeof key.value === "string") {

                    quotes = true;

                    if (checkQuotesRedundancy) {
                        try {
                            tokens = espree.tokenize(key.value);
                        } catch (e) {
                            necessaryQuotes = true;
                            return;
                        }

                        necessaryQuotes = necessaryQuotes || !areQuotesRedundant(key.value, tokens) || KEYWORDS && isKeyword(tokens[0].value);
                    }
                } else if (KEYWORDS && checkQuotesRedundancy && key.type === "Identifier" && isKeyword(key.name)) {
                    necessaryQuotes = true;
                    context.report(node, "Properties should be quoted as '{{property}}' is a reserved word.", {property: key.name});
                } else {
                    lackOfQuotes = true;
                }

                if (quotes && lackOfQuotes) {
                    context.report(node, "Inconsistently quoted property '{{key}}' found.", {
                        key: key.name || key.value
                    });
                }
            });

            if (checkQuotesRedundancy && quotes && !necessaryQuotes) {
                context.report(node, "Properties shouldn't be quoted as all quotes are redundant.");
            }
        }

        return {
            Property: function(node) {
                if (MODE === "always" || !MODE) {
                    checkOmittedQuotes(node);
                }
                if (MODE === "as-needed") {
                    checkUnnecessaryQuotes(node);
                }
            },
            ObjectExpression: function(node) {
                if (MODE === "consistent") {
                    checkConsistency(node, false);
                }
                if (MODE === "consistent-as-needed") {
                    checkConsistency(node, true);
                }
            }
        };

    }
};
