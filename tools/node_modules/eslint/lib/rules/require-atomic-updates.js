/**
 * @fileoverview disallow assignments that can lead to race conditions due to usage of `await` or `yield`
 * @author Teddy Katz
 * @author Toru Nagashima
 */
"use strict";

/**
 * Make the map from identifiers to each reference.
 * @param {escope.Scope} scope The scope to get references.
 * @param {Map<Identifier, escope.Reference>} [outReferenceMap] The map from identifier nodes to each reference object.
 * @returns {Map<Identifier, escope.Reference>} `referenceMap`.
 */
function createReferenceMap(scope, outReferenceMap = new Map()) {
    for (const reference of scope.references) {
        outReferenceMap.set(reference.identifier, reference);
    }
    for (const childScope of scope.childScopes) {
        if (childScope.type !== "function") {
            createReferenceMap(childScope, outReferenceMap);
        }
    }

    return outReferenceMap;
}

/**
 * Checks if an expression is a variable that can only be observed within the given function.
 * @param {escope.Variable} variable The variable to check
 * @returns {boolean} `true` if the variable is local to the given function, and is never referenced in a closure.
 */
function isLocalVariableWithoutEscape(variable) {
    const functionScope = variable.scope.variableScope;

    return variable.references.every(reference =>
        reference.from.variableScope === functionScope);
}

class SegmentInfo {
    constructor() {
        this.info = new WeakMap();
    }

    /**
     * Initialize the segment information.
     * @param {PathSegment} segment The segment to initialize.
     * @returns {void}
     */
    initialize(segment) {
        const outdatedReadVariables = new Set();
        const freshReadVariables = new Set();

        for (const prevSegment of segment.prevSegments) {
            const info = this.info.get(prevSegment);

            if (info) {
                info.outdatedReadVariables.forEach(Set.prototype.add, outdatedReadVariables);
                info.freshReadVariables.forEach(Set.prototype.add, freshReadVariables);
            }
        }

        this.info.set(segment, { outdatedReadVariables, freshReadVariables });
    }

    /**
     * Mark a given variable as read on given segments.
     * @param {PathSegment[]} segments The segments that it read the variable on.
     * @param {escope.Variable} variable The variable to be read.
     * @returns {void}
     */
    markAsRead(segments, variable) {
        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info) {
                info.freshReadVariables.add(variable);
            }
        }
    }

    /**
     * Move `freshReadVariables` to `outdatedReadVariables`.
     * @param {PathSegment[]} segments The segments to process.
     * @returns {void}
     */
    makeOutdated(segments) {
        const vars = new Set();

        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info) {
                info.freshReadVariables.forEach(Set.prototype.add, info.outdatedReadVariables);
                info.freshReadVariables.forEach(Set.prototype.add, vars);
                info.freshReadVariables.clear();
            }
        }
    }

    /**
     * Check if a given variable is outdated on the current segments.
     * @param {PathSegment[]} segments The current segments.
     * @param {escope.Variable} variable The variable to check.
     * @returns {boolean} `true` if the variable is outdated on the segments.
     */
    isOutdated(segments, variable) {
        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info && info.outdatedReadVariables.has(variable)) {
                return true;
            }
        }
        return false;
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow assignments that can lead to race conditions due to usage of `await` or `yield`",
            category: "Possible Errors",
            recommended: true,
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
        const globalScope = context.getScope();
        const dummyVariables = new Map();
        const assignmentReferences = new Map();
        const segmentInfo = new SegmentInfo();
        let stack = null;

        /**
         * Get the variable of a given reference.
         * If it's not defined, returns a dummy object.
         * @param {escope.Reference} reference The reference to get.
         * @returns {escope.Variable} The variable of the reference.
         */
        function getVariable(reference) {
            if (reference.resolved) {
                return reference.resolved;
            }

            // Get or create a dummy.
            const name = reference.identifier.name;
            let variable = dummyVariables.get(name);

            if (!variable) {
                variable = {
                    name,
                    scope: globalScope,
                    references: []
                };
                dummyVariables.set(name, variable);
            }
            variable.references.push(reference);

            return variable;
        }

        /**
         * Get `reference.writeExpr` of a given reference.
         * If it's the read reference of MemberExpression in LHS, returns RHS in order to address `a.b = await a`
         * @param {escope.Reference} reference The reference to get.
         * @returns {Expression|null} The `reference.writeExpr`.
         */
        function getWriteExpr(reference) {
            if (reference.writeExpr) {
                return reference.writeExpr;
            }
            let node = reference.identifier;

            while (node) {
                const t = node.parent.type;

                if (t === "AssignmentExpression" && node.parent.left === node) {
                    return node.parent.right;
                }
                if (t === "MemberExpression" && node.parent.object === node) {
                    node = node.parent;
                    continue;
                }

                break;
            }

            return null;
        }

        return {
            onCodePathStart(codePath) {
                const scope = context.getScope();
                const shouldVerify =
                    scope.type === "function" &&
                    (scope.block.async || scope.block.generator);

                stack = {
                    upper: stack,
                    codePath,
                    referenceMap: shouldVerify ? createReferenceMap(scope) : null
                };
            },
            onCodePathEnd() {
                stack = stack.upper;
            },

            // Initialize the segment information.
            onCodePathSegmentStart(segment) {
                segmentInfo.initialize(segment);
            },

            // Handle references to prepare verification.
            Identifier(node) {
                const { codePath, referenceMap } = stack;
                const reference = referenceMap && referenceMap.get(node);

                // Ignore if this is not a valid variable reference.
                if (!reference) {
                    return;
                }
                const variable = getVariable(reference);
                const writeExpr = getWriteExpr(reference);

                // Add a fresh read variable.
                if (reference.isRead() && !(writeExpr && writeExpr.parent.operator === "=")) {
                    segmentInfo.markAsRead(codePath.currentSegments, variable);
                }

                /*
                 * Register the variable to verify after ESLint traversed the `writeExpr` node
                 * if this reference is an assignment to a variable which is referred from other clausure.
                 */
                if (writeExpr &&
                    writeExpr.parent.right === writeExpr && // ← exclude variable declarations.
                    !isLocalVariableWithoutEscape(variable)
                ) {
                    let refs = assignmentReferences.get(writeExpr);

                    if (!refs) {
                        refs = [];
                        assignmentReferences.set(writeExpr, refs);
                    }

                    refs.push(reference);
                }
            },

            /*
             * Verify assignments.
             * If the reference exists in `outdatedReadVariables` list, report it.
             */
            ":expression:exit"(node) {
                const { codePath, referenceMap } = stack;

                // referenceMap exists if this is in a resumable function scope.
                if (!referenceMap) {
                    return;
                }

                // Mark the read variables on this code path as outdated.
                if (node.type === "AwaitExpression" || node.type === "YieldExpression") {
                    segmentInfo.makeOutdated(codePath.currentSegments);
                }

                // Verify.
                const references = assignmentReferences.get(node);

                if (references) {
                    assignmentReferences.delete(node);

                    for (const reference of references) {
                        const variable = getVariable(reference);

                        if (segmentInfo.isOutdated(codePath.currentSegments, variable)) {
                            context.report({
                                node: node.parent,
                                messageId: "nonAtomicUpdate",
                                data: {
                                    value: sourceCode.getText(node.parent.left)
                                }
                            });
                        }
                    }
                }
            }
        };
    }
};
