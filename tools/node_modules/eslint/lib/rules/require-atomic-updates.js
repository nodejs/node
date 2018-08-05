/**
 * @fileoverview disallow assignments that can lead to race conditions due to usage of `await` or `yield`
 * @author Teddy Katz
 */
"use strict";

const astUtils = require("../util/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow assignments that can lead to race conditions due to usage of `await` or `yield`",
            category: "Possible Errors",
            recommended: false,
            url: "https://eslint.org/docs/rules/require-atomic-updates"
        },
        fixable: null,
        schema: [],
        messages: {
            nonAtomicUpdate: "Possible race condition: `{{value}}` might be reassigned based on an outdated value of `{{value}}`."
        }
    },

    create(context) {
        const sourceCode = context.getSourceCode();
        const identifierToSurroundingFunctionMap = new WeakMap();
        const expressionsByCodePathSegment = new Map();

        //----------------------------------------------------------------------
        // Helpers
        //----------------------------------------------------------------------

        const resolvedVariableCache = new WeakMap();

        /**
         * Gets the variable scope around this variable reference
         * @param {ASTNode} identifier An `Identifier` AST node
         * @returns {Scope|null} An escope Scope
         */
        function getScope(identifier) {
            for (let currentNode = identifier; currentNode; currentNode = currentNode.parent) {
                const scope = sourceCode.scopeManager.acquire(currentNode, true);

                if (scope) {
                    return scope;
                }
            }
            return null;
        }

        /**
         * Resolves a given identifier to a given scope
         * @param {ASTNode} identifier An `Identifier` AST node
         * @param {Scope} scope An escope Scope
         * @returns {Variable|null} An escope Variable corresponding to the given identifier
         */
        function resolveVariableInScope(identifier, scope) {
            return scope.variables.find(variable => variable.name === identifier.name) ||
                (scope.upper ? resolveVariableInScope(identifier, scope.upper) : null);
        }

        /**
         * Resolves an identifier to a variable
         * @param {ASTNode} identifier An identifier node
         * @returns {Variable|null} The escope Variable that uses this identifier
         */
        function resolveVariable(identifier) {
            if (!resolvedVariableCache.has(identifier)) {
                const surroundingScope = getScope(identifier);

                if (surroundingScope) {
                    resolvedVariableCache.set(identifier, resolveVariableInScope(identifier, surroundingScope));
                } else {
                    resolvedVariableCache.set(identifier, null);
                }
            }

            return resolvedVariableCache.get(identifier);
        }

        /**
         * Checks if an expression is a variable that can only be observed within the given function.
         * @param {ASTNode} expression The expression to check
         * @param {ASTNode} surroundingFunction The function node
         * @returns {boolean} `true` if the expression is a variable which is local to the given function, and is never
         * referenced in a closure.
         */
        function isLocalVariableWithoutEscape(expression, surroundingFunction) {
            if (expression.type !== "Identifier") {
                return false;
            }

            const variable = resolveVariable(expression);

            if (!variable) {
                return false;
            }

            return variable.references.every(reference => identifierToSurroundingFunctionMap.get(reference.identifier) === surroundingFunction) &&
                variable.defs.every(def => identifierToSurroundingFunctionMap.get(def.name) === surroundingFunction);
        }

        /**
         * Reports an AssignmentExpression node that has a non-atomic update
         * @param {ASTNode} assignmentExpression The assignment that is potentially unsafe
         * @returns {void}
         */
        function reportAssignment(assignmentExpression) {
            context.report({
                node: assignmentExpression,
                messageId: "nonAtomicUpdate",
                data: {
                    value: sourceCode.getText(assignmentExpression.left)
                }
            });
        }

        /**
         * If the control flow graph of a function enters an assignment expression, then does the
         * both of the following steps in order (possibly with other steps in between) before exiting the
         * assignment expression, then the assignment might be using an outdated value.
         * 1. Enters a read of the variable or property assigned in the expression (not necessary if operator assignment is used)
         * 2. Exits an `await` or `yield` expression
         *
         * This function checks for the outdated values and reports them.
         * @param {CodePathSegment} codePathSegment The current code path segment to traverse
         * @param {ASTNode} surroundingFunction The function node containing the code path segment
         * @returns {void}
         */
        function findOutdatedReads(
            codePathSegment,
            surroundingFunction,
            {
                seenSegments = new Set(),
                openAssignmentsWithoutReads = new Set(),
                openAssignmentsWithReads = new Set()
            } = {}
        ) {
            if (seenSegments.has(codePathSegment)) {

                // An AssignmentExpression can't contain loops, so it's not necessary to reenter them with new state.
                return;
            }

            expressionsByCodePathSegment.get(codePathSegment).forEach(({ entering, node }) => {
                if (node.type === "AssignmentExpression") {
                    if (entering) {
                        (node.operator === "=" ? openAssignmentsWithoutReads : openAssignmentsWithReads).add(node);
                    } else {
                        openAssignmentsWithoutReads.delete(node);
                        openAssignmentsWithReads.delete(node);
                    }
                } else if (!entering && (node.type === "AwaitExpression" || node.type === "YieldExpression")) {
                    [...openAssignmentsWithReads]
                        .filter(assignment => !isLocalVariableWithoutEscape(assignment.left, surroundingFunction))
                        .forEach(reportAssignment);

                    openAssignmentsWithReads.clear();
                } else if (!entering && (node.type === "Identifier" || node.type === "MemberExpression")) {
                    [...openAssignmentsWithoutReads]
                        .filter(assignment => (
                            assignment.left !== node &&
                            assignment.left.type === node.type &&
                            astUtils.equalTokens(assignment.left, node, sourceCode)
                        ))
                        .forEach(assignment => {
                            openAssignmentsWithoutReads.delete(assignment);
                            openAssignmentsWithReads.add(assignment);
                        });
                }
            });

            codePathSegment.nextSegments.forEach(nextSegment => {
                findOutdatedReads(
                    nextSegment,
                    surroundingFunction,
                    {
                        seenSegments: new Set(seenSegments).add(codePathSegment),
                        openAssignmentsWithoutReads: new Set(openAssignmentsWithoutReads),
                        openAssignmentsWithReads: new Set(openAssignmentsWithReads)
                    }
                );
            });
        }

        //----------------------------------------------------------------------
        // Public
        //----------------------------------------------------------------------

        const currentCodePathSegmentStack = [];
        let currentCodePathSegment = null;
        const functionStack = [];

        return {
            onCodePathStart() {
                currentCodePathSegmentStack.push(currentCodePathSegment);
            },

            onCodePathEnd(codePath, node) {
                currentCodePathSegment = currentCodePathSegmentStack.pop();

                if (astUtils.isFunction(node) && (node.async || node.generator)) {
                    findOutdatedReads(codePath.initialSegment, node);
                }
            },

            onCodePathSegmentStart(segment) {
                currentCodePathSegment = segment;
                expressionsByCodePathSegment.set(segment, []);
            },

            "AssignmentExpression, Identifier, MemberExpression, AwaitExpression, YieldExpression"(node) {
                expressionsByCodePathSegment.get(currentCodePathSegment).push({ entering: true, node });
            },

            "AssignmentExpression, Identifier, MemberExpression, AwaitExpression, YieldExpression:exit"(node) {
                expressionsByCodePathSegment.get(currentCodePathSegment).push({ entering: false, node });
            },

            ":function"(node) {
                functionStack.push(node);
            },

            ":function:exit"() {
                functionStack.pop();
            },

            Identifier(node) {
                if (functionStack.length) {
                    identifierToSurroundingFunctionMap.set(node, functionStack[functionStack.length - 1]);
                }
            }
        };
    }
};
