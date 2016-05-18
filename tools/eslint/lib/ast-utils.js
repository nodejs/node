/**
 * @fileoverview Common utils for AST.
 * @author Gyandeep Singh
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var esutils = require("esutils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var anyFunctionPattern = /^(?:Function(?:Declaration|Expression)|ArrowFunctionExpression)$/;
var arrayOrTypedArrayPattern = /Array$/;
var arrayMethodPattern = /^(?:every|filter|find|findIndex|forEach|map|some)$/;
var bindOrCallOrApplyPattern = /^(?:bind|call|apply)$/;
var breakableTypePattern = /^(?:(?:Do)?While|For(?:In|Of)?|Switch)Statement$/;
var thisTagPattern = /^[\s\*]*@this/m;

/**
 * Checks reference if is non initializer and writable.
 * @param {Reference} reference - A reference to check.
 * @param {int} index - The index of the reference in the references.
 * @param {Reference[]} references - The array that the reference belongs to.
 * @returns {boolean} Success/Failure
 * @private
 */
function isModifyingReference(reference, index, references) {
    var identifier = reference.identifier,
        modifyingDifferentIdentifier;

    /*
     * Destructuring assignments can have multiple default value, so
     * possibly there are multiple writeable references for the same
     * identifier.
     */
    modifyingDifferentIdentifier = index === 0 ||
        references[index - 1].identifier !== identifier;

    return (identifier &&
        reference.init === false &&
        reference.isWrite() &&
        modifyingDifferentIdentifier
    );
}

/**
 * Checks whether or not a node is a constructor.
 * @param {ASTNode} node - A function node to check.
 * @returns {boolean} Wehether or not a node is a constructor.
 */
function isES5Constructor(node) {
    return (
        node.id &&
        node.id.name[0] === node.id.name[0].toLocaleUpperCase()
    );
}

/**
 * Finds a function node from ancestors of a node.
 * @param {ASTNode} node - A start node to find.
 * @returns {Node|null} A found function node.
 */
function getUpperFunction(node) {
    while (node) {
        if (anyFunctionPattern.test(node.type)) {
            return node;
        }
        node = node.parent;
    }
    return null;
}

/**
 * Checks whether or not a node is `null` or `undefined`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a `null` or `undefined`.
 * @public
 */
function isNullOrUndefined(node) {
    return (
        (node.type === "Literal" && node.value === null) ||
        (node.type === "Identifier" && node.name === "undefined") ||
        (node.type === "UnaryExpression" && node.operator === "void")
    );
}

/**
 * Checks whether or not a node is callee.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is callee.
 */
function isCallee(node) {
    return node.parent.type === "CallExpression" && node.parent.callee === node;
}

/**
 * Checks whether or not a node is `Reclect.apply`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a `Reclect.apply`.
 */
function isReflectApply(node) {
    return (
        node.type === "MemberExpression" &&
        node.object.type === "Identifier" &&
        node.object.name === "Reflect" &&
        node.property.type === "Identifier" &&
        node.property.name === "apply" &&
        node.computed === false
    );
}

/**
 * Checks whether or not a node is `Array.from`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a `Array.from`.
 */
function isArrayFromMethod(node) {
    return (
        node.type === "MemberExpression" &&
        node.object.type === "Identifier" &&
        arrayOrTypedArrayPattern.test(node.object.name) &&
        node.property.type === "Identifier" &&
        node.property.name === "from" &&
        node.computed === false
    );
}

/**
 * Checks whether or not a node is a method which has `thisArg`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a method which has `thisArg`.
 */
function isMethodWhichHasThisArg(node) {
    while (node) {
        if (node.type === "Identifier") {
            return arrayMethodPattern.test(node.name);
        }
        if (node.type === "MemberExpression" && !node.computed) {
            node = node.property;
            continue;
        }

        break;
    }

    return false;
}

/**
 * Checks whether or not a node has a `@this` tag in its comments.
 * @param {ASTNode} node - A node to check.
 * @param {SourceCode} sourceCode - A SourceCode instance to get comments.
 * @returns {boolean} Whether or not the node has a `@this` tag in its comments.
 */
