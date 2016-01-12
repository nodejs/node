/**
 * @fileoverview Rule to flag when re-assigning native objects
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var config = context.options[0];
    var exceptions = (config && config.exceptions) || [];

    /**
     * Gets the names of writeable built-in variables.
     * @param {escope.Scope} scope - A scope to get.
     * @returns {object} A map that its key is variable names.
     */
    function getBuiltinGlobals(scope) {
        return scope.variables.reduce(function(retv, variable) {
            if (variable.writeable === false && variable.name !== "__proto__") {
                retv[variable.name] = true;
            }
            return retv;
        }, Object.create(null));
    }

    /**
     * Reports if a given reference's name is same as native object's.
     * @param {object} builtins - A map that its key is a variable name.
     * @param {Reference} reference - A reference to check.
     * @param {int} index - The index of the reference in the references.
     * @param {Reference[]} references - The array that the reference belongs to.
     * @returns {void}
     */
    function checkThroughReference(builtins, reference, index, references) {
        var identifier = reference.identifier;

        if (identifier &&
            builtins[identifier.name] &&
            exceptions.indexOf(identifier.name) === -1 &&
            reference.init === false &&
            reference.isWrite() &&
            // Destructuring assignments can have multiple default value,
            // so possibly there are multiple writeable references for the same identifier.
            (index === 0 || references[index - 1].identifier !== identifier)
        ) {
            context.report(
                identifier,
                "{{name}} is a read-only native object.",
                {name: identifier.name});
        }
    }

    return {
        // Checks assignments of global variables.
        // References to implicit global variables are not resolved,
        // so those are in the `through` of the global scope.
        "Program": function() {
            var globalScope = context.getScope();
            var builtins = getBuiltinGlobals(globalScope);
            globalScope.through.forEach(checkThroughReference.bind(null, builtins));
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "exceptions": {
                "type": "array",
                "items": {"type": "string"},
                "uniqueItems": true
            }
        },
        "additionalProperties": false
    }
];
