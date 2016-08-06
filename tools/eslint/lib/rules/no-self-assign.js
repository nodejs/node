/**
 * @fileoverview Rule to disallow assignments where both sides are exactly the same
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Traverses 2 Pattern nodes in parallel, then reports self-assignments.
 *
 * @param {ASTNode|null} left - A left node to traverse. This is a Pattern or
 *      a Property.
 * @param {ASTNode|null} right - A right node to traverse. This is a Pattern or
 *      a Property.
 * @param {Function} report - A callback function to report.
 * @returns {void}
 */
function eachSelfAssignment(left, right, report) {
    let i, j;

    if (!left || !right) {

        // do nothing
    } else if (
        left.type === "Identifier" &&
        right.type === "Identifier" &&
        left.name === right.name
    ) {
        report(right);
    } else if (
        left.type === "ArrayPattern" &&
        right.type === "ArrayExpression"
    ) {
        let end = Math.min(left.elements.length, right.elements.length);

        for (i = 0; i < end; ++i) {
            let rightElement = right.elements[i];

            eachSelfAssignment(left.elements[i], rightElement, report);

            // After a spread element, those indices are unknown.
            if (rightElement && rightElement.type === "SpreadElement") {
                break;
            }
        }
    } else if (
        left.type === "RestElement" &&
        right.type === "SpreadElement"
    ) {
        eachSelfAssignment(left.argument, right.argument, report);
    } else if (
        left.type === "ObjectPattern" &&
        right.type === "ObjectExpression" &&
        right.properties.length >= 1
    ) {

        // Gets the index of the last spread property.
        // It's possible to overwrite properties followed by it.
        let startJ = 0;

        for (i = right.properties.length - 1; i >= 0; --i) {
            if (right.properties[i].type === "ExperimentalSpreadProperty") {
                startJ = i + 1;
                break;
            }
        }

        for (i = 0; i < left.properties.length; ++i) {
            for (j = startJ; j < right.properties.length; ++j) {
                eachSelfAssignment(
                    left.properties[i],
                    right.properties[j],
                    report
                );
            }
        }
    } else if (
        left.type === "Property" &&
        right.type === "Property" &&
        !left.computed &&
        !right.computed &&
        right.kind === "init" &&
        !right.method &&
        left.key.name === right.key.name
    ) {
        eachSelfAssignment(left.value, right.value, report);
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow assignments where both sides are exactly the same",
            category: "Best Practices",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        /**
         * Reports a given node as self assignments.
         *
         * @param {ASTNode} node - A node to report. This is an Identifier node.
         * @returns {void}
         */
        function report(node) {
            context.report({
                node: node,
                message: "'{{name}}' is assigned to itself.",
                data: node
            });
        }

        return {
            AssignmentExpression: function(node) {
                if (node.operator === "=") {
                    eachSelfAssignment(node.left, node.right, report);
                }
            }
        };
    }
};
