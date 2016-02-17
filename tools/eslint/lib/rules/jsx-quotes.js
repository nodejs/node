/**
 * @fileoverview A rule to ensure consistent quotes used in jsx syntax.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2015 Mathias Schreck
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var QUOTE_SETTINGS = {
    "prefer-double": {
        quote: "\"",
        description: "singlequote"
    },
    "prefer-single": {
        quote: "'",
        description: "doublequote"
    }
};

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var quoteOption = context.options[0] || "prefer-double",
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
        "JSXAttribute": function(node) {
            var attributeValue = node.value;

            if (attributeValue && astUtils.isStringLiteral(attributeValue) && !usesExpectedQuotes(attributeValue)) {
                context.report(attributeValue, "Unexpected usage of {{description}}.", setting);
            }
        }
    };
};

module.exports.schema = [
    {
        "enum": [ "prefer-single", "prefer-double" ]
    }
];
