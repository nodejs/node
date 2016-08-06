/**
 * @fileoverview A rule to ensure consistent quotes used in jsx syntax.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

let QUOTE_SETTINGS = {
    "prefer-double": {
        quote: "\"",
        description: "singlequote",
        convert: function(str) {
            return str.replace(/'/g, "\"");
        }
    },
    "prefer-single": {
        quote: "'",
        description: "doublequote",
        convert: function(str) {
            return str.replace(/"/g, "'");
        }
    }
};

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce the consistent use of either double or single quotes in JSX attributes",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                enum: [ "prefer-single", "prefer-double" ]
            }
        ]
    },

    create: function(context) {
        let quoteOption = context.options[0] || "prefer-double",
            setting = QUOTE_SETTINGS[quoteOption];

        /**
         * Checks if the given string literal node uses the expected quotes
         * @param {ASTNode} node - A string literal node.
         * @returns {boolean} Whether or not the string literal used the expected quotes.
         * @public
         */
        function usesExpectedQuotes(node) {
            return node.value.indexOf(setting.quote) !== -1 || astUtils.isSurroundedBy(node.raw, setting.quote);
        }

        return {
            JSXAttribute: function(node) {
                let attributeValue = node.value;

                if (attributeValue && astUtils.isStringLiteral(attributeValue) && !usesExpectedQuotes(attributeValue)) {
                    context.report({
                        node: attributeValue,
                        message: "Unexpected usage of " + setting.description + ".",
                        fix: function(fixer) {
                            return fixer.replaceText(attributeValue, setting.convert(attributeValue.raw));
                        }
                    });
                }
            }
        };
    }
};