function hasJSDocThisTag(node, sourceCode) {
    var jsdocComment = sourceCode.getJSDocComment(node);

    if (jsdocComment && thisTagPattern.test(jsdocComment.value)) {
        return true;
    }

    // Checks `@this` in its leading comments for callbacks,
    // because callbacks don't have its JSDoc comment.
    // e.g.
    //     sinon.test(/* @this sinon.Sandbox */function() { this.spy(); });
    return sourceCode.getComments(node).leading.some(function(comment) {
        return thisTagPattern.test(comment.value);
    });
}

/**
 * Determines if a node is surrounded by parentheses.
 * @param {RuleContext} context The context object passed to the rule
 * @param {ASTNode} node The node to be checked.
 * @returns {boolean} True if the node is parenthesised.
 * @private
 */
function isParenthesised(context, node) {
    var previousToken = context.getTokenBefore(node),
        nextToken = context.getTokenAfter(node);

    return Boolean(previousToken && nextToken) &&
        previousToken.value === "(" && previousToken.range[1] <= node.range[0] &&
        nextToken.value === ")" && nextToken.range[0] >= node.range[1];
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    /**
     * Determines whether two adjacent tokens are on the same line.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not the tokens are on the same line.
     * @public
     */
    isTokenOnSameLine: function(left, right) {
        return left.loc.end.line === right.loc.start.line;
    },

    isNullOrUndefined: isNullOrUndefined,
    isCallee: isCallee,
    isES5Constructor: isES5Constructor,
    getUpperFunction: getUpperFunction,
    isArrayFromMethod: isArrayFromMethod,
    isParenthesised: isParenthesised,

    /**
     * Checks whether or not a given node is a string literal.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} `true` if the node is a string literal.
     */
    isStringLiteral: function(node) {
        return (
            (node.type === "Literal" && typeof node.value === "string") ||
            node.type === "TemplateLiteral"
        );
    },

    /**
     * Checks whether a given node is a breakable statement or not.
     * The node is breakable if the node is one of the following type:
     *
     * - DoWhileStatement
     * - ForInStatement
     * - ForOfStatement
     * - ForStatement
     * - SwitchStatement
     * - WhileStatement
     *
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} `true` if the node is breakable.
     */
    isBreakableStatement: function(node) {
        return breakableTypePattern.test(node.type);
    },

    /**
     * Gets the label if the parent node of a given node is a LabeledStatement.
     *
     * @param {ASTNode} node - A node to get.
     * @returns {string|null} The label or `null`.
     */
    getLabel: function(node) {
        if (node.parent.type === "LabeledStatement") {
            return node.parent.label.name;
        }
        return null;
    },

    /**
     * Gets references which are non initializer and writable.
     * @param {Reference[]} references - An array of references.
     * @returns {Reference[]} An array of only references which are non initializer and writable.
     * @public
     */
    getModifyingReferences: function(references) {
        return references.filter(isModifyingReference);
    },

    /**
     * Validate that a string passed in is surrounded by the specified character
     * @param  {string} val The text to check.
     * @param  {string} character The character to see if it's surrounded by.
     * @returns {boolean} True if the text is surrounded by the character, false if not.
     * @private
     */
    isSurroundedBy: function(val, character) {
        return val[0] === character && val[val.length - 1] === character;
    },

    /**
     * Returns whether the provided node is an ESLint directive comment or not
     * @param {LineComment|BlockComment} node The node to be checked
     * @returns {boolean} `true` if the node is an ESLint directive comment
     */
    isDirectiveComment: function(node) {
        var comment = node.value.trim();

        return (
            node.type === "Line" && comment.indexOf("eslint-") === 0 ||
            node.type === "Block" && (
                comment.indexOf("global ") === 0 ||
                comment.indexOf("eslint ") === 0 ||
                comment.indexOf("eslint-") === 0
            )
        );
    },

    /**
     * Gets the trailing statement of a given node.
     *
     *     if (code)
     *         consequent;
     *
     * When taking this `IfStatement`, returns `consequent;` statement.
     *
     * @param {ASTNode} A node to get.
     * @returns {ASTNode|null} The trailing statement's node.
     */
    getTrailingStatement: esutils.ast.trailingStatement,

    /**
     * Finds the variable by a given name in a given scope and its upper scopes.
     *
     * @param {escope.Scope} initScope - A scope to start find.
     * @param {string} name - A variable name to find.
     * @returns {escope.Variable|null} A found variable or `null`.
     */
    getVariableByName: function(initScope, name) {
        var scope = initScope;

        while (scope) {
            var variable = scope.set.get(name);

            if (variable) {
                return variable;
            }

            scope = scope.upper;
        }

        return null;
    },

    /**
     * Checks whether or not a given function node is the default `this` binding.
     *
     * First, this checks the node:
     *
     * - The function name does not start with uppercase (it's a constructor).
     * - The function does not have a JSDoc comment that has a @this tag.
     *
     * Next, this checks the location of the node.
     * If the location is below, this judges `this` is valid.
     *
     * - The location is not on an object literal.
     * - The location does not assign to a property.
     * - The location is not on an ES2015 class.
     * - The location does not call its `bind`/`call`/`apply` method directly.
     * - The function is not a callback of array methods (such as `.forEach()`) if `thisArg` is given.
     *
     * @param {ASTNode} node - A function node to check.
     * @param {SourceCode} sourceCode - A SourceCode instance to get comments.
     * @returns {boolean} The function node is the default `this` binding.
     */
    isDefaultThisBinding: function(node, sourceCode) {
        if (isES5Constructor(node) || hasJSDocThisTag(node, sourceCode)) {
            return false;
        }

        while (node) {
            var parent = node.parent;

            switch (parent.type) {

                /*
                 * Looks up the destination.
                 * e.g., obj.foo = nativeFoo || function foo() { ... };
                 */
                case "LogicalExpression":
                case "ConditionalExpression":
                    node = parent;
                    break;

                // If the upper function is IIFE, checks the destination of the return value.
                // e.g.
                //   obj.foo = (function() {
                //     // setup...
                //     return function foo() { ... };
                //   })();
                case "ReturnStatement":
                    var func = getUpperFunction(parent);

                    if (func === null || !isCallee(func)) {
                        return true;
                    }
                    node = func.parent;
                    break;

                // e.g.
                //   var obj = { foo() { ... } };
                //   var obj = { foo: function() { ... } };
                case "Property":
                    return false;

                // e.g.
                //   obj.foo = foo() { ... };
                case "AssignmentExpression":
                    return (
                        parent.right !== node ||
                        parent.left.type !== "MemberExpression"
                    );

                // e.g.
                //   class A { constructor() { ... } }
                //   class A { foo() { ... } }
                //   class A { get foo() { ... } }
                //   class A { set foo() { ... } }
                //   class A { static foo() { ... } }
                case "MethodDefinition":
                    return false;

                // e.g.
                //   var foo = function foo() { ... }.bind(obj);
                //   (function foo() { ... }).call(obj);
                //   (function foo() { ... }).apply(obj, []);
                case "MemberExpression":
                    return (
                        parent.object !== node ||
                        parent.property.type !== "Identifier" ||
                        !bindOrCallOrApplyPattern.test(parent.property.name) ||
                        !isCallee(parent) ||
                        parent.parent.arguments.length === 0 ||
                        isNullOrUndefined(parent.parent.arguments[0])
                    );

                // e.g.
                //   Reflect.apply(function() {}, obj, []);
                //   Array.from([], function() {}, obj);
                //   list.forEach(function() {}, obj);
                case "CallExpression":
                    if (isReflectApply(parent.callee)) {
                        return (
                            parent.arguments.length !== 3 ||
                            parent.arguments[0] !== node ||
                            isNullOrUndefined(parent.arguments[1])
                        );
                    }
                    if (isArrayFromMethod(parent.callee)) {
                        return (
                            parent.arguments.length !== 3 ||
                            parent.arguments[1] !== node ||
                            isNullOrUndefined(parent.arguments[2])
                        );
                    }
                    if (isMethodWhichHasThisArg(parent.callee)) {
                        return (
                            parent.arguments.length !== 2 ||
                            parent.arguments[0] !== node ||
                            isNullOrUndefined(parent.arguments[1])
                        );
                    }
                    return true;

                // Otherwise `this` is default.
                default:
                    return true;
            }
        }

        /* istanbul ignore next */
        return true;
    }
};
