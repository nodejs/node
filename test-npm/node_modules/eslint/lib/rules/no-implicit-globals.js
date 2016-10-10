/**
 * @fileoverview Rule to check for implicit global variables and functions.
 * @author Joshua Peek
 * @copyright 2015 Joshua Peek. All rights reserved.
 * See LICENSE file in root directory for full license.
*/

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    return {
        "Program": function() {
            var scope = context.getScope();

            scope.variables.forEach(function(variable) {
                if (variable.writeable) {
                    return;
                }

                variable.defs.forEach(function(def) {
                    if (def.type === "FunctionName" || (def.type === "Variable" && def.parent.kind === "var")) {
                        context.report(def.node, "Implicit global variable, assign as global property instead.");
                    }
                });
            });

            scope.implicit.variables.forEach(function(variable) {
                var scopeVariable = scope.set.get(variable.name);
                if (scopeVariable && scopeVariable.writeable) {
                    return;
                }

                variable.defs.forEach(function(def) {
                    context.report(def.node, "Implicit global variable, assign as global property instead.");
                });
            });
        }
    };

};

module.exports.schema = [];
