/**
 * @fileoverview Main ESLint object.
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var estraverse = require("estraverse-fb"),
    escope = require("escope"),
    environments = require("../conf/environments"),
    assign = require("object-assign"),
    rules = require("./rules"),
    util = require("./util"),
    RuleContext = require("./rule-context"),
    timing = require("./timing"),
    createTokenStore = require("./token-store.js"),
    EventEmitter = require("events").EventEmitter,
    escapeRegExp = require("escape-string-regexp");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

// TODO: Remove when estraverse is updated
estraverse.Syntax.Super = "Super";
estraverse.VisitorKeys.Super = [];

/**
 * Parses a list of "name:boolean_value" or/and "name" options divided by comma or
 * whitespace.
 * @param {string} string The string to parse.
 * @returns {Object} Result map object of names and boolean values
 */
function parseBooleanConfig(string) {
    var items = {};
    // Collapse whitespace around : to make parsing easier
    string = string.replace(/\s*:\s*/g, ":");
    // Collapse whitespace around ,
    string = string.replace(/\s*,\s*/g, ",");
    string.split(/\s|,+/).forEach(function(name) {
        if (!name) {
            return;
        }
        var pos = name.indexOf(":"),
            value;
        if (pos !== -1) {
            value = name.substring(pos + 1, name.length);
            name = name.substring(0, pos);
        }

        items[name] = (value === "true");

    });
    return items;
}

/**
 * Parses a JSON-like config.
 * @param {string} string The string to parse.
 * @param {Object} location Start line and column of comments for potential error message.
 * @param {Object[]} messages The messages queue for potential error message.
 * @returns {Object} Result map object
 */
