/**
 * @fileoverview Rule to enforce consistent naming of "this" context variables
 * @author Raphael Pigulla
 * @copyright 2015 Timothy Jones. All rights reserved.
 * @copyright 2015 David Aurelio. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var alias = context.options[0];

    /**
     * Reports that a variable declarator or assignment expression is assigning
     * a non-'this' value to the specified alias.
     * @param {ASTNode} node - The assigning node.
     * @returns {void}
     */
    function reportBadAssignment(node) {
        context.report(node,
            "Designated alias '{{alias}}' is not assigned to 'this'.",
            { alias: alias });
    }

    /**
     * Checks that an assignment to an identifier only assigns 'this' to the
     * appropriate alias, and the alias is only assigned to 'this'.
     * @param {ASTNode} node - The assigning node.
     * @param {Identifier} name - The name of the variable assigned to.
     * @param {Expression} value - The value of the assignment.
     * @returns {void}
     */
    function checkAssignment(node, name, value) {
        var isThis = value.type === "ThisExpression";

        if (name === alias) {
            if (!isThis || node.operator && node.operator !== "=") {
                reportBadAssignment(node);
            }
        } else if (isThis) {
            context.report(node,
                "Unexpected alias '{{name}}' for 'this'.", { name: name });
        }
    }

    /**
     * Ensures that a variable declaration of the alias in a program or function
     * is assigned to the correct value.
     * @returns {void}
     */
    function ensureWasAssigned() {
        var scope = context.getScope();

        scope.variables.some(function (variable) {
            var lookup;

            if (variable.name === alias) {
                if (variable.defs.some(function (def) {
                    return def.node.type === "VariableDeclarator" &&
                        def.node.init !== null;
                })) {
                    return true;
                }

                lookup = scope.type === "global" ? scope : variable;

                // The alias has been declared and not assigned: check it was
                // assigned later in the same scope.
                if (!lookup.references.some(function (reference) {
                    var write = reference.writeExpr;

                    if (reference.from === scope &&
                            write && write.type === "ThisExpression" &&
                            write.parent.operator === "=") {
                        return true;
                    }
                })) {
                    variable.defs.map(function (def) {
                        return def.node;
                    }).forEach(reportBadAssignment);
                }

                return true;
            }
        });
    }

    return {
        "Program:exit": ensureWasAssigned,
        "FunctionExpression:exit": ensureWasAssigned,
        "FunctionDeclaration:exit": ensureWasAssigned,

        "VariableDeclarator": function (node) {
            var id = node.id;
            var isDestructuring =
                id.type === "ArrayPattern" || id.type === "ObjectPattern";

            if (node.init !== null && !isDestructuring) {
                checkAssignment(node, id.name, node.init);
            }
        },

        "AssignmentExpression": function (node) {
            if (node.left.type === "Identifier") {
                checkAssignment(node, node.left.name, node.right);
            }
        }
    };

};

module.exports.schema = [
    {
        "type": "string"
    }
];
