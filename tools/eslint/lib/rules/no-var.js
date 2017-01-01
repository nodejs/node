/**
 * @fileoverview Rule to check for the usage of var.
 * @author Jamund Ferguson
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Finds the nearest function scope or global scope walking up the scope
 * hierarchy.
 *
 * @param {escope.Scope} scope - The scope to traverse.
 * @returns {escope.Scope} a function scope or global scope containing the given
 *      scope.
 */
function getEnclosingFunctionScope(scope) {
    while (scope.type !== "function" && scope.type !== "global") {
        scope = scope.upper;
    }
    return scope;
}

/**
 * Checks whether the given variable has any references from a more specific
 * function expression (i.e. a closure).
 *
 * @param {escope.Variable} variable - A variable to check.
 * @returns {boolean} `true` if the variable is used from a closure.
 */
function isReferencedInClosure(variable) {
    const enclosingFunctionScope = getEnclosingFunctionScope(variable.scope);

    return variable.references.some(reference =>
        getEnclosingFunctionScope(reference.from) !== enclosingFunctionScope);
}

/**
 * Checks whether the given node is the assignee of a loop.
 *
 * @param {ASTNode} node - A VariableDeclaration node to check.
 * @returns {boolean} `true` if the declaration is assigned as part of loop
 *      iteration.
 */
function isLoopAssignee(node) {
    return (node.parent.type === "ForOfStatement" || node.parent.type === "ForInStatement") &&
        node === node.parent.left;
}

/**
 * Checks whether the given variable declaration is immediately initialized.
 *
 * @param {ASTNode} node - A VariableDeclaration node to check.
 * @returns {boolean} `true` if the declaration has an initializer.
 */
function isDeclarationInitialized(node) {
    return node.declarations.every(declarator => declarator.init !== null);
}

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

    create(context) {
        const sourceCode = context.getSourceCode();

        /**
         * Checks whether it can fix a given variable declaration or not.
         * It cannot fix if the following cases:
         *
         * - A variable is declared on a SwitchCase node.
         * - A variable is redeclared.
         * - A variable is used from outside the scope.
         * - A variable is used from a closure within a loop.
         * - A variable might be used before it is assigned within a loop.
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
         * ## A variable is used from a closure within a loop.
         *
         * A `var` declaration within a loop shares the same variable instance
         * across all loop iterations, while a `let` declaration creates a new
         * instance for each iteration. This means if a variable in a loop is
         * referenced by any closure, changing it from `var` to `let` would
         * change the behavior in a way that is generally unsafe.
         *
         * ## A variable might be used before it is assigned within a loop.
         *
         * Within a loop, a `let` declaration without an initializer will be
         * initialized to null, while a `var` declaration will retain its value
         * from the previous iteration, so it is only safe to change `var` to
         * `let` if we can statically determine that the variable is always
         * assigned a value before its first access in the loop body. To keep
         * the implementation simple, we only convert `var` to `let` within
         * loops when the variable is a loop assignee or the declaration has an
         * initializer.
         *
         * @param {ASTNode} node - A variable declaration node to check.
         * @returns {boolean} `true` if it can fix the node.
         */
        function canFix(node) {
            const variables = context.getDeclaredVariables(node);
            const scopeNode = getScopeNode(node);

            if (node.parent.type === "SwitchCase" ||
                    variables.some(isRedeclared) ||
                    variables.some(isUsedFromOutsideOf(scopeNode))) {
                return false;
            }

            if (astUtils.isInLoop(node)) {
                if (variables.some(isReferencedInClosure)) {
                    return false;
                }
                if (!isLoopAssignee(node) && !isDeclarationInitialized(node)) {
                    return false;
                }
            }

            return true;
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
                node,
                message: "Unexpected var, use let or const instead.",

                fix(fixer) {
                    if (canFix(node)) {
                        return fixer.replaceText(varToken, "let");
                    }
                    return null;
                }
            });
        }

        return {
            VariableDeclaration(node) {
                if (node.kind === "var") {
                    report(node);
                }
            }
        };
    }
};
