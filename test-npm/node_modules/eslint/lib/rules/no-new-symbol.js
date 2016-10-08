/**
 * @fileoverview Rule to disallow use of the new operator with the `Symbol` object
 * @author Alberto Rodríguez
 * @copyright 2016 Alberto Rodríguez. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "Program:exit": function() {
            var globalScope = context.getScope();
            var variable = globalScope.set.get("Symbol");
            if (variable && variable.defs.length === 0) {
                variable.references.forEach(function(ref) {
                    var node = ref.identifier;
                    if (node.parent && node.parent.type === "NewExpression") {
                        context.report(node, "`Symbol` cannot be called as a constructor.");
                    }
                });
            }
        }
    };

};

module.exports.schema = [];
