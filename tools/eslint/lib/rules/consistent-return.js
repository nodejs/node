/**
 * @fileoverview Rule to flag consistent return values
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var functions = [];

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Marks entrance into a function by pushing a new object onto the functions
     * stack.
     * @returns {void}
     * @private
     */
    function enterFunction() {
        functions.push({});
    }

    /**
     * Marks exit of a function by popping off the functions stack.
     * @returns {void}
     * @private
     */
    function exitFunction() {
        functions.pop();
    }


    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "Program": enterFunction,
        "FunctionDeclaration": enterFunction,
        "FunctionExpression": enterFunction,
        "ArrowFunctionExpression": enterFunction,

        "Program:exit": exitFunction,
        "FunctionDeclaration:exit": exitFunction,
        "FunctionExpression:exit": exitFunction,
        "ArrowFunctionExpression:exit": exitFunction,

        "ReturnStatement": function(node) {

            var returnInfo = functions[functions.length - 1],
                returnTypeDefined = "type" in returnInfo;

            if (returnTypeDefined) {

                if (returnInfo.type !== !!node.argument) {
                    context.report(node, "Expected " + (returnInfo.type ? "a" : "no") + " return value.");
                }

            } else {
                returnInfo.type = !!node.argument;
            }

        }
    };

};

module.exports.schema = [];
