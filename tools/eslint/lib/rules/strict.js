/**
 * @fileoverview Rule to control usage of strict mode directives.
 * @author Brandon Mills
 * @copyright 2015 Brandon Mills. All rights reserved.
 * @copyright 2013-2014 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var messages = {
    function: "Use the function form of \"use strict\".",
    global: "Use the global form of \"use strict\".",
    multiple: "Multiple \"use strict\" directives.",
    never: "Strict mode is not permitted.",
    unnecessary: "Unnecessary \"use strict\" directive.",
    unnecessaryInModules: "\"use strict\" is unnecessary inside of modules.",
    unnecessaryInClasses: "\"use strict\" is unnecessary inside of classes."
};

/**
 * Gets all of the Use Strict Directives in the Directive Prologue of a group of
 * statements.
 * @param {ASTNode[]} statements Statements in the program or function body.
 * @returns {ASTNode[]} All of the Use Strict Directives.
 */
function getUseStrictDirectives(statements) {
    var directives = [],
        i, statement;

    for (i = 0; i < statements.length; i++) {
        statement = statements[i];

        if (
            statement.type === "ExpressionStatement" &&
            statement.expression.type === "Literal" &&
            statement.expression.value === "use strict"
        ) {
            directives[i] = statement;
        } else {
            break;
        }
    }

    return directives;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var mode = context.options[0];

    /**
     * Report a node or array of nodes with a given message.
     * @param {(ASTNode|ASTNode[])} nodes Node or nodes to report.
     * @param {string} message Message to display.
     * @returns {void}
     */
    function report(nodes, message) {
        var i;

        if (Array.isArray(nodes)) {
            for (i = 0; i < nodes.length; i++) {
                context.report(nodes[i], message);
            }
        } else {
            context.report(nodes, message);
        }
    }

    //--------------------------------------------------------------------------
    // "never" mode
    //--------------------------------------------------------------------------

    if (mode === "never") {
        return {
            "Program": function(node) {
                report(getUseStrictDirectives(node.body), messages.never);
            },
            "FunctionDeclaration": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.never);
            },
            "FunctionExpression": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.never);
            },
            "ArrowFunctionExpression": function(node) {
                if (node.body.type === "BlockStatement") {
                    report(getUseStrictDirectives(node.body.body), messages.never);
                }
            }
        };
    }

    //--------------------------------------------------------------------------
    // If this is modules, all "use strict" directives are unnecessary.
    //--------------------------------------------------------------------------

    if (context.ecmaFeatures.modules) {
        return {
            "Program": function(node) {
                report(getUseStrictDirectives(node.body), messages.unnecessaryInModules);
            },
            "FunctionDeclaration": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.unnecessaryInModules);
            },
            "FunctionExpression": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.unnecessaryInModules);
            },
            "ArrowFunctionExpression": function(node) {
                if (node.body.type === "BlockStatement") {
                    report(getUseStrictDirectives(node.body.body), messages.unnecessaryInModules);
                }
            }
        };
    }

    //--------------------------------------------------------------------------
    // "global" mode
    //--------------------------------------------------------------------------

    if (mode === "global") {
        return {
            "Program": function(node) {
                var useStrictDirectives = getUseStrictDirectives(node.body);

                if (node.body.length > 0 && useStrictDirectives.length === 0) {
                    report(node, messages.global);
                } else {
                    report(useStrictDirectives.slice(1), messages.multiple);
                }
            },
            "FunctionDeclaration": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.global);
            },
            "FunctionExpression": function(node) {
                report(getUseStrictDirectives(node.body.body), messages.global);
            },
            "ArrowFunctionExpression": function(node) {
                if (node.body.type === "BlockStatement") {
                    report(getUseStrictDirectives(node.body.body), messages.global);
                }
            }
        };
    }

    //--------------------------------------------------------------------------
    // "function" mode (Default)
    //--------------------------------------------------------------------------

    var scopes = [],
        classScopes = [];

    /**
     * Entering a function pushes a new nested scope onto the stack. The new
     * scope is true if the nested function is strict mode code.
     * @param {ASTNode} node The function declaration or expression.
     * @returns {void}
     */
    function enterFunction(node) {
        var isInClass = classScopes.length > 0,
            isParentGlobal = scopes.length === 0 && classScopes.length === 0,
            isParentStrict = scopes.length > 0 && scopes[scopes.length - 1],
            isNotBlock = node.body.type !== "BlockStatement",
            useStrictDirectives = isNotBlock ? [] : getUseStrictDirectives(node.body.body),
            isStrict = useStrictDirectives.length > 0;

        if (isStrict) {
            if (isParentStrict) {
                report(useStrictDirectives[0], messages.unnecessary);
            } else if (isInClass) {
                report(useStrictDirectives[0], messages.unnecessaryInClasses);
            }

            report(useStrictDirectives.slice(1), messages.multiple);
        } else if (isParentGlobal) {
            report(node, messages.function);
        }

        scopes.push(isParentStrict || isStrict);
    }

    /**
     * Exiting a function pops its scope off the stack.
     * @returns {void}
     */
    function exitFunction() {
        scopes.pop();
    }

    return {
        "Program": function(node) {
            report(getUseStrictDirectives(node.body), messages.function);
        },

        // Inside of class bodies are always strict mode.
        "ClassBody": function() {
            classScopes.push(true);
        },
        "ClassBody:exit": function() {
            classScopes.pop();
        },

        "FunctionDeclaration": enterFunction,
        "FunctionExpression": enterFunction,
        "ArrowFunctionExpression": enterFunction,

        "FunctionDeclaration:exit": exitFunction,
        "FunctionExpression:exit": exitFunction,
        "ArrowFunctionExpression:exit": exitFunction
    };
};

module.exports.schema = [
    {
        "enum": ["never", "global", "function"]
    }
];
