/**
 * @fileoverview Disallow reassignment of function parameters.
 * @author Nat Burns
 * @copyright 2014 Nat Burns. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var stopNodePattern = /(?:Statement|Declaration|Function(?:Expression)?|Program)$/;

module.exports = function(context) {
    var props = context.options[0] && Boolean(context.options[0].props);

    /**
     * Checks whether or not the reference modifies properties of its variable.
     * @param {Reference} reference - A reference to check.
     * @returns {boolean} Whether or not the reference modifies properties of its variable.
     */
    function isModifyingProp(reference) {
        var node = reference.identifier;
        var parent = node.parent;

        while (parent && !stopNodePattern.test(parent.type)) {
            switch (parent.type) {

                // e.g. foo.a = 0;
                case "AssignmentExpression":
                    return parent.left === node;

                // e.g. ++foo.a;
                case "UpdateExpression":
                    return true;

                // e.g. delete foo.a;
                case "UnaryExpression":
                    if (parent.operator === "delete") {
                        return true;
                    }
                    break;

                // EXCLUDES: e.g. cache.get(foo.a).b = 0;
                case "CallExpression":
                    if (parent.callee !== node) {
                        return false;
                    }
                    break;

                // EXCLUDES: e.g. cache[foo.a] = 0;
                case "MemberExpression":
                    if (parent.property === node) {
                        return false;
                    }
                    break;

                default:
                    break;
            }

            node = parent;
            parent = node.parent;
        }

        return false;
    }

    /**
     * Reports a reference if is non initializer and writable.
     * @param {Reference} reference - A reference to check.
     * @param {int} index - The index of the reference in the references.
     * @param {Reference[]} references - The array that the reference belongs to.
     * @returns {void}
     */
    function checkReference(reference, index, references) {
        var identifier = reference.identifier;

        if (identifier &&
            !reference.init &&

            // Destructuring assignments can have multiple default value,
            // so possibly there are multiple writeable references for the same identifier.
            (index === 0 || references[index - 1].identifier !== identifier)
        ) {
            if (reference.isWrite()) {
                context.report(
                    identifier,
                    "Assignment to function parameter '{{name}}'.",
                    {name: identifier.name});
            } else if (props && isModifyingProp(reference)) {
                context.report(
                    identifier,
                    "Assignment to property of function parameter '{{name}}'.",
                    {name: identifier.name});
            }
        }
    }

    /**
     * Finds and reports references that are non initializer and writable.
     * @param {Variable} variable - A variable to check.
     * @returns {void}
     */
    function checkVariable(variable) {
        if (variable.defs[0].type === "Parameter") {
            variable.references.forEach(checkReference);
        }
    }

    /**
     * Checks parameters of a given function node.
     * @param {ASTNode} node - A function node to check.
     * @returns {void}
     */
    function checkForFunction(node) {
        context.getDeclaredVariables(node).forEach(checkVariable);
    }

    return {

        // `:exit` is needed for the `node.parent` property of identifier nodes.
        "FunctionDeclaration:exit": checkForFunction,
        "FunctionExpression:exit": checkForFunction,
        "ArrowFunctionExpression:exit": checkForFunction
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "props": {"type": "boolean"}
        },
        "additionalProperties": false
    }
];
