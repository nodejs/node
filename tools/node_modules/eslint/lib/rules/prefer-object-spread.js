/**
 * @fileoverview Prefers object spread property over Object.assign
 * @author Sharmila Jesupaul
 * See LICENSE file in root directory for full license.
 */

"use strict";

const matchAll = require("string.prototype.matchall");

/**
 * Helper that checks if the node is an Object.assign call
 * @param {ASTNode} node - The node that the rule warns on
 * @returns {boolean} - Returns true if the node is an Object.assign call
 */
function isObjectAssign(node) {
    return (
        node.callee &&
        node.callee.type === "MemberExpression" &&
        node.callee.object.name === "Object" &&
        node.callee.property.name === "assign"
    );
}

/**
 * Helper that checks if the Object.assign call has array spread
 * @param {ASTNode} node - The node that the rule warns on
 * @returns {boolean} - Returns true if the Object.assign call has array spread
 */
function hasArraySpread(node) {
    return node.arguments.some(arg => arg.type === "SpreadElement");
}

/**
 * Helper that checks if the node needs parentheses to be valid JS.
 * The default is to wrap the node in parentheses to avoid parsing errors.
 * @param {ASTNode} node - The node that the rule warns on
 * @returns {boolean} - Returns true if the node needs parentheses
 */
function needsParens(node) {
    const parent = node.parent;

    if (!parent || !node.type) {
        return true;
    }

    switch (parent.type) {
        case "VariableDeclarator":
        case "ArrayExpression":
        case "ReturnStatement":
        case "CallExpression":
        case "Property":
            return false;
        default:
            return true;
    }
}

/**
 * Determines if an argument needs parentheses. The default is to not add parens.
 * @param {ASTNode} node - The node to be checked.
 * @returns {boolean} True if the node needs parentheses
 */
function argNeedsParens(node) {
    if (!node.type) {
        return false;
    }

    switch (node.type) {
        case "AssignmentExpression":
        case "ArrowFunctionExpression":
        case "ConditionalExpression":
            return true;
        default:
            return false;
    }
}

/**
 * Helper that adds a comma after the last non-whitespace character that is not a part of a comment.
 * @param {string} formattedArg - String of argument text
 * @param {array} comments - comments inside the argument
 * @returns {string} - argument with comma at the end of it
 */
function addComma(formattedArg, comments) {
    const nonWhitespaceCharacterRegex = /[^\s\\]/g;
    const nonWhitespaceCharacters = Array.from(matchAll(formattedArg, nonWhitespaceCharacterRegex));
    const commentRanges = comments.map(comment => comment.range);
    const validWhitespaceMatches = [];

    // Create a list of indexes where non-whitespace characters exist.
    nonWhitespaceCharacters.forEach(match => {
        const insertIndex = match.index + match[0].length;

        if (!commentRanges.length) {
            validWhitespaceMatches.push(insertIndex);
        }

        // If comment ranges are found make sure that the non whitespace characters are not part of the comment.
        commentRanges.forEach(arr => {
            const commentStart = arr[0];
            const commentEnd = arr[1];

            if (insertIndex < commentStart || insertIndex > commentEnd) {
                validWhitespaceMatches.push(insertIndex);
            }
        });
    });
    const insertPos = Math.max(...validWhitespaceMatches);
    const regex = new RegExp(`^((?:.|[^/s/S]){${insertPos}}) *`);

    return formattedArg.replace(regex, "$1, ");
}

/**
 * Helper formats an argument by either removing curlies or adding a spread operator
 * @param {ASTNode|null} arg - ast node representing argument to format
 * @param {boolean} isLast - true if on the last element of the array
 * @param {Object} sourceCode - in context sourcecode object
 * @param {array} comments - comments inside checked node
 * @returns {string} - formatted argument
 */
function formatArg(arg, isLast, sourceCode, comments) {
    const text = sourceCode.getText(arg);
    const parens = argNeedsParens(arg);
    const spread = arg.type === "SpreadElement" ? "" : "...";

    if (arg.type === "ObjectExpression" && arg.properties.length === 0) {
        return "";
    }

    if (arg.type === "ObjectExpression") {

        /**
         * This regex finds the opening curly brace and any following spaces and replaces it with whatever
         * exists before the curly brace. It also does the same for the closing curly brace. This is to avoid
         * having multiple spaces around the object expression depending on how the object properties are spaced.
         */
        const formattedObjectLiteral = text.replace(/^(.*){ */, "$1").replace(/ *}([^}]*)$/, "$1");

        return isLast ? formattedObjectLiteral : addComma(formattedObjectLiteral, comments);
    }

    if (isLast) {
        return parens ? `${spread}(${text})` : `${spread}${text}`;
    }

    return parens ? addComma(`${spread}(${text})`, comments) : `${spread}${addComma(text, comments)}`;
}

