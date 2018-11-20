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
    unnecessary: "Unnecessary \"use strict\" directive."
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

    var mode = context.options[0],
        isModule = context.ecmaFeatures.modules,
        modes = {},
        scopes = [];

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
    // "deprecated" mode (default)
    //--------------------------------------------------------------------------

    /**
     * Determines if a given node is "use strict".
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node is a strict pragma, false if not.
     * @void
     */
    function isStrictPragma(node) {
        return (node && node.type === "ExpressionStatement" &&
                node.expression.value === "use strict");
    }

    /**
     * When you enter a scope, push the strict value from the previous scope
     * onto the stack.
     * @param {ASTNode} node The AST node being checked.
     * @returns {void}
     * @private
     */
    function enterScope(node) {

        var isStrict = false,
            isProgram = (node.type === "Program"),
            isParentGlobal = scopes.length === 1,
            isParentStrict = scopes.length ? scopes[scopes.length - 1] : false;

        // look for the "use strict" pragma
        if (isModule) {
            isStrict = true;
        } else if (isProgram) {
            isStrict = isStrictPragma(node.body[0]) || isParentStrict;
        } else {
            isStrict = node.body.body && isStrictPragma(node.body.body[0]) || isParentStrict;
        }

        scopes.push(isStrict);

        // never warn if the parent is strict or the function is strict
        if (!isParentStrict && !isStrict && isParentGlobal) {
            context.report(node, "Missing \"use strict\" statement.");
        }
    }

    /**
     * When you exit a scope, pop off the top scope and see if it's true or
     * false.
     * @returns {void}
     * @private
     */
    function exitScope() {
        scopes.pop();
    }

    modes.deprecated = {
        "Program": enterScope,
        "FunctionDeclaration": enterScope,
        "FunctionExpression": enterScope,
        "ArrowFunctionExpression": enterScope,

        "Program:exit": exitScope,
        "FunctionDeclaration:exit": exitScope,
        "FunctionExpression:exit": exitScope,
        "ArrowFunctionExpression:exit": exitScope
    };

    //--------------------------------------------------------------------------
    // "never" mode
    //--------------------------------------------------------------------------

    modes.never = {
        "Program": function(node) {
            report(getUseStrictDirectives(node.body), messages.never);
        },
        "FunctionDeclaration": function(node) {
            report(getUseStrictDirectives(node.body.body), messages.never);
        },
        "FunctionExpression": function(node) {
            report(getUseStrictDirectives(node.body.body), messages.never);
        }
    };

    //--------------------------------------------------------------------------
    // "global" mode
    //--------------------------------------------------------------------------

    modes.global = {
        "Program": function(node) {
            var useStrictDirectives = getUseStrictDirectives(node.body);

            if (!isModule && node.body.length && useStrictDirectives.length < 1) {
                report(node, messages.global);
            } else if (isModule) {
                report(useStrictDirectives, messages.unnecessary);
            } else {
                report(useStrictDirectives.slice(1), messages.multiple);
            }
        },
        "FunctionDeclaration": function(node) {
            report(getUseStrictDirectives(node.body.body), messages.global);
        },
        "FunctionExpression": function(node) {
            report(getUseStrictDirectives(node.body.body), messages.global);
        }
    };

    //--------------------------------------------------------------------------
    // "function" mode
    //--------------------------------------------------------------------------

    /**
     * Entering a function pushes a new nested scope onto the stack. The new
     * scope is true if the nested function is strict mode code.
     * @param {ASTNode} node The function declaration or expression.
     * @returns {void}
     */
    function enterFunction(node) {
        var useStrictDirectives = getUseStrictDirectives(node.body.body),
            isParentGlobal = scopes.length === 0,
            isParentStrict = isModule || (scopes.length && scopes[scopes.length - 1]),
            isStrict = useStrictDirectives.length > 0 || isModule;

        if (isStrict) {
            if (isParentStrict && useStrictDirectives.length) {
                report(useStrictDirectives[0], messages.unnecessary);
            }

            report(useStrictDirectives.slice(1), messages.multiple);
        } else if (isParentGlobal && !isModule) {
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

    modes.function = {
        "Program": function(node) {
            report(getUseStrictDirectives(node.body), messages.function);
        },
        "FunctionDeclaration": enterFunction,
        "FunctionExpression": enterFunction,
        "FunctionDeclaration:exit": exitFunction,
        "FunctionExpression:exit": exitFunction
    };

    return modes[mode || "deprecated"];

};
