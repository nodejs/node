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
    var aliases = [];

    if (context.options.length === 0) {
        aliases.push("that");
    } else {
        aliases = context.options;
    }

    /**
     * Reports that a variable declarator or assignment expression is assigning
     * a non-'this' value to the specified alias.
     * @param {ASTNode} node - The assigning node.
     * @param {string} alias - the name of the alias that was incorrectly used.
     * @returns {void}
     */
    function reportBadAssignment(node, alias) {
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

        if (aliases.indexOf(name) !== -1) {
            if (!isThis || node.operator && node.operator !== "=") {
                reportBadAssignment(node, name);
            }
        } else if (isThis) {
            context.report(node,
                "Unexpected alias '{{name}}' for 'this'.", { name: name });
        }
    }

    /**
     * Ensures that a variable declaration of the alias in a program or function
     * is assigned to the correct value.
     * @param {string} alias alias the check the assignment of.
     * @param {object} scope scope of the current code we are checking.
     * @private
     * @returns {void}
     */
    function checkWasAssigned(alias, scope) {
        var variable = scope.set.get(alias);

        if (!variable) {
            return;
        }

        if (variable.defs.some(function(def) {
            return def.node.type === "VariableDeclarator" &&
            def.node.init !== null;
        })) {
            return;
        }

        // The alias has been declared and not assigned: check it was
        // assigned later in the same scope.
        if (!variable.references.some(function(reference) {
            var write = reference.writeExpr;

            return (
                reference.from === scope &&
                write && write.type === "ThisExpression" &&
                write.parent.operator === "="
            );
        })) {
            variable.defs.map(function(def) {
                return def.node;
            }).forEach(function(node) {
                reportBadAssignment(node, alias);
            });
        }
    }

    /**
     * Check each alias to ensure that is was assinged to the correct value.
     * @returns {void}
     */
    function ensureWasAssigned() {
        var scope = context.getScope();

        aliases.forEach(function(alias) {
            checkWasAssigned(alias, scope);
        });
    }

    return {
        "Program:exit": ensureWasAssigned,
        "FunctionExpression:exit": ensureWasAssigned,
        "FunctionDeclaration:exit": ensureWasAssigned,

        "VariableDeclarator": function(node) {
            var id = node.id;
            var isDestructuring =
                id.type === "ArrayPattern" || id.type === "ObjectPattern";

            if (node.init !== null && !isDestructuring) {
                checkAssignment(node, id.name, node.init);
            }
        },

        "AssignmentExpression": function(node) {
            if (node.left.type === "Identifier") {
                checkAssignment(node, node.left.name, node.right);
            }
        }
    };

};

module.exports.schema = {
    "type": "array",
    "items": {
        "type": "string",
        "minLength": 1
    },
    "uniqueItems": true
};