function parseJsonConfig(string, location, messages) {
    var items = {};
    string = string.replace(/([a-zA-Z0-9\-\/]+):/g, "\"$1\":").replace(/(\]|[0-9])\s+(?=")/, "$1,");
    try {
        items = JSON.parse("{" + string + "}");
    } catch(ex) {

        messages.push({
            fatal: true,
            severity: 2,
            message: "Failed to parse JSON from '" + string + "': " + ex.message,
            line: location.start.line,
            column: location.start.column
        });

    }

    return items;
}

/**
 * Parses a config of values separated by comma.
 * @param {string} string The string to parse.
 * @returns {Object} Result map of values and true values
 */
function parseListConfig(string) {
    var items = {};
    // Collapse whitespace around ,
    string = string.replace(/\s*,\s*/g, ",");
    string.split(/,+/).forEach(function(name) {
        name = name.trim();
        if (!name) {
            return;
        }
        items[name] = true;
    });
    return items;
}

/**
 * @param {Scope} scope The scope object to check.
 * @param {string} name The name of the variable to look up.
 * @returns {Variable} The variable object if found or null if not.
 */
function getVariable(scope, name) {
    var variable = null;
    scope.variables.some(function(v) {
        if (v.name === name) {
            variable = v;
            return true;
        } else {
            return false;
        }

    });
    return variable;
}

/**
 * Ensures that variables representing built-in properties of the Global Object,
 * and any globals declared by special block comments, are present in the global
 * scope.
 * @param {ASTNode} program The top node of the AST.
 * @param {Scope} globalScope The global scope.
 * @param {Object} config The existing configuration data.
 * @returns {void}
 */
function addDeclaredGlobals(program, globalScope, config) {
    var declaredGlobals = {},
        explicitGlobals = {},
        builtin = environments.builtin;

    assign(declaredGlobals, builtin);

    Object.keys(config.env).forEach(function (name) {
        if (config.env[name]) {
            var environmentGlobals = environments[name] && environments[name].globals;
            if (environmentGlobals) {
                assign(declaredGlobals, environmentGlobals);
            }
        }
    });

    assign(declaredGlobals, config.globals);
    assign(explicitGlobals, config.astGlobals);

    Object.keys(declaredGlobals).forEach(function(name) {
        var variable = getVariable(globalScope, name);
        if (!variable) {
            variable = new escope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = false;
            globalScope.variables.push(variable);
        }
        variable.writeable = declaredGlobals[name];
    });

    Object.keys(explicitGlobals).forEach(function(name) {
        var variable = getVariable(globalScope, name);
        if (!variable) {
            variable = new escope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = true;
            globalScope.variables.push(variable);
        }
        variable.writeable = explicitGlobals[name];
    });
}

/**
 * Add data to reporting configuration to disable reporting for list of rules
 * starting from start location
 * @param  {Object[]} reportingConfig Current reporting configuration
 * @param  {Object} start Position to start
 * @param  {string[]} rulesToDisable List of rules
 * @returns {void}
 */
function disableReporting(reportingConfig, start, rulesToDisable) {

    if (rulesToDisable.length) {
        rulesToDisable.forEach(function(rule) {
            reportingConfig.push({
                start: start,
                end: null,
                rule: rule
            });
        });
    } else {
        reportingConfig.push({
            start: start,
            end: null,
            rule: null
        });
    }
}

/**
 * Add data to reporting configuration to enable reporting for list of rules
 * starting from start location
 * @param  {Object[]} reportingConfig Current reporting configuration
 * @param  {Object} start Position to start
 * @param  {string[]} rulesToEnable List of rules
 * @returns {void}
 */
function enableReporting(reportingConfig, start, rulesToEnable) {
    var i;

    if (rulesToEnable.length) {
        rulesToEnable.forEach(function(rule) {
            for (i = reportingConfig.length - 1; i >= 0; i--) {
                if (!reportingConfig[i].end && reportingConfig[i].rule === rule ) {
                    reportingConfig[i].end = start;
                    break;
                }
            }
        });
    } else {
        // find all previous disabled locations if they was started as list of rules
        var prevStart;
        for (i = reportingConfig.length - 1; i >= 0; i--) {
            if (prevStart && prevStart !== reportingConfig[i].start) {
                break;
            }

            if (!reportingConfig[i].end) {
                reportingConfig[i].end = start;
                prevStart = reportingConfig[i].start;
            }
        }
    }
}

/**
 * Parses comments in file to extract file-specific config of rules, globals
 * and environments and merges them with global config; also code blocks
 * where reporting is disabled or enabled and merges them with reporting config.
 * @param {ASTNode} ast The top node of the AST.
 * @param {Object} config The existing configuration data.
 * @param {Object[]} reportingConfig The existing reporting configuration data.
 * @param {Object[]} messages The messages queue.
 * @returns {void}
 */
function modifyConfigsFromComments(ast, config, reportingConfig, messages) {

    var commentConfig = {
        astGlobals: {},
        rules: {},
        env: {}
    };
    var commentRules = {};

    ast.comments.forEach(function(comment) {

        var value = comment.value.trim();
        var match = /^(eslint-\w+|eslint-\w+-\w+|eslint|globals?)(\s|$)/.exec(value);

        if (match) {
            value = value.substring(match.index + match[1].length);

            if (comment.type === "Block") {
                switch (match[1]) {
                    case "globals":
                    case "global":
                        assign(commentConfig.astGlobals, parseBooleanConfig(value));
                        break;

                    case "eslint-env":
                        assign(commentConfig.env, parseListConfig(value));
                        break;

                    case "eslint-disable":
                        disableReporting(reportingConfig, comment.loc.start, Object.keys(parseListConfig(value)));
                        break;

                    case "eslint-enable":
                        enableReporting(reportingConfig, comment.loc.start, Object.keys(parseListConfig(value)));
                        break;

                    case "eslint":
                        var items = parseJsonConfig(value, comment.loc, messages);
                        Object.keys(items).forEach(function(name) {
                            var ruleValue = items[name];
                            if (typeof ruleValue === "number" || (Array.isArray(ruleValue) && typeof ruleValue[0] === "number")) {
                                commentRules[name] = ruleValue;
                            }
                        });
                        break;

                    // no default
                }
            } else {
                // comment.type === "Line"
                if (match[1] === "eslint-disable-line") {
                    disableReporting(reportingConfig, { "line": comment.loc.start.line, "column": 0 }, Object.keys(parseListConfig(value)));
                    enableReporting(reportingConfig, comment.loc.end, Object.keys(parseListConfig(value)));
                }
            }
        }
    });

    // apply environment rules before user rules
    Object.keys(commentConfig.env).forEach(function (name) {
        var environmentRules = environments[name] && environments[name].rules;
        if (commentConfig.env[name] && environmentRules) {
            assign(commentConfig.rules, environmentRules);
        }
    });
    assign(commentConfig.rules, commentRules);

    util.mergeConfigs(config, commentConfig);
}

/**
 * Check if message of rule with ruleId should be ignored in location
 * @param  {Object[]} reportingConfig  Collection of ignore records
 * @param  {string} ruleId   Id of rule
 * @param  {Object} location Location of message
 * @returns {boolean}          True if message should be ignored, false otherwise
 */
function isDisabledByReportingConfig(reportingConfig, ruleId, location) {

    for (var i = 0, c = reportingConfig.length; i < c; i++) {

        var ignore = reportingConfig[i];
        if ((!ignore.rule || ignore.rule === ruleId) &&
            (location.line > ignore.start.line || (location.line === ignore.start.line && location.column >= ignore.start.column)) &&
            (!ignore.end || (location.line < ignore.end.line || (location.line === ignore.end.line && location.column <= ignore.end.column)))) {
            return true;
        }
    }

    return false;
}

/**
 * Process initial config to make it safe to extend by file comment config
 * @param  {Object} config Initial config
 * @returns {Object}        Processed config
 */
function prepareConfig(config) {

    config.globals = config.globals || config.global || {};
    delete config.global;

    var copiedRules = {},
        ecmaFeatures = {},
        preparedConfig;

    if (typeof config.rules === "object") {
        Object.keys(config.rules).forEach(function(k) {
            var rule = config.rules[k];
            if (rule === null) {
                throw new Error("Invalid config for rule '" + k + "'\.");
            }
            if (Array.isArray(rule)) {
                copiedRules[k] = rule.slice();
            } else {
                copiedRules[k] = rule;
            }
        });
    }

    // merge in environment ecmaFeatures
    if (typeof config.env === "object") {
        Object.keys(config.env).forEach(function(env) {
            if (config.env[env] && environments[env].ecmaFeatures) {
                assign(ecmaFeatures, environments[env].ecmaFeatures);
            }
        });
    }

    preparedConfig = {
        rules: copiedRules,
        parser: config.parser || "espree",
        globals: util.mergeConfigs({}, config.globals),
        env: util.mergeConfigs({}, config.env || {}),
        settings: util.mergeConfigs({}, config.settings || {}),
        ecmaFeatures: util.mergeConfigs(ecmaFeatures, config.ecmaFeatures || {})
    };

    // can't have global return inside of modules
    if (preparedConfig.ecmaFeatures.modules) {
        preparedConfig.ecmaFeatures.globalReturn = false;
    }

    return preparedConfig;
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Object that is responsible for verifying JavaScript text
 * @name eslint
 */
module.exports = (function() {

    var api = Object.create(new EventEmitter()),
        messages = [],
        currentText = null,
        currentTextLines = [],
        currentConfig = null,
        currentTokens = null,
        currentScopes = null,
        scopeMap = null,
        scopeManager = null,
        currentFilename = null,
        controller = null,
        reportingConfig = [],
        commentLocsEnter = [],
        commentLocsExit = [],
        currentAST = null;

    /**
     * Parses text into an AST. Moved out here because the try-catch prevents
     * optimization of functions, so it's best to keep the try-catch as isolated
     * as possible
     * @param {string} text The text to parse.
     * @param {Object} config The ESLint configuration object.
     * @returns {ASTNode} The AST if successful or null if not.
     * @private
     */
    function parse(text, config) {

        var parser;

        try {
            parser = require(config.parser);
        } catch (ex) {
            messages.push({
                fatal: true,
                severity: 2,
                message: ex.message,
                line: 0,
                column: 0
            });

            return null;
        }

        /*
         * Check for parsing errors first. If there's a parsing error, nothing
         * else can happen. However, a parsing error does not throw an error
         * from this method - it's just considered a fatal error message, a
         * problem that ESLint identified just like any other.
         */
        try {
            return parser.parse(text, {
                loc: true,
                range: true,
                raw: true,
                tokens: true,
                comment: true,
                attachComment: true,
                ecmaFeatures: config.ecmaFeatures
            });
        } catch (ex) {

            messages.push({
                fatal: true,
                severity: 2,

                // messages come as "Line X: Unexpected token foo", so strip off leading part
                message: ex.message.substring(ex.message.indexOf(":") + 1).trim(),

                line: ex.lineNumber,
                column: ex.column
            });

            return null;
        }
    }

    /**
     * Check collection of comments to prevent double event for comment as
     * leading and trailing, then emit event if passing
     * @param {ASTNode[]} comments Collection of comment nodes
     * @param {Object[]} locs List of locations of previous comment nodes
     * @param {string} eventName Event name postfix
     * @returns {void}
     */
    function emitComments(comments, locs, eventName) {

        if (comments.length) {
            comments.forEach(function(node) {
                if (locs.indexOf(node.loc) >= 0) {
                    locs.splice(locs.indexOf(node.loc), 1);
                } else {
                    locs.push(node.loc);
                    api.emit(node.type + eventName, node);
                }
            });
        }
    }

    /**
     * Shortcut to check and emit enter of comment nodes
     * @param {ASTNode[]} comments Collection of comment nodes
     * @returns {void}
     */
    function emitCommentsEnter(comments) {
        emitComments(comments, commentLocsEnter, "Comment");
    }

    /**
     * Shortcut to check and emit exit of comment nodes
     * @param {ASTNode[]} comments Collection of comment nodes
     * @returns {void}
     */
    function emitCommentsExit(comments) {
        emitComments(comments, commentLocsExit, "Comment:exit");
    }

    /**
     * Get the severity level of a rule (0 - none, 1 - warning, 2 - error)
     * Returns 0 if the rule config is not valid (an Array or a number)
     * @param {Array|number} ruleConfig rule configuration
     * @returns {number} 0, 1, or 2, indicating rule severity
     */
    function getRuleSeverity(ruleConfig) {
        if (typeof ruleConfig === "number") {
            return ruleConfig;
        } else if (Array.isArray(ruleConfig)) {
            return ruleConfig[0];
        } else {
            return 0;
        }
    }

    /**
     * Get the options for a rule (not including severity), if any
     * @param {Array|number} ruleConfig rule configuration
     * @returns {Array} of rule options, empty Array if none
     */
    function getRuleOptions(ruleConfig) {
        if (Array.isArray(ruleConfig)) {
            return ruleConfig.slice(1);
        } else {
            return [];
        }
    }

    // set unlimited listeners (see https://github.com/eslint/eslint/issues/524)
    api.setMaxListeners(0);

    /**
     * Resets the internal state of the object.
     * @returns {void}
     */
    api.reset = function() {
        this.removeAllListeners();
        messages = [];
        currentAST = null;
        currentConfig = null;
        currentText = null;
        currentTextLines = [];
        currentTokens = null;
        currentScopes = null;
        scopeMap = null;
        scopeManager = null;
        controller = null;
        reportingConfig = [];
        commentLocsEnter = [];
        commentLocsExit = [];
    };

    /**
     * Verifies the text against the rules specified by the second argument.
     * @param {string} text The JavaScript text to verify.
     * @param {Object} config An object whose keys specify the rules to use.
     * @param {string=} filename The optional filename of the file being checked.
     *      If this is not set, the filename will default to '<input>' in the rule context.
     * @param {boolean=} saveState Indicates if the state from the last run should be saved.
     *      Mostly useful for testing purposes.
     * @returns {Object[]} The results as an array of messages or null if no messages.
     */
    api.verify = function(text, config, filename, saveState) {

        var ast,
            shebang,
            ecmaFeatures,
            ecmaVersion;

        // set the current parsed filename
        currentFilename = filename;

        if (!saveState) {
            this.reset();
        }

        // there's no input, just exit here
        if (text.trim().length === 0) {
            currentText = text;
            return messages;
        }

        // process initial config to make it safe to extend
        config = prepareConfig(config || {});

        ast = parse(text.replace(/^#!([^\r\n]+)/, function(match, captured) {
            shebang = captured;
            return "//" + captured;
        }), config);

        // if espree failed to parse the file, there's no sense in setting up rules
        if (ast) {

            currentAST = ast;

            // parse global comments and modify config
            modifyConfigsFromComments(ast, config, reportingConfig, messages);

            // enable appropriate rules
            Object.keys(config.rules).filter(function(key) {
                return getRuleSeverity(config.rules[key]) > 0;
            }).forEach(function(key) {

                var ruleCreator = rules.get(key),
                    severity = getRuleSeverity(config.rules[key]),
                    options = getRuleOptions(config.rules[key]),
                    rule;

                if (ruleCreator) {
                    try {
                        rule = ruleCreator(new RuleContext(
                            key, api, severity, options,
                            config.settings, config.ecmaFeatures
                        ));

                        // add all the node types as listeners
                        Object.keys(rule).forEach(function(nodeType) {
                            api.on(nodeType, timing.enabled
                                ? timing.time(key, rule[nodeType])
                                : rule[nodeType]
                            );
                        });
                    } catch(ex) {
                        ex.message = "Error while loading rule '" + key + "': " + ex.message;
                        throw ex;
                    }

                } else {
                    throw new Error("Definition for rule '" + key + "' was not found.");
                }
            });

            // save config so rules can access as necessary
            currentConfig = config;
            currentText = text;
            controller = new estraverse.Controller();

            ecmaFeatures = currentConfig.ecmaFeatures;
            ecmaVersion = (ecmaFeatures.blockBindings || ecmaFeatures.classes ||
                    ecmaFeatures.modules || ecmaFeatures.defaultParams ||
                    ecmaFeatures.destructuring) ? 6 : 5;


            // gather data that may be needed by the rules
            scopeManager = escope.analyze(ast, {
                ignoreEval: true,
                nodejsScope: ecmaFeatures.globalReturn,
                ecmaVersion: ecmaVersion,
                sourceType: ecmaFeatures.modules ? "module" : "script"
            });
            currentScopes = scopeManager.scopes;

            /*
             * Index the scopes by the start range of their block for efficient
             * lookup in getScope.
             */
            scopeMap = [];
            currentScopes.forEach(function (scope, index) {
                var range = scope.block.range[0];

                // Sometimes two scopes are returned for a given node. This is
                // handled later in a known way, so just don't overwrite here.
                if (!scopeMap[range]) {
                    scopeMap[range] = index;
                }
            });

            /*
             * Split text here into array of lines so
             * it's not being done repeatedly
             * by individual rules.
             */
            currentTextLines = currentText.split(/\r\n|\r|\n|\u2028|\u2029/g);

            // Freezing so array isn't accidentally changed by a rule.
            Object.freeze(currentTextLines);

            currentTokens = createTokenStore(ast.tokens);
            Object.keys(currentTokens).forEach(function(method) {
                api[method] = currentTokens[method];
            });

            // augment global scope with declared global variables
            addDeclaredGlobals(ast, currentScopes[0], currentConfig);

            // remove shebang comments
            if (shebang && ast.comments.length && ast.comments[0].value === shebang) {
                ast.comments.splice(0, 1);

                if (ast.body.length && ast.body[0].leadingComments && ast.body[0].leadingComments[0].value === shebang) {
                    ast.body[0].leadingComments.splice(0, 1);
                }
            }

            /*
             * Each node has a type property. Whenever a particular type of node is found,
             * an event is fired. This allows any listeners to automatically be informed
             * that this type of node has been found and react accordingly.
             */
            controller.traverse(ast, {
                enter: function(node, parent) {

                    var comments = api.getComments(node);

                    emitCommentsEnter(comments.leading);
                    node.parent = parent;
                    api.emit(node.type, node);
                    emitCommentsEnter(comments.trailing);
                },
                leave: function(node) {

                    var comments = api.getComments(node);

                    emitCommentsExit(comments.trailing);
                    api.emit(node.type + ":exit", node);
                    emitCommentsExit(comments.leading);
                }
            });

        }

        // sort by line and column
        messages.sort(function(a, b) {
            var lineDiff = a.line - b.line;

            if (lineDiff === 0) {
                return a.column - b.column;
            } else {
                return lineDiff;
            }
        });

        return messages;
    };

    /**
     * Reports a message from one of the rules.
     * @param {string} ruleId The ID of the rule causing the message.
     * @param {number} severity The severity level of the rule as configured.
     * @param {ASTNode} node The AST node that the message relates to.
     * @param {Object=} location An object containing the error line and column
     *      numbers. If location is not provided the node's start location will
     *      be used.
     * @param {string} message The actual message.
     * @param {Object} opts Optional template data which produces a formatted message
     *     with symbols being replaced by this object's values.
     * @returns {void}
     */
    api.report = function(ruleId, severity, node, location, message, opts) {

        if (typeof location === "string") {
            opts = message;
            message = location;
            location = node.loc.start;
        }

        Object.keys(opts || {}).forEach(function (key) {
            var rx = new RegExp("{{" + escapeRegExp(key) + "}}", "g");
            message = message.replace(rx, opts[key]);
        });

        if (isDisabledByReportingConfig(reportingConfig, ruleId, location)) {
            return;
        }

        messages.push({
            ruleId: ruleId,
            severity: severity,
            message: message,
            line: location.line,
            column: location.column,
            nodeType: node.type,
            source: currentTextLines[location.line - 1] || ""
        });
    };

    /**
     * Gets the source code for the given node.
     * @param {ASTNode=} node The AST node to get the text for.
     * @param {int=} beforeCount The number of characters before the node to retrieve.
     * @param {int=} afterCount The number of characters after the node to retrieve.
     * @returns {string} The text representing the AST node.
     */
    api.getSource = function(node, beforeCount, afterCount) {
        if (node) {
            return (currentText !== null) ? currentText.slice(Math.max(node.range[0] - (beforeCount || 0), 0),
                node.range[1] + (afterCount || 0)) : null;
        } else {
            return currentText;
        }

    };

    /**
     * Gets the entire source text split into an array of lines.
     * @returns {Array} The source text as an array of lines.
     */
    api.getSourceLines = function() {
        return currentTextLines;
    };

    /**
     * Retrieves an array containing all comments in the source code.
     * @returns {ASTNode[]} An array of comment nodes.
     */
    api.getAllComments = function() {
        return currentAST.comments;
    };

    /**
     * Gets all comments for the given node.
     * @param {ASTNode} node The AST node to get the comments for.
     * @returns {Object} The list of comments indexed by their position.
     */
    api.getComments = function(node) {

        var leadingComments = node.leadingComments || [],
            trailingComments = node.trailingComments || [];

        /*
         * espree adds a "comments" array on Program nodes rather than
         * leadingComments/trailingComments. Comments are only left in the
         * Program node comments array if there is no executable code.
         */
        if (node.type === "Program") {
            if (node.body.length === 0) {
                leadingComments = node.comments;
            }
        }

        return {
            leading: leadingComments,
            trailing: trailingComments
        };
    };

    /**
     * Retrieves the JSDoc comment for a given node.
     * @param {ASTNode} node The AST node to get the comment for.
     * @returns {ASTNode} The BlockComment node containing the JSDoc for the
     *      given node or null if not found.
     */
    api.getJSDocComment = function(node) {

        var parent = node.parent,
            line = node.loc.start.line;

        /**
         * Finds a JSDoc comment node in an array of comment nodes.
         * @param {ASTNode[]} comments The array of comment nodes to search.
         * @returns {ASTNode} The node if found, null if not.
         * @private
         */
        function findJSDocComment(comments) {

            if (comments) {
                for (var i = comments.length - 1; i >= 0; i--) {
                    if (comments[i].type === "Block" && comments[i].value.charAt(0) === "*") {

                        if (line - comments[i].loc.end.line <= 1) {
                            return comments[i];
                        } else {
                            break;
                        }
                    }
                }
            }

            return null;
        }

        switch (node.type) {
            case "FunctionDeclaration":
                return findJSDocComment(node.leadingComments);

            case "ArrowFunctionExpression":
            case "FunctionExpression":

                if (parent.type !== "CallExpression" || parent.callee !== node) {
                    while (parent && !parent.leadingComments && !/Function/.test(parent.type)) {
                        parent = parent.parent;
                    }

                    return parent && (parent.type !== "FunctionDeclaration") ? findJSDocComment(parent.leadingComments) : null;
                }

                // falls through

            default:
                return null;
        }
    };

    /**
     * Gets nodes that are ancestors of current node.
     * @returns {ASTNode[]} Array of objects representing ancestors.
     */
    api.getAncestors = function() {
        return controller.parents();
    };

    /**
     * Gets the deepest node containing a range index.
     * @param {int} index Range index of the desired node.
     * @returns {ASTNode} [description]
     */
    api.getNodeByRangeIndex = function(index) {
        var result = null;

        estraverse.traverse(controller.root, {
            enter: function (node) {
                if (node.range[0] <= index && index < node.range[1]) {
                    result = node;
                } else {
                    this.skip();
                }
            },
            leave: function (node) {
                if (node === result) {
                    this.break();
                }
            }
        });

        return result;
    };

    /**
     * Gets the scope for the current node.
     * @returns {Object} An object representing the current node's scope.
     */
    api.getScope = function() {
        var parents = controller.parents(),
            scope = currentScopes[0];

        // Don't do this for Program nodes - they have no parents
        if (parents.length) {

            // if current node is function declaration, add it to the list
            var current = controller.current();
            if (["FunctionDeclaration", "FunctionExpression",
                    "ArrowFunctionExpression"].indexOf(current.type) >= 0) {
                parents.push(current);
            }

            // Ascend the current node's parents
            for (var i = parents.length - 1; i >= 0; --i) {

                scope = scopeManager.acquire(parents[i]);
                if (scope) {
                    if (scope.type === "function-expression-name") {
                        return scope.childScopes[0];
                    } else {
                        return scope;
                    }
                }

            }

        }

        return currentScopes[0];
    };

    /**
     * Record that a particular variable has been used in code
     * @param {string} name The name of the variable to mark as used
     * @returns {boolean} True if the variable was found and marked as used,
     *      false if not.
     */
    api.markVariableAsUsed = function(name) {
        var scope = this.getScope(),
            specialScope = currentConfig.ecmaFeatures.globalReturn || currentConfig.ecmaFeatures.modules,
            variables,
            i,
            len;

        // Special Node.js scope means we need to start one level deeper
        if (scope.type === "global" && specialScope) {
            scope = scope.childScopes[0];
        }

        do {
            variables = scope.variables;
            for (i = 0, len = variables.length; i < len; i++) {
                if (variables[i].name === name) {
                    variables[i].eslintUsed = true;
                    return true;
                }
            }
        } while ( (scope = scope.upper) );

        return false;
    };

    /**
     * Gets the filename for the currently parsed source.
     * @returns {string} The filename associated with the source being parsed.
     *     Defaults to "<input>" if no filename info is present.
     */
    api.getFilename = function() {
        if (typeof currentFilename === "string") {
            return currentFilename;
        } else {
            return "<input>";
        }
    };

    /**
     * Defines a new linting rule.
     * @param {string} ruleId A unique rule identifier
     * @param {Function} ruleModule Function from context to object mapping AST node types to event handlers
     * @returns {void}
     */
    var defineRule = api.defineRule = function(ruleId, ruleModule) {
        rules.define(ruleId, ruleModule);
    };

    /**
     * Defines many new linting rules.
     * @param {object} rulesToDefine map from unique rule identifier to rule
     * @returns {void}
     */
    api.defineRules = function(rulesToDefine) {
        Object.getOwnPropertyNames(rulesToDefine).forEach(function(ruleId) {
            defineRule(ruleId, rulesToDefine[ruleId]);
        });
    };

    /**
     * Gets the default eslint configuration.
     * @returns {Object} Object mapping rule IDs to their default configurations
     */
    api.defaults = function() {
        return require("../conf/eslint.json");
    };

    return api;

}());
