/**
 * @fileoverview Rule to check for the usage of var.
 * @author Jamund Ferguson
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const SCOPE_NODE_TYPE = /^(?:Program|BlockStatement|SwitchStatement|ForStatement|ForInStatement|ForOfStatement)$/;

/**
 * Gets the scope node which directly contains a given node.
 *
 * @param {ASTNode} node - A node to get. This is a `VariableDeclaration` or
 *      an `Identifier`.
 * @returns {ASTNode} A scope node. This is one of `Program`, `BlockStatement`,
 *      `SwitchStatement`, `ForStatement`, `ForInStatement`, and
 *      `ForOfStatement`.
 */
function getScopeNode(node) {
    while (node) {
        if (SCOPE_NODE_TYPE.test(node.type)) {
            return node;
        }

        node = node.parent;
    }

    /* istanbul ignore next : unreachable */
    return null;
}

/**
 * Checks whether a given variable is redeclared or not.
 *
 * @param {escope.Variable} variable - A variable to check.
 * @returns {boolean} `true` if the variable is redeclared.
 */
function isRedeclared(variable) {
    return variable.defs.length >= 2;
}

/**
 * Checks whether a given variable is used from outside of the specified scope.
 *
 * @param {ASTNode} scopeNode - A scope node to check.
 * @returns {Function} The predicate function which checks whether a given
 *      variable is used from outside of the specified scope.
 */
function isUsedFromOutsideOf(scopeNode) {

    /**
     * Checks whether a given reference is inside of the specified scope or not.
     *
     * @param {escope.Reference} reference - A reference to check.
     * @returns {boolean} `true` if the reference is inside of the specified
     *      scope.
     */
    function isOutsideOfScope(reference) {
        const scope = scopeNode.range;
        const id = reference.identifier.range;

        return id[0] < scope[0] || id[1] > scope[1];
    }

    return function(variable) {
        return variable.references.some(isOutsideOfScope);
    };
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require `let` or `const` instead of `var`",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: [],
        fixable: "code"
    },

    create: function(context) {
        const sourceCode = context.getSourceCode();

        /**
         * Checks whether it can fix a given variable declaration or not.
         * It cannot fix if the following cases:
         *
         * - A variable is declared on a SwitchCase node.
         * - A variable is redeclared.
         * - A variable is used from outside the scope.
         *
         * ## A variable is declared on a SwitchCase node.
         *
         * If this rule modifies 'var' declarations on a SwitchCase node, it
         * would generate the warnings of 'no-case-declarations' rule. And the
         * 'eslint:recommended' preset includes 'no-case-declarations' rule, so
         * this rule doesn't modify those declarations.
         *
         * ## A variable is redeclared.
         *
         * The language spec disallows redeclarations of `let` declarations.
         * Those variables would cause syntax errors.
         *
         * ## A variable is used from outside the scope.
         *
         * The language spec disallows accesses from outside of the scope for
         * `let` declarations. Those variables would cause reference errors.
         *
         * @param {ASTNode} node - A variable declaration node to check.
         * @returns {boolean} `true` if it can fix the node.
         */
        function canFix(node) {
            const variables = context.getDeclaredVariables(node);
            const scopeNode = getScopeNode(node);

            return !(
                node.parent.type === "SwitchCase" ||
                variables.some(isRedeclared) ||
                variables.some(isUsedFromOutsideOf(scopeNode))
            );
        }

        /**
         * Reports a given variable declaration node.
         *
         * @param {ASTNode} node - A variable declaration node to report.
         * @returns {void}
         */
        function report(node) {
            const varToken = sourceCode.getFirstToken(node);

            context.report({
                node: node,
                message: "Unexpected var, use let or const instead.",

                fix: function(fixer) {
                    if (canFix(node)) {
                        return fixer.replaceText(varToken, "let");
                    }
                    return null;
                }
            });
        }

        return {
            VariableDeclaration: function(node) {
                if (node.kind === "var") {
                    report(node);
                }
            }
        };
    }
};
