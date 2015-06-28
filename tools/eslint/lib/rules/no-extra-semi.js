/**
 * @fileoverview Rule to flag use of unnecessary semicolons
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Reports an unnecessary semicolon error.
     * @param {Node|Token} nodeOrToken - A node or a token to be reported.
     * @returns {void}
     */
    function report(nodeOrToken) {
        context.report(nodeOrToken, "Unnecessary semicolon.");
    }

    /**
     * Checks for a part of a class body.
     * This checks tokens from a specified token to a next MethodDefinition or the end of class body.
     *
     * @param {Token} firstToken - The first token to check.
     * @returns {void}
     */
    function checkForPartOfClassBody(firstToken) {
        for (var token = firstToken;
            token.type === "Punctuator" && token.value !== "}";
            token = context.getTokenAfter(token)
        ) {
            if (token.value === ";") {
                report(token);
            }
        }
    }

    return {
        /**
         * Reports this empty statement.
         * @param {Node} node - A EmptyStatement node to be reported.
         * @returns {void}
         */
        "EmptyStatement": report,

        /**
         * Checks tokens from the head of this class body to the first MethodDefinition or the end of this class body.
         * @param {Node} node - A ClassBody node to check.
         * @returns {void}
         */
        "ClassBody": function(node) {
            checkForPartOfClassBody(context.getFirstToken(node, 1)); // 0 is `{`.
        },

        /**
         * Checks tokens from this MethodDefinition to the next MethodDefinition or the end of this class body.
         * @param {Node} node - A MethodDefinition node of the start point.
         * @returns {void}
         */
        "MethodDefinition": function(node) {
            checkForPartOfClassBody(context.getTokenAfter(node));
        }
    };

};

module.exports.schema = [];
