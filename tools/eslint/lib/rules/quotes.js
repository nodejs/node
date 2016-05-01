/**
 * @fileoverview A rule to choose between single and double quote marks
 * @author Matt DuVall <http://www.mattduvall.com/>, Brandon Payton
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
    double: {
        quote: "\"",
        alternateQuote: "'",
        description: "doublequote"
    },
    single: {
        quote: "'",
        alternateQuote: "\"",
        description: "singlequote"
    },
    backtick: {
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

module.exports = {
    meta: {
        docs: {
            description: "enforce the consistent use of either backticks, double, or single quotes",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "code",

        schema: [
            {
                enum: ["single", "double", "backtick"]
            },
            {
                anyOf: [
                    {
                        enum: ["avoid-escape"]
                    },
                    {
                        type: "object",
                        properties: {
                            avoidEscape: {
                                type: "boolean"
                            },
                            allowTemplateLiterals: {
                                type: "boolean"
                            }
                        },
                        additionalProperties: false
                    }
                ]
            }
        ]
    },

    create: function(context) {

        var quoteOption = context.options[0],
            settings = QUOTE_SETTINGS[quoteOption || "double"],
            options = context.options[1],
            avoidEscape = options && options.avoidEscape === true,
            allowTemplateLiterals = options && options.allowTemplateLiterals === true,
            sourceCode = context.getSourceCode();

        // deprecated
        if (options === AVOID_ESCAPE) {
            avoidEscape = true;
        }

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

            Literal: function(node) {
                var val = node.value,
                    rawVal = node.raw,
                    isValid;

                if (settings && typeof val === "string") {
                    isValid = (quoteOption === "backtick" && isAllowedAsNonBacktick(node)) ||
                        isJSXElement(node.parent) ||
                        astUtils.isSurroundedBy(rawVal, settings.quote);

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
            },

            TemplateLiteral: function(node) {

                // If backticks are expected or it's a tagged template, then this shouldn't throw an errors
                if (allowTemplateLiterals || quoteOption === "backtick" || node.parent.type === "TaggedTemplateExpression") {
                    return;
                }

                var shouldWarn = node.quasis.length === 1 && (node.quasis[0].value.cooked.indexOf("\n") === -1);

                if (shouldWarn) {
                    context.report({
                        node: node,
                        message: "Strings must use " + settings.description + ".",
                        fix: function(fixer) {
                            return fixer.replaceText(node, settings.convert(sourceCode.getText(node)));
                        }
                    });
                }
            }
        };

    }
};