/**
 * Autofixes the Object.assign call to use an object spread instead.
 * @param {ASTNode|null} node - The node that the rule warns on, i.e. the Object.assign call
 * @param {string} sourceCode - sourceCode of the Object.assign call
 * @returns {Function} autofixer - replaces the Object.assign with a spread object.
 */
function autofixSpread(node, sourceCode) {
    return fixer => {
        const args = node.arguments;
        const firstArg = args[0];
        const lastArg = args[args.length - 1];
        const parens = needsParens(node);
        const comments = sourceCode.getCommentsInside(node);
        const replaceObjectAssignStart = fixer.replaceTextRange(
            [node.range[0], firstArg.range[0]],
            `${parens ? "({" : "{"}`
        );

        const handleArgs = args
            .map((arg, i, arr) => formatArg(arg, i + 1 >= arr.length, sourceCode, comments))
            .filter(arg => arg !== "," && arg !== "");

        const insertBody = fixer.replaceTextRange([firstArg.range[0], lastArg.range[1]], handleArgs.join(""));
        const replaceObjectAssignEnd = fixer.replaceTextRange([lastArg.range[1], node.range[1]], `${parens ? "})" : "}"}`);

        return [
            replaceObjectAssignStart,
            insertBody,
            replaceObjectAssignEnd
        ];
    };
}

/**
 * Autofixes the Object.assign call with a single object literal as an argument
 * @param {ASTNode|null} node - The node that the rule warns on, i.e. the Object.assign call
 * @param {string} sourceCode - sourceCode of the Object.assign call
 * @returns {Function} autofixer - replaces the Object.assign with a object literal.
 */
function autofixObjectLiteral(node, sourceCode) {
    return fixer => {
        const argument = node.arguments[0];
        const parens = needsParens(node);

        return fixer.replaceText(node, `${parens ? "(" : ""}${sourceCode.text.slice(argument.range[0], argument.range[1])}${parens ? ")" : ""}`);
    };
}

/**
 * Check if the node has modified a given variable
 * @param {ASTNode|null} node - The node that the rule warns on, i.e. the Object.assign call
 * @returns {boolean} - true if node is an assignment, variable declaration, or import statement
 */
function isModifier(node) {
    if (!node.type) {
        return false;
    }

    return node.type === "AssignmentExpression" ||
        node.type === "VariableDeclarator" ||
        node.type === "ImportDeclaration";
}

/**
 * Check if the node has modified a given variable
 * @param {array} references - list of reference nodes
 * @returns {boolean} - true if node is has been overwritten by an assignment or import
 */
function modifyingObjectReference(references) {
    return references.some(ref => (
        ref.identifier &&
        ref.identifier.parent &&
        isModifier(ref.identifier.parent)
    ));
}


module.exports = {
    meta: {
        docs: {
            description:
                "disallow using Object.assign with an object literal as the first argument and prefer the use of object spread instead.",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/prefer-object-spread"
        },
        schema: [],
        fixable: "code",
        messages: {
            useSpreadMessage: "Use an object spread instead of `Object.assign` eg: `{ ...foo }`",
            useLiteralMessage: "Use an object literal instead of `Object.assign`. eg: `{ foo: bar }`"
        }
    },

    create: function rule(context) {
        const sourceCode = context.getSourceCode();
        const scope = context.getScope();
        const objectVariable = scope.variables.filter(variable => variable.name === "Object");
        const moduleReferences = scope.childScopes.filter(childScope => {
            const varNamedObject = childScope.variables.filter(variable => variable.name === "Object");

            return childScope.type === "module" && varNamedObject.length;
        });
        const references = [].concat(...objectVariable.map(variable => variable.references || []));

        return {
            CallExpression(node) {

                /*
                 * If current file is either importing Object or redefining it,
                 * we skip warning on this rule.
                 */
                if (moduleReferences.length || (references.length && modifyingObjectReference(references))) {
                    return;
                }

                /*
                 * The condition below is cases where Object.assign has a single argument and
                 * that argument is an object literal. e.g. `Object.assign({ foo: bar })`.
                 * For now, we will warn on this case and autofix it.
                 */
                if (
                    node.arguments.length === 1 &&
                    node.arguments[0].type === "ObjectExpression" &&
                    isObjectAssign(node)
                ) {
                    context.report({
                        node,
                        messageId: "useLiteralMessage",
                        fix: autofixObjectLiteral(node, sourceCode)
                    });
                }

                /*
                 * The condition below warns on `Object.assign` calls that that have
                 * an object literal as the first argument and have a second argument
                 * that can be spread. e.g `Object.assign({ foo: bar }, baz)`
                 */
                if (
                    node.arguments.length > 1 &&
                    node.arguments[0].type === "ObjectExpression" &&
                    isObjectAssign(node) &&
                    !hasArraySpread(node)
                ) {
                    context.report({
                        node,
                        messageId: "useSpreadMessage",
                        fix: autofixSpread(node, sourceCode)
                    });
                }
            }
        };
    }
};
