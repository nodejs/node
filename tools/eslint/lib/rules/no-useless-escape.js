/**
 * @fileoverview Look for useless escapes in strings and regexes
 * @author Onur Temizkan
 * @copyright 2016 Onur Temizkan. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var VALID_STRING_ESCAPES = [
    "\\",
    "n",
    "r",
    "v",
    "t",
    "b",
    "f",
    "u",
    "x",
    "\n",
    "\r"
];

var VALID_REGEX_ESCAPES = [
    "\\",
    ".",
    "-",
    "^",
    "$",
    "*",
    "+",
    "?",
    "{",
    "}",
    "[",
    "]",
    "|",
    "(",
    ")",
    "b",
    "B",
    "c",
    "d",
    "D",
    "f",
    "n",
    "r",
    "s",
    "S",
    "t",
    "v",
    "w",
    "W",
    "x",
    "u"
];

module.exports = function(context) {

    /**
     * Checks if the escape character in given slice is unnecessary.
     *
     * @private
     * @param {string} elm - string slice to validate.
     * @param {ASTNode} node - node to validate.
     * @returns {void}
     * @this escapes_quote_node
     */
    function validate(elm) {
        var escapeNotFound = this.escapes.indexOf(elm[1]) === -1;
        var isQuoteEscape = elm[1] === this.node.raw[0];

        if (escapeNotFound && !isQuoteEscape) {
            context.report(this.node, "Unnecessary escape character: " + elm);
        }
    }

    /**
     * Checks if a node has an escape.
     *
     * @param {ASTNode} node - node to check.
     * @returns {void}
     */
    function check(node) {
        var nodeEscapes;

        if (typeof node.value === "string") {
            nodeEscapes = VALID_STRING_ESCAPES;
        } else if (node.regex) {
            nodeEscapes = VALID_REGEX_ESCAPES;
        } else {
            return;
        }

        (node.raw.match(/\\[^\d]/g) || []).forEach(validate, {
            "escapes": nodeEscapes,
            "node": node
        });
    }
    return {
        "Literal": check
    };
};

module.exports.schema = [];
