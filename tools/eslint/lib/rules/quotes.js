/**
 * @fileoverview A rule to choose between single and double quote marks
 * @author Matt DuVall <http://www.mattduvall.com/>, Brandon Payton
 * @copyright 2013 Matt DuVall. All rights reserved.
 * See LICENSE file in root directory for full license.
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
    "double": {
        quote: "\"",
        alternateQuote: "'",
        description: "doublequote"
    },
    "single": {
        quote: "'",
        alternateQuote: "\"",
        description: "singlequote"
    },
    "backtick": {
        quote: "`",
        alternateQuote: "\"",
        description: "backtick"
    }
};
/**
 * Switches quoting of javascript string between ' " and `
 * escaping and unescaping as necessary.
 * Only escaping of the minimal set of characters is changed.
 * Note: escaping of newlines when switching from backtick to other quotes is not handled.
 * @param {string} str - A string to convert.
 * @returns {string} The string with changed quotes.
 * @private
 */
QUOTE_SETTINGS.double.convert =
QUOTE_SETTINGS.single.convert =
QUOTE_SETTINGS.backtick.convert = function(str) {
    var newQuote = this.quote;
    var oldQuote = str[0];
    if (newQuote === oldQuote) {
        return str;
    }
    return newQuote + str.slice(1, -1).replace(/\\(\${|\r\n?|\n|.)|["'`]|\${|(\r\n?|\n)/g, function(match, escaped, newline) {
        if (escaped === oldQuote || oldQuote === "`" && escaped === "${") {
            return escaped; // unescape
        }
        if (match === newQuote || newQuote === "`" && match === "${") {
            return "\\" + match; // escape
        }
        if (newline && oldQuote === "`") {
            return "\\n"; // escape newlines
        }
        return match;
    }) + newQuote;
};

var AVOID_ESCAPE = "avoid-escape",
    FUNCTION_TYPE = /^(?:Arrow)?Function(?:Declaration|Expression)$/;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    /**
     * Determines if a given node is part of JSX syntax.
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node is a JSX node, false if not.
     * @private
     */
    function isJSXElement(node) {
        return node.type.indexOf("JSX") === 0;
    }

    /**
     * Checks whether or not a given node is a directive.
     * The directive is a `ExpressionStatement` which has only a string literal.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} Whether or not the node is a directive.
     * @private
     */
    function isDirective(node) {
        return (
            node.type === "ExpressionStatement" &&
            node.expression.type === "Literal" &&
            typeof node.expression.value === "string"
        );
    }

    /**
     * Checks whether or not a given node is a part of directive prologues.
     * See also: http://www.ecma-international.org/ecma-262/6.0/#sec-directive-prologues-and-the-use-strict-directive
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} Whether or not the node is a part of directive prologues.
     * @private
     */
    function isPartOfDirectivePrologue(node) {
        var block = node.parent.parent;
        if (block.type !== "Program" && (block.type !== "BlockStatement" || !FUNCTION_TYPE.test(block.parent.type))) {
            return false;
        }

        // Check the node is at a prologue.
        for (var i = 0; i < block.body.length; ++i) {
            var statement = block.body[i];

            if (statement === node.parent) {
                return true;
            }
            if (!isDirective(statement)) {
                break;
            }
        }

        return false;
    }

    /**
     * Checks whether or not a given node is allowed as non backtick.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} Whether or not the node is allowed as non backtick.
     * @private
     */
    function isAllowedAsNonBacktick(node) {
        var parent = node.parent;

        switch (parent.type) {
            // Directive Prologues.
            case "ExpressionStatement":
                return isPartOfDirectivePrologue(node);

            // LiteralPropertyName.
            case "Property":
                return parent.key === node && !parent.computed;

            // ModuleSpecifier.
            case "ImportDeclaration":
            case "ExportNamedDeclaration":
            case "ExportAllDeclaration":
                return parent.source === node;

            // Others don't allow.
            default:
                return false;
        }
    }

    return {

        "Literal": function(node) {
            var val = node.value,
                rawVal = node.raw,
                quoteOption = context.options[0],
                settings = QUOTE_SETTINGS[quoteOption || "double"],
                avoidEscape = context.options[1] === AVOID_ESCAPE,
                isValid;

            if (settings && typeof val === "string") {
                isValid = (quoteOption === "backtick" && isAllowedAsNonBacktick(node)) || isJSXElement(node.parent) || astUtils.isSurroundedBy(rawVal, settings.quote);

                if (!isValid && avoidEscape) {
                    isValid = astUtils.isSurroundedBy(rawVal, settings.alternateQuote) && rawVal.indexOf(settings.quote) >= 0;
                }

                if (!isValid) {
                    context.report({
                        node: node,
                        message: "Strings must use " + settings.description + ".",
                        fix: function(fixer) {
                            return fixer.replaceText(node, settings.convert(node.raw));
                        }
                    });
                }
            }
        }
    };

};

module.exports.schema = [
    {
        "enum": ["single", "double", "backtick"]
    },
    {
        "enum": ["avoid-escape"]
    }
];
