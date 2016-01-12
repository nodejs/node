/**
 * @fileoverview Common utils for AST.
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var esutils = require("esutils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks reference if is non initializer and writable.
 * @param {Reference} reference - A reference to check.
 * @param {int} index - The index of the reference in the references.
 * @param {Reference[]} references - The array that the reference belongs to.
 * @returns {boolean} Success/Failure
 * @private
 */
function isModifyingReference(reference, index, references) {
    var identifier = reference.identifier;

    return (identifier &&
        reference.init === false &&
        reference.isWrite() &&
            // Destructuring assignments can have multiple default value,
            // so possibly there are multiple writeable references for the same identifier.
        (index === 0 || references[index - 1].identifier !== identifier)
    );
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    /**
     * Determines whether two adjacent tokens are on the same line.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not the tokens are on the same line.
     * @public
     */
    isTokenOnSameLine: function(left, right) {
        return left.loc.end.line === right.loc.start.line;
    },

    /**
     * Checks whether or not a node is `null` or `undefined`.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} Whether or not the node is a `null` or `undefined`.
     * @public
     */
    isNullOrUndefined: function(node) {
        return (
            (node.type === "Literal" && node.value === null) ||
            (node.type === "Identifier" && node.name === "undefined") ||
            (node.type === "UnaryExpression" && node.operator === "void")
        );
    },

    /**
     * Checks whether or not a given node is a string literal.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} `true` if the node is a string literal.
     */
    isStringLiteral: function(node) {
        return (
            (node.type === "Literal" && typeof node.value === "string") ||
            node.type === "TemplateLiteral"
        );
    },

    /**
     * Gets references which are non initializer and writable.
     * @param {Reference[]} references - An array of references.
     * @returns {Reference[]} An array of only references which are non initializer and writable.
     * @public
     */
    getModifyingReferences: function(references) {
        return references.filter(isModifyingReference);
    },

    /**
     * Validate that a string passed in is surrounded by the specified character
     * @param  {string} val The text to check.
     * @param  {string} character The character to see if it's surrounded by.
     * @returns {boolean} True if the text is surrounded by the character, false if not.
     * @private
     */
    isSurroundedBy: function(val, character) {
        return val[0] === character && val[val.length - 1] === character;
    },

    /**
     * Returns whether the provided node is an ESLint directive comment or not
     * @param {LineComment|BlockComment} node The node to be checked
     * @returns {boolean} `true` if the node is an ESLint directive comment
     */
    isDirectiveComment: function(node) {
        var comment = node.value.trim();
        return (
            node.type === "Line" && comment.indexOf("eslint-") === 0 ||
            node.type === "Block" && (
                comment.indexOf("global ") === 0 ||
                comment.indexOf("eslint ") === 0 ||
                comment.indexOf("eslint-") === 0
            )
        );
    },

    /**
     * Gets the trailing statement of a given node.
     *
     *     if (code)
     *         consequent;
     *
     * When taking this `IfStatement`, returns `consequent;` statement.
     *
     * @param {ASTNode} A node to get.
     * @returns {ASTNode|null} The trailing statement's node.
     */
    getTrailingStatement: esutils.ast.trailingStatement,

    /**
     * Finds the variable by a given name in a given scope and its upper scopes.
     *
     * @param {escope.Scope} initScope - A scope to start find.
     * @param {string} name - A variable name to find.
     * @returns {escope.Variable|null} A found variable or `null`.
     */
    getVariableByName: function(initScope, name) {
        var scope = initScope;
        while (scope) {
            var variable = scope.set.get(name);
            if (variable) {
                return variable;
            }

            scope = scope.upper;
        }

        return null;
    }
};
