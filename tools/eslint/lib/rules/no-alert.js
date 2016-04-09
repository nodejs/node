/**
 * @fileoverview Rule to flag use of alert, confirm, prompt
 * @author Nicholas C. Zakas
 * @copyright 2015 Mathias Schreck
 * @copyright 2013 Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks if the given name is a prohibited identifier.
 * @param {string} name The name to check
 * @returns {boolean} Whether or not the name is prohibited.
 */
function isProhibitedIdentifier(name) {
    return /^(alert|confirm|prompt)$/.test(name);
}

/**
 * Reports the given node and identifier name.
 * @param {RuleContext} context The ESLint rule context.
 * @param {ASTNode} node The node to report on.
 * @param {string} identifierName The name of the identifier.
 * @returns {void}
 */
function report(context, node, identifierName) {
    context.report(node, "Unexpected {{name}}.", { name: identifierName });
}

/**
 * Returns the property name of a MemberExpression.
 * @param {ASTNode} memberExpressionNode The MemberExpression node.
 * @returns {string|null} Returns the property name if available, null else.
 */
function getPropertyName(memberExpressionNode) {
    if (memberExpressionNode.computed) {
        if (memberExpressionNode.property.type === "Literal") {
            return memberExpressionNode.property.value;
        }
    } else {
        return memberExpressionNode.property.name;
    }
    return null;
}

/**
 * Finds the escope reference in the given scope.
 * @param {Object} scope The scope to search.
 * @param {ASTNode} node The identifier node.
 * @returns {Reference|null} Returns the found reference or null if none were found.
 */
function findReference(scope, node) {
    var references = scope.references.filter(function(reference) {
        return reference.identifier.range[0] === node.range[0] &&
            reference.identifier.range[1] === node.range[1];
    });

    if (references.length === 1) {
        return references[0];
    }
    return null;
}

/**
 * Checks if the given identifier node is shadowed in the given scope.
 * @param {Object} scope The current scope.
 * @param {Object} globalScope The global scope.
 * @param {string} node The identifier node to check
 * @returns {boolean} Whether or not the name is shadowed.
 */
function isShadowed(scope, globalScope, node) {
    var reference = findReference(scope, node);

    return reference && reference.resolved && reference.resolved.defs.length > 0;
}

/**
 * Checks if the given identifier node is a ThisExpression in the global scope or the global window property.
 * @param {Object} scope The current scope.
 * @param {Object} globalScope The global scope.
 * @param {string} node The identifier node to check
 * @returns {boolean} Whether or not the node is a reference to the global object.
 */
function isGlobalThisReferenceOrGlobalWindow(scope, globalScope, node) {
    if (scope.type === "global" && node.type === "ThisExpression") {
        return true;
    } else if (node.name === "window") {
        return !isShadowed(scope, globalScope, node);
    }

    return false;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var globalScope;

    return {

        "Program": function() {
            globalScope = context.getScope();
        },

        "CallExpression": function(node) {
            var callee = node.callee,
                identifierName,
                currentScope = context.getScope();

            // without window.
            if (callee.type === "Identifier") {
                identifierName = callee.name;

                if (!isShadowed(currentScope, globalScope, callee) && isProhibitedIdentifier(callee.name)) {
                    report(context, node, identifierName);
                }

            } else if (callee.type === "MemberExpression" && isGlobalThisReferenceOrGlobalWindow(currentScope, globalScope, callee.object)) {
                identifierName = getPropertyName(callee);

                if (isProhibitedIdentifier(identifierName)) {
                    report(context, node, identifierName);
                }
            }

        }
    };

};

module.exports.schema = [];
