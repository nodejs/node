/**
 * @fileoverview Rule to flag use of variables before they are defined
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var SENTINEL_TYPE = /^(?:(?:Function|Class)(?:Declaration|Expression)|ArrowFunctionExpression|CatchClause|ImportDeclaration|ExportNamedDeclaration)$/;

/**
 * Parses a given value as options.
 *
 * @param {any} options - A value to parse.
 * @returns {object} The parsed options.
 */
function parseOptions(options) {
    var functions = true;
    var classes = true;

    if (typeof options === "string") {
        functions = (options !== "nofunc");
    } else if (typeof options === "object" && options !== null) {
        functions = options.functions !== false;
        classes = options.classes !== false;
    }

    return {functions: functions, classes: classes};
}

/**
 * @returns {boolean} `false`.
 */
function alwaysFalse() {
    return false;
}

/**
 * Checks whether or not a given variable is a function declaration.
 *
 * @param {escope.Variable} variable - A variable to check.
 * @returns {boolean} `true` if the variable is a function declaration.
 */
function isFunction(variable) {
    return variable.defs[0].type === "FunctionName";
}

/**
 * Checks whether or not a given variable is a class declaration in an upper function scope.
 *
 * @param {escope.Variable} variable - A variable to check.
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the variable is a class declaration.
 */
function isOuterClass(variable, reference) {
    return (
        variable.defs[0].type === "ClassName" &&
        variable.scope.variableScope !== reference.from.variableScope
    );
}

/**
 * Checks whether or not a given variable is a function declaration or a class declaration in an upper function scope.
 *
 * @param {escope.Variable} variable - A variable to check.
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the variable is a function declaration or a class declaration.
 */
function isFunctionOrOuterClass(variable, reference) {
    return isFunction(variable, reference) || isOuterClass(variable, reference);
}

/**
 * Checks whether or not a given location is inside of the range of a given node.
 *
 * @param {ASTNode} node - An node to check.
 * @param {number} location - A location to check.
 * @returns {boolean} `true` if the location is inside of the range of the node.
 */
function isInRange(node, location) {
    return node && node.range[0] <= location && location <= node.range[1];
}

/**
 * Checks whether or not a given reference is inside of the initializers of a given variable.
 *
 * @param {Variable} variable - A variable to check.
 * @param {Reference} reference - A reference to check.
 * @returns {boolean} `true` if the reference is inside of the initializers.
 */
function isInInitializer(variable, reference) {
    if (variable.scope !== reference.from) {
        return false;
    }

    var node = variable.identifiers[0].parent;
    var location = reference.identifier.range[1];

    while (node) {
        if (node.type === "VariableDeclarator") {
            if (isInRange(node.init, location)) {
                return true;
            }
            break;
        } else if (node.type === "AssignmentPattern") {
            if (isInRange(node.right, location)) {
                return true;
            }
        } else if (SENTINEL_TYPE.test(node.type)) {
            break;
        }

        node = node.parent;
    }

    return false;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of variables before they are defined",
            category: "Variables",
            recommended: false
        },

        schema: [
            {
                oneOf: [
                    {
                        enum: ["nofunc"]
                    },
                    {
                        type: "object",
                        properties: {
                            functions: {type: "boolean"},
                            classes: {type: "boolean"}
                        },
                        additionalProperties: false
                    }
                ]
            }
        ]
    },

    create: function(context) {
        var options = parseOptions(context.options[0]);

        // Defines a function which checks whether or not a reference is allowed according to the option.
        var isAllowed;

        if (options.functions && options.classes) {
            isAllowed = alwaysFalse;
        } else if (options.functions) {
            isAllowed = isOuterClass;
        } else if (options.classes) {
            isAllowed = isFunction;
        } else {
            isAllowed = isFunctionOrOuterClass;
        }

        /**
         * Finds and validates all variables in a given scope.
         * @param {Scope} scope The scope object.
         * @returns {void}
         * @private
         */
        function findVariablesInScope(scope) {
            scope.references.forEach(function(reference) {
                var variable = reference.resolved;

                // Skips when the reference is:
                // - initialization's.
                // - referring to an undefined variable.
                // - referring to a global environment variable (there're no identifiers).
                // - located preceded by the variable (except in initializers).
                // - allowed by options.
                if (reference.init ||
                    !variable ||
                    variable.identifiers.length === 0 ||
                    (variable.identifiers[0].range[1] < reference.identifier.range[1] && !isInInitializer(variable, reference)) ||
                    isAllowed(variable, reference)
                ) {
                    return;
                }

                // Reports.
                context.report({
                    node: reference.identifier,
                    message: "'{{name}}' was used before it was defined",
                    data: reference.identifier
                });
            });
        }

        /**
         * Validates variables inside of a node's scope.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         * @private
         */
        function findVariables() {
            var scope = context.getScope();

            findVariablesInScope(scope);
        }

        var ruleDefinition = {
            "Program:exit": function(node) {
                var scope = context.getScope(),
                    ecmaFeatures = context.parserOptions.ecmaFeatures || {};

                findVariablesInScope(scope);

                // both Node.js and Modules have an extra scope
                if (ecmaFeatures.globalReturn || node.sourceType === "module") {
                    findVariablesInScope(scope.childScopes[0]);
                }
            }
        };

        if (context.parserOptions.ecmaVersion >= 6) {
            ruleDefinition["BlockStatement:exit"] =
                ruleDefinition["SwitchStatement:exit"] = findVariables;

            ruleDefinition["ArrowFunctionExpression:exit"] = function(node) {
                if (node.body.type !== "BlockStatement") {
                    findVariables(node);
                }
            };
        } else {
            ruleDefinition["FunctionExpression:exit"] =
                ruleDefinition["FunctionDeclaration:exit"] =
                ruleDefinition["ArrowFunctionExpression:exit"] = findVariables;
        }

        return ruleDefinition;
    }
};
