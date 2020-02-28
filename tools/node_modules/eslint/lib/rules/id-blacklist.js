/**
 * @fileoverview Rule that warns when identifier names that are
 * blacklisted in the configuration are used.
 * @author Keith Cirkel (http://keithcirkel.co.uk)
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow specified identifiers",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/id-blacklist"
        },

        schema: {
            type: "array",
            items: {
                type: "string"
            },
            uniqueItems: true
        },
        messages: {
            blacklisted: "Identifier '{{name}}' is blacklisted."
        }
    },

    create(context) {


        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        const blacklist = context.options;
        const reportedNodes = new Set();


        /**
         * Checks if a string matches the provided pattern
         * @param {string} name The string to check.
         * @returns {boolean} if the string is a match
         * @private
         */
        function isInvalid(name) {
            return blacklist.indexOf(name) !== -1;
        }

        /**
         * Checks whether the given node represents an imported name that is renamed in the same import/export specifier.
         *
         * Examples:
         * import { a as b } from 'mod'; // node `a` is renamed import
         * export { a as b } from 'mod'; // node `a` is renamed import
         * @param {ASTNode} node `Identifier` node to check.
         * @returns {boolean} `true` if the node is a renamed import.
         */
        function isRenamedImport(node) {
            const parent = node.parent;

            return (
                (
                    parent.type === "ImportSpecifier" &&
                    parent.imported !== parent.local &&
                    parent.imported === node
                ) ||
                (
                    parent.type === "ExportSpecifier" &&
                    parent.parent.source && // re-export
                    parent.local !== parent.exported &&
                    parent.local === node
                )
            );
        }

        /**
         * Checks whether the given node is a renamed identifier node in an ObjectPattern destructuring.
         *
         * Examples:
         * const { a : b } = foo; // node `a` is renamed node.
         * @param {ASTNode} node `Identifier` node to check.
         * @returns {boolean} `true` if the node is a renamed node in an ObjectPattern destructuring.
         */
        function isRenamedInDestructuring(node) {
            const parent = node.parent;

            return (
                (
                    !parent.computed &&
                    parent.type === "Property" &&
                    parent.parent.type === "ObjectPattern" &&
                    parent.value !== node &&
                    parent.key === node
                )
            );
        }

        /**
         * Verifies if we should report an error or not.
         * @param {ASTNode} node The node to check
         * @returns {boolean} whether an error should be reported or not
         */
        function shouldReport(node) {
            const parent = node.parent;

            return (
                parent.type !== "CallExpression" &&
                parent.type !== "NewExpression" &&
                !isRenamedImport(node) &&
                !isRenamedInDestructuring(node) &&
                isInvalid(node.name)
            );
        }

        /**
         * Reports an AST node as a rule violation.
         * @param {ASTNode} node The node to report.
         * @returns {void}
         * @private
         */
        function report(node) {
            if (!reportedNodes.has(node)) {
                context.report({
                    node,
                    messageId: "blacklisted",
                    data: {
                        name: node.name
                    }
                });
                reportedNodes.add(node);
            }
        }

        return {

            Identifier(node) {

                // MemberExpressions get special rules
                if (node.parent.type === "MemberExpression") {
                    const name = node.name,
                        effectiveParent = node.parent.parent;

                    // Always check object names
                    if (node.parent.object.type === "Identifier" &&
                        node.parent.object.name === name) {
                        if (isInvalid(name)) {
                            report(node);
                        }

                    // Report AssignmentExpressions only if they are the left side of the assignment
                    } else if (effectiveParent.type === "AssignmentExpression" &&
                        (effectiveParent.right.type !== "MemberExpression" ||
                            effectiveParent.left.type === "MemberExpression" &&
                            effectiveParent.left.property.name === name)) {
                        if (isInvalid(name)) {
                            report(node);
                        }

                    // Report the last identifier in an ObjectPattern destructuring.
                    } else if (
                        (
                            effectiveParent.type === "Property" &&
                            effectiveParent.value === node.parent &&
                            effectiveParent.parent.type === "ObjectPattern"
                        ) ||
                        effectiveParent.type === "RestElement" ||
                        effectiveParent.type === "ArrayPattern" ||
                        (
                            effectiveParent.type === "AssignmentPattern" &&
                            effectiveParent.left === node.parent
                        )
                    ) {
                        if (isInvalid(name)) {
                            report(node);
                        }
                    }

                } else if (shouldReport(node)) {
                    report(node);
                }
            }

        };

    }
};
