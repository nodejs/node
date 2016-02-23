/**
 * @fileoverview Rule to control usage of strict mode directives.
 * @author Brandon Mills
 * @copyright 2015 Brandon Mills. All rights reserved.
 * @copyright 2013-2014 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var lodash = require("lodash");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var messages = {
    function: "Use the function form of 'use strict'.",
    global: "Use the global form of 'use strict'.",
    multiple: "Multiple 'use strict' directives.",
    never: "Strict mode is not permitted.",
    unnecessary: "Unnecessary 'use strict' directive.",
    module: "'use strict' is unnecessary inside of modules.",
    implied: "'use strict' is unnecessary when implied strict mode is enabled.",
    unnecessaryInClasses: "'use strict' is unnecessary inside of classes."
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

    var mode = context.options[0] || "safe",
        ecmaFeatures = context.parserOptions.ecmaFeatures || {},
        scopes = [],
        classScopes = [],
        rule;

    if (ecmaFeatures.impliedStrict) {
        mode = "implied";
    } else if (mode === "safe") {
        mode = ecmaFeatures.globalReturn ? "global" : "function";
    }

    /**
     * Report a slice of an array of nodes with a given message.
     * @param {ASTNode[]} nodes Nodes.
     * @param {string} start Index to start from.
     * @param {string} end Index to end before.
     * @param {string} message Message to display.
     * @returns {void}
     */
    function reportSlice(nodes, start, end, message) {
        var i;

        for (i = start; i < end; i++) {
            context.report(nodes[i], message);
        }
    }

    /**
     * Report all nodes in an array with a given message.
     * @param {ASTNode[]} nodes Nodes.
     * @param {string} message Message to display.
     * @returns {void}
     */
    function reportAll(nodes, message) {
        reportSlice(nodes, 0, nodes.length, message);
    }

    /**
     * Report all nodes in an array, except the first, with a given message.
     * @param {ASTNode[]} nodes Nodes.
     * @param {string} message Message to display.
     * @returns {void}
     */
    function reportAllExceptFirst(nodes, message) {
        reportSlice(nodes, 1, nodes.length, message);
    }

    /**
     * Entering a function in 'function' mode pushes a new nested scope onto the
     * stack. The new scope is true if the nested function is strict mode code.
     * @param {ASTNode} node The function declaration or expression.
     * @param {ASTNode[]} useStrictDirectives The Use Strict Directives of the node.
     * @returns {void}
     */
    function enterFunctionInFunctionMode(node, useStrictDirectives) {
        var isInClass = classScopes.length > 0,
            isParentGlobal = scopes.length === 0 && classScopes.length === 0,
            isParentStrict = scopes.length > 0 && scopes[scopes.length - 1],
            isStrict = useStrictDirectives.length > 0;

        if (isStrict) {
            if (isParentStrict) {
                context.report(useStrictDirectives[0], messages.unnecessary);
            } else if (isInClass) {
                context.report(useStrictDirectives[0], messages.unnecessaryInClasses);
            }

            reportAllExceptFirst(useStrictDirectives, messages.multiple);
        } else if (isParentGlobal) {
            context.report(node, messages.function);
        }

        scopes.push(isParentStrict || isStrict);
    }

    /**
     * Exiting a function in 'function' mode pops its scope off the stack.
     * @returns {void}
     */
    function exitFunctionInFunctionMode() {
        scopes.pop();
    }

    /**
     * Enter a function and either:
     * - Push a new nested scope onto the stack (in 'function' mode).
     * - Report all the Use Strict Directives (in the other modes).
     * @param {ASTNode} node The function declaration or expression.
     * @returns {void}
     */
    function enterFunction(node) {
        var isBlock = node.body.type === "BlockStatement",
            useStrictDirectives = isBlock ?
                getUseStrictDirectives(node.body.body) : [];

        if (mode === "function") {
            enterFunctionInFunctionMode(node, useStrictDirectives);
        } else {
            reportAll(useStrictDirectives, messages[mode]);
        }
    }

    rule = {
        "Program": function(node) {
            var useStrictDirectives = getUseStrictDirectives(node.body);

            if (node.sourceType === "module") {
                mode = "module";
            }

            if (mode === "global") {
                if (node.body.length > 0 && useStrictDirectives.length === 0) {
                    context.report(node, messages.global);
                }
                reportAllExceptFirst(useStrictDirectives, messages.multiple);
            } else {
                reportAll(useStrictDirectives, messages[mode]);
            }
        },
        "FunctionDeclaration": enterFunction,
        "FunctionExpression": enterFunction,
        "ArrowFunctionExpression": enterFunction
    };

    if (mode === "function") {
        lodash.assign(rule, {
            // Inside of class bodies are always strict mode.
            "ClassBody": function() {
                classScopes.push(true);
            },
            "ClassBody:exit": function() {
                classScopes.pop();
            },

            "FunctionDeclaration:exit": exitFunctionInFunctionMode,
            "FunctionExpression:exit": exitFunctionInFunctionMode,
            "ArrowFunctionExpression:exit": exitFunctionInFunctionMode
        });
    }

    return rule;
};

module.exports.schema = [
    {
        "enum": ["never", "global", "function", "safe"]
    }
];
