/**
 * @fileoverview Rule to flag duplicate arguments
 * @author Jamund Ferguson
 * @copyright 2015 Jamund Ferguson. All rights reserved.
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Checks whether or not a given definition is a parameter's.
     * @param {escope.DefEntry} def - A definition to check.
     * @returns {boolean} `true` if the definition is a parameter's.
     */
    function isParameter(def) {
        return def.type === "Parameter";
    }

    /**
     * Determines if a given node has duplicate parameters.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkParams(node) {
        var variables = context.getDeclaredVariables(node);
        var keyMap = Object.create(null);

        for (var i = 0; i < variables.length; ++i) {
            var variable = variables[i];

            // TODO(nagashima): Remove this duplication check after https://github.com/estools/escope/pull/79
            var key = "$" + variable.name; // to avoid __proto__.

            if (!isParameter(variable.defs[0]) || keyMap[key]) {
                continue;
            }
            keyMap[key] = true;

            // Checks and reports duplications.
            var defs = variable.defs.filter(isParameter);

            if (defs.length >= 2) {
                context.report({
                    node: node,
                    message: "Duplicate param '{{name}}'.",
                    data: {name: variable.name}
                });
            }
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "FunctionDeclaration": checkParams,
        "FunctionExpression": checkParams
    };

};

module.exports.schema = [];
