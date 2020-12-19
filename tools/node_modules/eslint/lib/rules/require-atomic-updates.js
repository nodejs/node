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

/**
 * Checks if an expression is a variable that can only be observed within the given function.
 * @param {Variable|null} variable The variable to check
 * @param {boolean} isMemberAccess If `true` then this is a member access.
 * @returns {boolean} `true` if the variable is local to the given function, and is never referenced in a closure.
 */
function isLocalVariableWithoutEscape(variable, isMemberAccess) {
    if (!variable) {
        return false; // A global variable which was not defined.
    }

    // If the reference is a property access and the variable is a parameter, it handles the variable is not local.
    if (isMemberAccess && variable.defs.some(d => d.type === "Parameter")) {
        return false;
    }

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
        const outdatedReadVariableNames = new Set();
        const freshReadVariableNames = new Set();

        for (const prevSegment of segment.prevSegments) {
            const info = this.info.get(prevSegment);

            if (info) {
                info.outdatedReadVariableNames.forEach(Set.prototype.add, outdatedReadVariableNames);
                info.freshReadVariableNames.forEach(Set.prototype.add, freshReadVariableNames);
            }
        }

        this.info.set(segment, { outdatedReadVariableNames, freshReadVariableNames });
    }

    /**
     * Mark a given variable as read on given segments.
     * @param {PathSegment[]} segments The segments that it read the variable on.
     * @param {string} variableName The variable name to be read.
     * @returns {void}
     */
    markAsRead(segments, variableName) {
        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info) {
                info.freshReadVariableNames.add(variableName);

                // If a variable is freshly read again, then it's no more out-dated.
                info.outdatedReadVariableNames.delete(variableName);
            }
        }
    }

    /**
     * Move `freshReadVariableNames` to `outdatedReadVariableNames`.
     * @param {PathSegment[]} segments The segments to process.
     * @returns {void}
     */
    makeOutdated(segments) {
        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info) {
                info.freshReadVariableNames.forEach(Set.prototype.add, info.outdatedReadVariableNames);
                info.freshReadVariableNames.clear();
            }
        }
    }

    /**
     * Check if a given variable is outdated on the current segments.
     * @param {PathSegment[]} segments The current segments.
     * @param {string} variableName The variable name to check.
     * @returns {boolean} `true` if the variable is outdated on the segments.
     */
    isOutdated(segments, variableName) {
        for (const segment of segments) {
            const info = this.info.get(segment);

            if (info && info.outdatedReadVariableNames.has(variableName)) {
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
        const assignmentReferences = new Map();
        const segmentInfo = new SegmentInfo();
        let stack = null;

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
                const name = reference.identifier.name;
                const variable = reference.resolved;
                const writeExpr = getWriteExpr(reference);
                const isMemberAccess = reference.identifier.parent.type === "MemberExpression";

                // Add a fresh read variable.
                if (reference.isRead() && !(writeExpr && writeExpr.parent.operator === "=")) {
                    segmentInfo.markAsRead(codePath.currentSegments, name);
                }

                /*
                 * Register the variable to verify after ESLint traversed the `writeExpr` node
                 * if this reference is an assignment to a variable which is referred from other closure.
                 */
                if (writeExpr &&
                    writeExpr.parent.right === writeExpr && // ← exclude variable declarations.
                    !isLocalVariableWithoutEscape(variable, isMemberAccess)
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
             * If the reference exists in `outdatedReadVariableNames` list, report it.
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
                        const name = reference.identifier.name;

                        if (segmentInfo.isOutdated(codePath.currentSegments, name)) {
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
