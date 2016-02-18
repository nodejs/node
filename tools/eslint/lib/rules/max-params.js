/**
 * @fileoverview Rule to flag when a function has too many parameters
 * @author Ilya Volodin
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var numParams = context.options[0] || 3;

    /**
     * Checks a function to see if it has too many parameters.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkFunction(node) {
        if (node.params.length > numParams) {
            context.report(node, "This function has too many parameters ({{count}}). Maximum allowed is {{max}}.", {
                count: node.params.length,
                max: numParams
            });
        }
    }

    return {
        "FunctionDeclaration": checkFunction,
        "ArrowFunctionExpression": checkFunction,
        "FunctionExpression": checkFunction
    };

};

module.exports.schema = [
    {
        "type": "integer",
        "minimum": 0
    }
];
