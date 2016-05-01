/**
 * @fileoverview Rule to check for implicit global variables and functions.
 * @author Joshua Peek
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `var` and named `function` declarations in the global scope",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        return {
            Program: function() {
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

    }
};
