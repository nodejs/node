/**
 * @fileoverview Rule to specify spacing of object literal keys and values
 * @author Brandon Mills
 * @copyright 2014 Brandon Mills. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether a string contains a line terminator as defined in
 * http://www.ecma-international.org/ecma-262/5.1/#sec-7.3
 * @param {string} str String to test.
 * @returns {boolean} True if str contains a line terminator.
 */
function containsLineTerminator(str) {
    return /[\n\r\u2028\u2029]/.test(str);
}

/**
 * Gets the last element of an array.
 * @param {Array} arr An array.
 * @returns {any} Last element of arr.
 */
function last(arr) {
    return arr[arr.length - 1];
}

/**
 * Checks whether a property is a member of the property group it follows.
 * @param {ASTNode} lastMember The last Property known to be in the group.
 * @param {ASTNode} candidate The next Property that might be in the group.
 * @returns {boolean} True if the candidate property is part of the group.
 */
function continuesPropertyGroup(lastMember, candidate) {
    var groupEndLine = lastMember.loc.end.line,
        candidateStartLine = candidate.loc.start.line,
        comments, i;

    if (candidateStartLine - groupEndLine <= 1) {
        return true;
    }

    // Check that the first comment is adjacent to the end of the group, the
    // last comment is adjacent to the candidate property, and that successive
    // comments are adjacent to each other.
    comments = candidate.leadingComments;
    if (
        comments &&
        comments[0].loc.start.line - groupEndLine <= 1 &&
        candidateStartLine - last(comments).loc.end.line <= 1
    ) {
        for (i = 1; i < comments.length; i++) {
            if (comments[i].loc.start.line - comments[i - 1].loc.end.line > 1) {
                return false;
            }
        }
        return true;
    }

    return false;
}

/**
 * Checks whether a node is contained on a single line.
 * @param {ASTNode} node AST Node being evaluated.
 * @returns {boolean} True if the node is a single line.
 */
