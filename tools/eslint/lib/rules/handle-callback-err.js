/**
 * @fileoverview Ensure handling of errors when we know they exist.
 * @author Jamund Ferguson
 * @copyright 2014 Jamund Ferguson. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var errorArgument = context.options[0] || "err";
    var callbacks = [];
    var scopes = 0;

    /**
     * Checks if the given argument should be interpreted as a regexp pattern.
     * @param {string} stringToCheck The string which should be checked.
     * @returns {boolean} Whether or not the string should be interpreted as a pattern.
     */
    function isPattern(stringToCheck) {
        var firstChar = stringToCheck[0];
        return firstChar === "^";
    }

    /**
     * Checks if the given name matches the configured error argument.
     * @param {string} name The name which should be compared.
     * @returns {boolean} Whether or not the given name matches the configured error variable name.
     */
    function matchesConfiguredErrorName(name) {
        if (isPattern(errorArgument)) {
            var regexp = new RegExp(errorArgument);
            return regexp.test(name);
        }
        return name === errorArgument;
    }

    /**
     * Check the arguments to see if we need to start tracking the error object.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     */
    function startFunction(node) {

        // keep track of nested scopes
        scopes++;

        // check if the first argument matches our argument name
        var firstArg = node.params && node.params[0];
        if (firstArg && matchesConfiguredErrorName(firstArg.name)) {
            callbacks.push({handled: false, depth: scopes, errorVariableName: firstArg.name});
        }
    }

    /**
     * At the end of a function check to see if the error was handled.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     */
    function endFunction(node) {

        var callback = callbacks[callbacks.length - 1] || {};

        // check if a callback is ending, if so pop it off the stack
        if (callback.depth === scopes) {
            callbacks.pop();

            // check if there were no handled errors since the last callback
            if (!callback.handled) {
                context.report(node, "Expected error to be handled.");
            }
        }

        // less nested functions
        scopes--;

    }

    /**
     * Check to see if we're handling the error object properly.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     */
    function checkForError(node) {
        if (callbacks.length > 0) {
            var callback = callbacks[callbacks.length - 1] || {};

            // make sure the node's name matches our error argument name
            var isAboutError = node.name === callback.errorVariableName;

            // we don't consider these use cases as "handling" the error
            var doNotCount = ["FunctionDeclaration", "ArrowFunctionExpression", "FunctionExpression", "CatchClause"];

            // make sure this identifier isn't used as part of one of them
            var isHandled = doNotCount.indexOf(node.parent.type) === -1;

            if (isAboutError && isHandled) {
                // record that this callback handled its error
                callback.handled = true;
            }
        }
    }

    return {
        "FunctionDeclaration": startFunction,
        "FunctionExpression": startFunction,
        "ArrowFunctionExpression": startFunction,
        "Identifier": checkForError,
        "FunctionDeclaration:exit": endFunction,
        "FunctionExpression:exit": endFunction,
        "ArrowFunctionExpression:exit": endFunction
    };

};
