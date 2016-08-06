/**
 * @fileoverview Rule to control usage of strict mode directives.
 * @author Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

let lodash = require("lodash");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

let messages = {
    function: "Use the function form of 'use strict'.",
    global: "Use the global form of 'use strict'.",
    multiple: "Multiple 'use strict' directives.",
    never: "Strict mode is not permitted.",
    unnecessary: "Unnecessary 'use strict' directive.",
    module: "'use strict' is unnecessary inside of modules.",
    implied: "'use strict' is unnecessary when implied strict mode is enabled.",
    unnecessaryInClasses: "'use strict' is unnecessary inside of classes.",
    nonSimpleParameterList: "'use strict' directive inside a function with non-simple parameter list throws a syntax error since ES2016.",
    wrap: "Wrap this function in a function with 'use strict' directive."
};

/**
 * Gets all of the Use Strict Directives in the Directive Prologue of a group of
 * statements.
 * @param {ASTNode[]} statements Statements in the program or function body.
 * @returns {ASTNode[]} All of the Use Strict Directives.
 */
function getUseStrictDirectives(statements) {
    let directives = [],
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

/**
 * Checks whether a given parameter is a simple parameter.
 *
 * @param {ASTNode} node - A pattern node to check.
 * @returns {boolean} `true` if the node is an Identifier node.
 */
function isSimpleParameter(node) {
    return node.type === "Identifier";
}

/**
 * Checks whether a given parameter list is a simple parameter list.
 *
 * @param {ASTNode[]} params - A parameter list to check.
 * @returns {boolean} `true` if the every parameter is an Identifier node.
 */
function isSimpleParameterList(params) {
    return params.every(isSimpleParameter);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require or disallow strict mode directives",
            category: "Strict Mode",
            recommended: false
        },

        schema: [
            {
                enum: ["never", "global", "function", "safe"]
            }
        ]
    },

    create: function(context) {

        let mode = context.options[0] || "safe",
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
            let i;

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
            let isInClass = classScopes.length > 0,
                isParentGlobal = scopes.length === 0 && classScopes.length === 0,
                isParentStrict = scopes.length > 0 && scopes[scopes.length - 1],
                isStrict = useStrictDirectives.length > 0;

            if (isStrict) {
                if (!isSimpleParameterList(node.params)) {
                    context.report(useStrictDirectives[0], messages.nonSimpleParameterList);
                } else if (isParentStrict) {
                    context.report(useStrictDirectives[0], messages.unnecessary);
                } else if (isInClass) {
                    context.report(useStrictDirectives[0], messages.unnecessaryInClasses);
                }

                reportAllExceptFirst(useStrictDirectives, messages.multiple);
            } else if (isParentGlobal) {
                if (isSimpleParameterList(node.params)) {
                    context.report(node, messages.function);
                } else {
                    context.report(node, messages.wrap);
                }
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
            let isBlock = node.body.type === "BlockStatement",
                useStrictDirectives = isBlock ?
                    getUseStrictDirectives(node.body.body) : [];

            if (mode === "function") {
                enterFunctionInFunctionMode(node, useStrictDirectives);
            } else if (useStrictDirectives.length > 0) {
                if (isSimpleParameterList(node.params)) {
                    reportAll(useStrictDirectives, messages[mode]);
                } else {
                    context.report(useStrictDirectives[0], messages.nonSimpleParameterList);
                    reportAllExceptFirst(useStrictDirectives, messages.multiple);
                }
            }
        }

        rule = {
            Program: function(node) {
                let useStrictDirectives = getUseStrictDirectives(node.body);

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
            FunctionDeclaration: enterFunction,
            FunctionExpression: enterFunction,
            ArrowFunctionExpression: enterFunction
        };

        if (mode === "function") {
            lodash.assign(rule, {

                // Inside of class bodies are always strict mode.
                ClassBody: function() {
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
    }
};
