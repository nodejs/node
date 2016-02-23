/**
 * @fileoverview A rule to suggest using of const declaration for variables that are never modified after declared.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var LOOP_TYPES = /^(?:While|DoWhile|For|ForIn|ForOf)Statement$/;
var FOR_IN_OF_TYPES = /^For(?:In|Of)Statement$/;
var SENTINEL_TYPES = /(?:Declaration|Statement)$/;
var END_POSITION_TYPES = /^(?:Assignment|Update)/;

/**
 * Gets a write reference from a given variable if the variable is modified only
 * once.
 *
 * @param {escope.Variable} variable - A variable to get.
 * @returns {escope.Reference|null} A write reference or null.
 */
function getWriteReferenceIfOnce(variable) {
    var retv = null;

    var references = variable.references;
    for (var i = 0; i < references.length; ++i) {
        var reference = references[i];

        if (reference.isWrite()) {
            if (retv && !(retv.init && reference.init)) {
                // This variable is modified two or more times.
                return null;
            }
            retv = reference;
        }
    }

    return retv;
}

/**
 * Checks whether or not a given reference is in a loop condition or a
 * for-loop's updater.
 *
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the reference is in a loop condition or a
 *      for-loop's updater.
 */
function isInLoopHead(reference) {
    var node = reference.identifier;
    var parent = node.parent;
    var assignment = false;

    while (parent) {
        if (LOOP_TYPES.test(parent.type)) {
            return true;
        }

        // VariableDeclaration can be at ForInStatement.left
        // This is catching the code like `for (const {b = ++a} of foo()) { ... }`
        if (assignment &&
            parent.type === "VariableDeclaration" &&
            FOR_IN_OF_TYPES.test(parent.parent.type) &&
            parent.parent.left === parent
        ) {
            return true;
        }
        if (parent.type === "AssignmentPattern") {
            assignment = true;
        }

        // If a declaration or a statement was found before a loop,
        // it's not in the head of a loop.
        if (SENTINEL_TYPES.test(parent.type)) {
            break;
        }

        node = parent;
        parent = parent.parent;
    }

    return false;
}

/**
 * Gets the end position of a given reference.
 * This position is used to check every ReadReferences exist after the given
 * reference.
 *
 * If the reference is belonging to AssignmentExpression or UpdateExpression,
 * this function returns the most rear position of those nodes.
 * The range of those nodes are executed before the assignment.
 *
 * @param {escope.Reference} writer - A reference to get.
 * @returns {number} The end position of the reference.
 */
function getEndPosition(writer) {
    var node = writer.identifier;
    var end = node.range[1];

    // Detects the end position of the assignment expression the reference is
    // belonging to.
    while ((node = node.parent)) {
        if (END_POSITION_TYPES.test(node.type)) {
            end = node.range[1];
        }
        if (SENTINEL_TYPES.test(node.type)) {
            break;
        }
    }

    return end;
}

/**
 * Gets a function which checks a given reference with the following condition:
 *
 * - The reference is a ReadReference.
 * - The reference exists after a specific WriteReference.
 * - The reference exists inside of the scope a specific WriteReference is
 *   belonging to.
 *
 * @param {escope.Reference} writer - A reference to check.
 * @returns {function} A function which checks a given reference.
 */
function isInScope(writer) {
    var start = getEndPosition(writer);
    var end = writer.from.block.range[1];

    return function(reference) {
        if (!reference.isRead()) {
            return true;
        }

        var range = reference.identifier.range;
        return start <= range[0] && range[1] <= end;
    };
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Searches and reports variables that are never modified after declared.
     * @param {Scope} scope - A scope of the search domain.
     * @returns {void}
     */
    function checkForVariables(scope) {
        // Skip the TDZ type.
        if (scope.type === "TDZ") {
            return;
        }

        var variables = scope.variables;
        for (var i = 0; i < variables.length; ++i) {
            var variable = variables[i];
            var def = variable.defs[0];
            var declaration = def && def.parent;
            var statement = declaration && declaration.parent;
            var references = variable.references;
            var identifier = variable.identifiers[0];

            // Skips excludes `let`.
            // And skips if it's at `ForStatement.init`.
            if (!declaration ||
                declaration.type !== "VariableDeclaration" ||
                declaration.kind !== "let" ||
                (statement.type === "ForStatement" && statement.init === declaration)
            ) {
                continue;
            }

            // Checks references.
            // - One WriteReference exists.
            // - Two or more WriteReference don't exist.
            // - Every ReadReference exists after the WriteReference.
            // - The WriteReference doesn't exist in a loop condition.
            // - If `eslintUsed` is true, we cannot know where it was used from.
            //   In this case, if the scope of the variable would change, it
            //   skips the variable.
            var writer = getWriteReferenceIfOnce(variable);
            if (writer &&
                !(variable.eslintUsed && variable.scope !== writer.from) &&
                !isInLoopHead(writer) &&
                references.every(isInScope(writer))
            ) {
                context.report({
                    node: identifier,
                    message: "'{{name}}' is never modified, use 'const' instead.",
                    data: identifier
                });
            }
        }
    }

    /**
     * Adds multiple items to the tail of an array.
     * @param {any[]} array - A destination to add.
     * @param {any[]} values - Items to be added.
     * @returns {void}
     */
    var pushAll = Function.apply.bind(Array.prototype.push);

    return {
        "Program:exit": function() {
            var stack = [context.getScope()];
            while (stack.length) {
                var scope = stack.pop();
                pushAll(stack, scope.childScopes);

                checkForVariables(scope);
            }
        }
    };

};

module.exports.schema = [];
