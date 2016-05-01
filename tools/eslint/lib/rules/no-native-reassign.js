/**
 * @fileoverview Rule to flag when re-assigning native objects
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow reassigning native objects",
            category: "Best Practices",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    exceptions: {
                        type: "array",
                        items: {type: "string"},
                        uniqueItems: true
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        var config = context.options[0];
        var exceptions = (config && config.exceptions) || [];

        /**
         * Reports write references.
         * @param {Reference} reference - A reference to check.
         * @param {int} index - The index of the reference in the references.
         * @param {Reference[]} references - The array that the reference belongs to.
         * @returns {void}
         */
        function checkReference(reference, index, references) {
            var identifier = reference.identifier;

            if (reference.init === false &&
                reference.isWrite() &&

                // Destructuring assignments can have multiple default value,
                // so possibly there are multiple writeable references for the same identifier.
                (index === 0 || references[index - 1].identifier !== identifier)
            ) {
                context.report({
                    node: identifier,
                    message: "{{name}} is a read-only native object.",
                    data: identifier
                });
            }
        }

        /**
         * Reports write references if a given variable is readonly builtin.
         * @param {Variable} variable - A variable to check.
         * @returns {void}
         */
        function checkVariable(variable) {
            if (variable.writeable === false && exceptions.indexOf(variable.name) === -1) {
                variable.references.forEach(checkReference);
            }
        }

        return {
            Program: function() {
                var globalScope = context.getScope();

                globalScope.variables.forEach(checkVariable);
            }
        };
    }
};
