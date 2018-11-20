/**
 * @fileoverview A rule to ensure the use of a single variable declaration.
 * @author Ian Christian Myers
 * @copyright 2015 Joey Baker. All rights reserved.
 * @copyright 2015 Danny Fritz. All rights reserved.
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MODE = context.options[0] || "always";
    var options = {};

    // simple options configuration with just a string or no option
    if (typeof context.options[0] === "string" || context.options[0] == null) {
        options.var = MODE;
        options.let = MODE;
        options.const = MODE;
    } else {
        options = context.options[0];
    }

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    var functionStack = [];
    var blockStack = [];

    /**
     * Increments the blockStack counter.
     * @returns {void}
     * @private
     */
    function startBlock() {
        blockStack.push({let: false, const: false});
    }

    /**
     * Increments the functionStack counter.
     * @returns {void}
     * @private
     */
    function startFunction() {
        functionStack.push(false);
        startBlock();
    }

    /**
     * Decrements the blockStack counter.
     * @returns {void}
     * @private
     */
    function endBlock() {
        blockStack.pop();
    }

    /**
     * Decrements the functionStack counter.
     * @returns {void}
     * @private
     */
    function endFunction() {
        functionStack.pop();
        endBlock();
    }

    /**
     * Determines if there is more than one var statement in the current scope.
     * @returns {boolean} Returns true if it is the first var declaration, false if not.
     * @private
     */
    function hasOnlyOneVar() {
        if (functionStack[functionStack.length - 1]) {
            return true;
        } else {
            functionStack[functionStack.length - 1] = true;
            return false;
        }
    }

    /**
     * Determines if there is more than one let statement in the current scope.
     * @returns {boolean} Returns true if it is the first let declaration, false if not.
     * @private
     */
    function hasOnlyOneLet() {
        if (blockStack[blockStack.length - 1].let) {
            return true;
        } else {
            blockStack[blockStack.length - 1].let = true;
            return false;
        }
    }

    /**
     * Determines if there is more than one const statement in the current scope.
     * @returns {boolean} Returns true if it is the first const declaration, false if not.
     * @private
     */
    function hasOnlyOneConst() {
        if (blockStack[blockStack.length - 1].const) {
            return true;
        } else {
            blockStack[blockStack.length - 1].const = true;
            return false;
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "Program": startFunction,
        "FunctionDeclaration": startFunction,
        "FunctionExpression": startFunction,
        "ArrowFunctionExpression": startFunction,
        "BlockStatement": startBlock,
        "ForStatement": startBlock,
        "SwitchStatement": startBlock,

        "VariableDeclaration": function(node) {
            var declarationCount = node.declarations.length;

            if (node.kind === "var") {
                if (options.var === "never") {
                    if (declarationCount > 1) {
                        context.report(node, "Split 'var' declaration into multiple statements.");
                    }
                } else {
                    if (hasOnlyOneVar()) {
                        context.report(node, "Combine this with the previous 'var' statement.");
                    }
                }
            } else if (node.kind === "let") {
                if (options.let === "never") {
                    if (declarationCount > 1) {
                        context.report(node, "Split 'let' declaration into multiple statements.");
                    }
                } else {
                    if (hasOnlyOneLet()) {
                        context.report(node, "Combine this with the previous 'let' statement.");
                    }
                }
            } else if (node.kind === "const") {
                if (options.const === "never") {
                    if (declarationCount > 1) {
                        context.report(node, "Split 'const' declaration into multiple statements.");
                    }
                } else {
                    if (hasOnlyOneConst()) {
                        context.report(node, "Combine this with the previous 'const' statement.");
                    }
                }

            }
        },

        "ForStatement:exit": endBlock,
        "SwitchStatement:exit": endBlock,
        "BlockStatement:exit": endBlock,
        "Program:exit": endFunction,
        "FunctionDeclaration:exit": endFunction,
        "FunctionExpression:exit": endFunction,
        "ArrowFunctionExpression:exit": endFunction
    };

};