function isSingleLine(node) {
    return (node.loc.end.line === node.loc.start.line);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var messages = {
    key: "{{error}} space after {{computed}}key \"{{key}}\".",
    value: "{{error}} space before value for {{computed}}key \"{{key}}\"."
};

module.exports = function(context) {

    /**
     * OPTIONS
     * "key-spacing": [2, {
     *     beforeColon: false,
     *     afterColon: true,
     *     align: "colon" // Optional, or "value"
     * }
     */

    var options = context.options[0] || {},
        align = options.align,
        beforeColon = +!!options.beforeColon, // Defaults to false
        afterColon = +!(options.afterColon === false); // Defaults to true

    /**
     * Gets an object literal property's key as the identifier name or string value.
     * @param {ASTNode} property Property node whose key to retrieve.
     * @returns {string} The property's key.
     */
    function getKey(property) {
        var key = property.key;

        if (property.computed) {
            return context.getSource().slice(key.range[0], key.range[1]);
        }

        return property.key.name || property.key.value;
    }

    /**
     * Reports an appropriately-formatted error if spacing is incorrect on one
     * side of the colon.
     * @param {ASTNode} property Key-value pair in an object literal.
     * @param {string} side Side being verified - either "key" or "value".
     * @param {string} whitespace Actual whitespace string.
     * @param {int} expected Expected whitespace length.
     * @returns {void}
     */
    function report(property, side, whitespace, expected) {
        var diff = whitespace.length - expected,
            key = property.key,
            firstTokenAfterColon = context.getTokenAfter(key, 1),
            location = side === "key" ? key.loc.start : firstTokenAfterColon.loc.start;

        if (diff && !(expected && containsLineTerminator(whitespace))) {
            context.report(property[side], location, messages[side], {
                error: diff > 0 ? "Extra" : "Missing",
                computed: property.computed ? "computed " : "",
                key: getKey(property)
            });
        }
    }

    /**
     * Gets the number of characters in a key, including quotes around string
     * keys and braces around computed property keys.
     * @param {ASTNode} property Property of on object literal.
     * @returns {int} Width of the key.
     */
    function getKeyWidth(property) {
        var key = property.key,
            startToken, endToken;

        // [computed]: value
        if (property.computed) {
            startToken = context.getTokenBefore(key);
            endToken = context.getTokenAfter(key);
            return endToken.range[1] - startToken.range[0];
        }

        // name: value
        if (key.type === "Identifier") {
            return key.name.length;
        }

        // "literal": value
        // 42: value
        if (key.type === "Literal") {
            return key.raw.length;
        }
    }

    /**
     * Gets the whitespace around the colon in an object literal property.
     * @param {ASTNode} property Property node from an object literal.
     * @returns {Object} Whitespace before and after the property's colon.
     */
    function getPropertyWhitespace(property) {
        var whitespace = /(\s*):(\s*)/.exec(context.getSource().slice(
            property.key.range[1], property.value.range[0]
        ));

        if (whitespace) {
            return {
                beforeColon: whitespace[1],
                afterColon: whitespace[2]
            };
        }
    }

    /**
     * Creates groups of properties.
     * @param  {ASTNode} node ObjectExpression node being evaluated.
     * @returns {Array.<ASTNode[]>} Groups of property AST node lists.
     */
    function createGroups(node) {
        if (node.properties.length === 1) {
            return [node.properties];
        }

        return node.properties.reduce(function(groups, property) {
            var currentGroup = last(groups),
                prev = last(currentGroup);

            if (!prev || continuesPropertyGroup(prev, property)) {
                currentGroup.push(property);
            } else {
                groups.push([property]);
            }

            return groups;
        }, [[]]);
    }

    /**
     * Verifies correct vertical alignment of a group of properties.
     * @param {ASTNode[]} properties List of Property AST nodes.
     * @returns {void}
     */
    function verifyGroupAlignment(properties) {
        var length = properties.length,
            widths = properties.map(getKeyWidth), // Width of keys, including quotes
            targetWidth = Math.max.apply(null, widths),
            i, property, whitespace, width;

        // Conditionally include one space before or after colon
        targetWidth += (align === "colon" ? beforeColon : afterColon);

        for (i = 0; i < length; i++) {
            property = properties[i];
            whitespace = getPropertyWhitespace(property);

            if (!whitespace) {
                continue; // Object literal getters/setters lack a colon
            }

            width = widths[i];

            if (align === "value") {
                report(property, "key", whitespace.beforeColon, beforeColon);
                report(property, "value", whitespace.afterColon, targetWidth - width);
            } else { // align = "colon"
                report(property, "key", whitespace.beforeColon, targetWidth - width);
                report(property, "value", whitespace.afterColon, afterColon);
            }
        }
    }

    /**
     * Verifies vertical alignment, taking into account groups of properties.
     * @param  {ASTNode} node ObjectExpression node being evaluated.
     * @returns {void}
     */
    function verifyAlignment(node) {
        createGroups(node).forEach(function(group) {
            verifyGroupAlignment(group);
        });
    }

    /**
     * Verifies spacing of property conforms to specified options.
     * @param  {ASTNode} node Property node being evaluated.
     * @returns {void}
     */
    function verifySpacing(node) {
        var whitespace = getPropertyWhitespace(node);
        if (whitespace) { // Object literal getters/setters lack colons
            report(node, "key", whitespace.beforeColon, beforeColon);
            report(node, "value", whitespace.afterColon, afterColon);
        }
    }

    /**
     * Verifies spacing of each property in a list.
     * @param  {ASTNode[]} properties List of Property AST nodes.
     * @returns {void}
     */
    function verifyListSpacing(properties) {
        var length = properties.length;

        for (var i = 0; i < length; i++) {
            verifySpacing(properties[i]);
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    if (align) { // Verify vertical alignment

        return {
            "ObjectExpression": function(node) {
                if (isSingleLine(node)) {
                    verifyListSpacing(node.properties);
                } else {
                    verifyAlignment(node);
                }
            }
        };

    } else { // Strictly obey beforeColon and afterColon in each property

        return {
            "Property": function (node) {
                verifySpacing(node);
            }
        };

    }

};
