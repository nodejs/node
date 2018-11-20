/**
 * @fileoverview Disallows or enforces spaces inside of brackets.
 * @author Ian Christian Myers
 * @copyright 2014 Brandyn Bennett. All rights reserved.
 * @copyright 2014 Michael Ficarra. No rights reserved.
 * @copyright 2014 Vignesh Anand. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var spaced = context.options[0] === "always";

    /**
     * Determines whether an option is set, relative to the spacing option.
     * If spaced is "always", then check whether option is set to false.
     * If spaced is "never", then check whether option is set to true.
     * @param {Object} option - The option to exclude.
     * @returns {boolean} Whether or not the property is excluded.
     */
    function isOptionSet(option) {
        return context.options[1] != null ? context.options[1][option] === !spaced : false;
    }

    var options = {
        spaced: spaced,
        singleElementException: isOptionSet("singleValue"),
        objectsInArraysException: isOptionSet("objectsInArrays"),
        arraysInArraysException: isOptionSet("arraysInArrays"),
        arraysInObjectsException: isOptionSet("arraysInObjects"),
        objectsInObjectsException: isOptionSet("objectsInObjects"),
        propertyNameException: isOptionSet("propertyName")
    };

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Determines whether two adjacent tokens are have whitespace between them.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not there is space between the tokens.
     */
    function isSpaced(left, right) {
        return left.range[1] < right.range[0];
    }

    /**
     * Determines whether two adjacent tokens are on the same line.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not the tokens are on the same line.
     */
    function isSameLine(left, right) {
        return left.loc.start.line === right.loc.start.line;
    }

    /**
    * Reports that there shouldn't be a space after the first token
    * @param {ASTNode} node - The node to report in the event of an error.
    * @param {Token} token - The token to use for the report.
    * @returns {void}
    */
    function reportNoBeginningSpace(node, token) {
        context.report(node, token.loc.start,
            "There should be no space after '" + token.value + "'");
    }

    /**
    * Reports that there shouldn't be a space before the last token
    * @param {ASTNode} node - The node to report in the event of an error.
    * @param {Token} token - The token to use for the report.
    * @returns {void}
    */
    function reportNoEndingSpace(node, token) {
        context.report(node, token.loc.start,
            "There should be no space before '" + token.value + "'");
    }

    /**
    * Reports that there should be a space after the first token
    * @param {ASTNode} node - The node to report in the event of an error.
    * @param {Token} token - The token to use for the report.
    * @returns {void}
    */
    function reportRequiredBeginningSpace(node, token) {
        context.report(node, token.loc.start,
            "A space is required after '" + token.value + "'");
    }

    /**
    * Reports that there should be a space before the last token
    * @param {ASTNode} node - The node to report in the event of an error.
    * @param {Token} token - The token to use for the report.
    * @returns {void}
    */
    function reportRequiredEndingSpace(node, token) {
        context.report(node, token.loc.start,
                    "A space is required before '" + token.value + "'");
    }


    /**
     * Determines if spacing in curly braces is valid.
     * @param {ASTNode} node The AST node to check.
     * @param {Token} first The first token to check (should be the opening brace)
     * @param {Token} second The second token to check (should be first after the opening brace)
     * @param {Token} penultimate The penultimate token to check (should be last before closing brace)
     * @param {Token} last The last token to check (should be closing brace)
     * @returns {void}
     */
    function validateBraceSpacing(node, first, second, penultimate, last) {
        var closingCurlyBraceMustBeSpaced =
            options.arraysInObjectsException && penultimate.value === "]" ||
            options.objectsInObjectsException && penultimate.value === "}"
            ? !options.spaced : options.spaced;

        if (isSameLine(first, second)) {
            if (options.spaced && !isSpaced(first, second)) {
                reportRequiredBeginningSpace(node, first);
            }
            if (!options.spaced && isSpaced(first, second)) {
                reportNoBeginningSpace(node, first);
            }
        }

        if (isSameLine(penultimate, last)) {
            if (closingCurlyBraceMustBeSpaced && !isSpaced(penultimate, last)) {
                reportRequiredEndingSpace(node, last);
            }
            if (!closingCurlyBraceMustBeSpaced && isSpaced(penultimate, last)) {
                reportNoEndingSpace(node, last);
            }
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        MemberExpression: function(node) {
            if (!node.computed) {
                return;
            }

            var property = node.property,
                before = context.getTokenBefore(property),
                first = context.getFirstToken(property),
                last = context.getLastToken(property),
                after = context.getTokenAfter(property);

            var propertyNameMustBeSpaced = options.propertyNameException ?
                                    !options.spaced : options.spaced;

            if (isSameLine(before, first)) {
                if (propertyNameMustBeSpaced) {
                    if (!isSpaced(before, first) && isSameLine(before, first)) {
                        reportRequiredBeginningSpace(node, before);
                    }
                } else {
                    if (isSpaced(before, first)) {
                        reportNoBeginningSpace(node, before);
                    }
                }
            }

            if (isSameLine(last, after)) {
                if (propertyNameMustBeSpaced) {
                    if (!isSpaced(last, after) && isSameLine(last, after)) {
                        reportRequiredEndingSpace(node, after);
                    }
                } else {
                    if (isSpaced(last, after)) {
                        reportNoEndingSpace(node, after);
                    }
                }
            }
        },

        ArrayExpression: function(node) {
            if (node.elements.length === 0) {
                return;
            }

            var first = context.getFirstToken(node),
                second = context.getFirstToken(node, 1),
                penultimate = context.getLastToken(node, 1),
                last = context.getLastToken(node);

            var openingBracketMustBeSpaced =
                options.objectsInArraysException && second.value === "{" ||
                options.arraysInArraysException && second.value === "[" ||
                options.singleElementException && node.elements.length === 1
                ? !options.spaced : options.spaced;

            var closingBracketMustBeSpaced =
                options.objectsInArraysException && penultimate.value === "}" ||
                options.arraysInArraysException && penultimate.value === "]" ||
                options.singleElementException && node.elements.length === 1
                ? !options.spaced : options.spaced;

            if (isSameLine(first, second)) {
                if (openingBracketMustBeSpaced && !isSpaced(first, second)) {
                    reportRequiredBeginningSpace(node, first);
                }
                if (!openingBracketMustBeSpaced && isSpaced(first, second)) {
                    reportNoBeginningSpace(node, first);
                }
            }

            if (isSameLine(penultimate, last)) {
                if (closingBracketMustBeSpaced && !isSpaced(penultimate, last)) {
                    reportRequiredEndingSpace(node, last);
                }
                if (!closingBracketMustBeSpaced && isSpaced(penultimate, last)) {
                    reportNoEndingSpace(node, last);
                }
            }
        },

        ImportDeclaration: function(node) {

            var firstSpecifier = node.specifiers[0],
                lastSpecifier = node.specifiers[node.specifiers.length - 1];

            // don't do anything for namespace or default imports
            if (firstSpecifier.type === "ImportSpecifier" && lastSpecifier.type === "ImportSpecifier") {
                var first = context.getTokenBefore(firstSpecifier),
                    second = context.getFirstToken(firstSpecifier),
                    penultimate = context.getLastToken(lastSpecifier),
                    last = context.getTokenAfter(lastSpecifier);

                validateBraceSpacing(node, first, second, penultimate, last);
            }

        },

        ExportNamedDeclaration: function(node) {

            var firstSpecifier = node.specifiers[0],
                lastSpecifier = node.specifiers[node.specifiers.length - 1],
                first = context.getTokenBefore(firstSpecifier),
                second = context.getFirstToken(firstSpecifier),
                penultimate = context.getLastToken(lastSpecifier),
                last = context.getTokenAfter(lastSpecifier);

            validateBraceSpacing(node, first, second, penultimate, last);

        },

        ObjectExpression: function(node) {
            if (node.properties.length === 0) {
                return;
            }

            var first = context.getFirstToken(node),
                second = context.getFirstToken(node, 1),
                penultimate = context.getLastToken(node, 1),
                last = context.getLastToken(node);

            validateBraceSpacing(node, first, second, penultimate, last);
        }

    };

};
