/**
 * @fileoverview Restrict usage of specified globals.
 * @author Benoît Zugmeyer
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow specified global variables",
            category: "Variables",
            recommended: false
        },

        schema: {
            type: "array",
            items: {
                type: "string"
            },
            uniqueItems: true
        }
    },

    create: function(context) {
        let restrictedGlobals = context.options;

        // if no globals are restricted we don't need to check
        if (restrictedGlobals.length === 0) {
            return {};
        }

        /**
         * Report a variable to be used as a restricted global.
         * @param {Reference} reference the variable reference
         * @returns {void}
         * @private
         */
        function reportReference(reference) {
            context.report(reference.identifier, "Unexpected use of '{{name}}'.", {
                name: reference.identifier.name
            });
        }

        /**
         * Check if the given name is a restricted global name.
         * @param {string} name name of a variable
         * @returns {boolean} whether the variable is a restricted global or not
         * @private
         */
        function isRestricted(name) {
            return restrictedGlobals.indexOf(name) >= 0;
        }

        return {
            Program: function() {
                let scope = context.getScope();

                // Report variables declared elsewhere (ex: variables defined as "global" by eslint)
                scope.variables.forEach(function(variable) {
                    if (!variable.defs.length && isRestricted(variable.name)) {
                        variable.references.forEach(reportReference);
                    }
                });

                // Report variables not declared at all
                scope.through.forEach(function(reference) {
                    if (isRestricted(reference.identifier.name)) {
                        reportReference(reference);
                    }
                });

            }
        };
    }
};
