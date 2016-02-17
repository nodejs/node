/**
 * @fileoverview Main ESLint object.
 * @author Nicholas C. Zakas
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var estraverse = require("./util/estraverse"),
    escope = require("escope"),
    environments = require("../conf/environments"),
    blankScriptAST = require("../conf/blank-script.json"),
    assign = require("object-assign"),
    rules = require("./rules"),
    RuleContext = require("./rule-context"),
    timing = require("./timing"),
    SourceCode = require("./util/source-code"),
    NodeEventGenerator = require("./util/node-event-generator"),
    CommentEventGenerator = require("./util/comment-event-generator"),
    EventEmitter = require("events").EventEmitter,
    ConfigOps = require("./config/config-ops"),
    validator = require("./config/config-validator"),
    replacements = require("../conf/replacements.json"),
    assert = require("assert");

var DEFAULT_PARSER = require("../conf/eslint.json").parser;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Parses a list of "name:boolean_value" or/and "name" options divided by comma or
 * whitespace.
 * @param {string} string The string to parse.
 * @param {Comment} comment The comment node which has the string.
 * @returns {Object} Result map object of names and boolean values
 */
function parseBooleanConfig(string, comment) {
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

        items[name] = {
            value: (value === "true"),
            comment: comment
        };

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
    } catch (ex) {

        messages.push({
            fatal: true,
            severity: 2,
            message: "Failed to parse JSON from '" + string + "': " + ex.message,
            line: location.start.line,
            column: location.start.column + 1
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
        exportedGlobals = {},
        explicitGlobals = {},
        builtin = environments.builtin;

    assign(declaredGlobals, builtin);

    Object.keys(config.env).forEach(function(name) {
        if (config.env[name]) {
            var environmentGlobals = environments[name] && environments[name].globals;
            if (environmentGlobals) {
                assign(declaredGlobals, environmentGlobals);
            }
        }
    });

    assign(exportedGlobals, config.exported);
    assign(declaredGlobals, config.globals);
    assign(explicitGlobals, config.astGlobals);

    Object.keys(declaredGlobals).forEach(function(name) {
        var variable = globalScope.set.get(name);
        if (!variable) {
            variable = new escope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = false;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = declaredGlobals[name];
    });

    Object.keys(explicitGlobals).forEach(function(name) {
        var variable = globalScope.set.get(name);
        if (!variable) {
            variable = new escope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = true;
            variable.eslintExplicitGlobalComment = explicitGlobals[name].comment;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = explicitGlobals[name].value;
    });

    // mark all exported variables as such
    Object.keys(exportedGlobals).forEach(function(name) {
        var variable = globalScope.set.get(name);
        if (variable) {
            variable.eslintUsed = true;
        }
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
 * @param {string} filename The file being checked.
 * @param {ASTNode} ast The top node of the AST.
 * @param {Object} config The existing configuration data.
 * @param {Object[]} reportingConfig The existing reporting configuration data.
 * @param {Object[]} messages The messages queue.
 * @returns {object} Modified config object
 */
function modifyConfigsFromComments(filename, ast, config, reportingConfig, messages) {

    var commentConfig = {
        exported: {},
        astGlobals: {},
        rules: {},
        env: {}
    };
    var commentRules = {};

    ast.comments.forEach(function(comment) {

        var value = comment.value.trim();
        var match = /^(eslint-\w+|eslint-\w+-\w+|eslint|exported|globals?)(\s|$)/.exec(value);

        if (match) {
            value = value.substring(match.index + match[1].length);

            if (comment.type === "Block") {
                switch (match[1]) {
                    case "exported":
                        assign(commentConfig.exported, parseBooleanConfig(value, comment));
                        break;

                    case "globals":
                    case "global":
                        assign(commentConfig.astGlobals, parseBooleanConfig(value, comment));
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
                            validator.validateRuleOptions(name, ruleValue, filename + " line " + comment.loc.start.line);
                            commentRules[name] = ruleValue;
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

    // apply environment configs
    Object.keys(commentConfig.env).forEach(function(name) {
        if (environments[name]) {
            commentConfig = ConfigOps.merge(commentConfig, environments[name]);
        }
    });
    assign(commentConfig.rules, commentRules);

    return ConfigOps.merge(config, commentConfig);
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
            if (config.env[env] && environments[env] && environments[env].ecmaFeatures) {
                assign(ecmaFeatures, environments[env].ecmaFeatures);
            }
        });
    }

    preparedConfig = {
        rules: copiedRules,
        parser: config.parser || DEFAULT_PARSER,
        globals: ConfigOps.merge({}, config.globals),
        env: ConfigOps.merge({}, config.env || {}),
        settings: ConfigOps.merge({}, config.settings || {}),
        ecmaFeatures: ConfigOps.merge(ecmaFeatures, config.ecmaFeatures || {})
    };

    // can't have global return inside of modules
    if (preparedConfig.ecmaFeatures.modules) {
        preparedConfig.ecmaFeatures.globalReturn = false;
    }

    return preparedConfig;
}

/**
 * Provide a stub rule with a given message
 * @param  {string} message The message to be displayed for the rule
 * @returns {Function}      Stub rule function
 */
function createStubRule(message) {

    /**
     * Creates a fake rule object
     * @param {object} context context object for each rule
     * @returns {object} collection of node to listen on
     */
    function createRuleModule(context) {
        return {
            Program: function(node) {
                context.report(node, message);
            }
        };
    }

    if (message) {
        return createRuleModule;
    } else {
        throw new Error("No message passed to stub rule");
    }
}

/**
 * Provide a rule replacement message
 * @param  {string} ruleId Name of the rule
 * @returns {string}       Message detailing rule replacement
 */
function getRuleReplacementMessage(ruleId) {
    if (ruleId in replacements.rules) {
        var newRules = replacements.rules[ruleId];
        return "Rule \'" + ruleId + "\' was removed and replaced by: " + newRules.join(", ");
    }
}

var eslintEnvPattern = /\/\*\s*eslint-env\s(.+?)\*\//g;

/**
 * Checks whether or not there is a comment which has "eslint-env *" in a given text.
 * @param {string} text - A source code text to check.
 * @returns {object|null} A result of parseListConfig() with "eslint-env *" comment.
 */
function findEslintEnv(text) {
    var match, retv;

    eslintEnvPattern.lastIndex = 0;
    while ((match = eslintEnvPattern.exec(text))) {
        retv = assign(retv || {}, parseListConfig(match[1]));
    }

    return retv;
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
        currentConfig = null,
        currentScopes = null,
        scopeMap = null,
        scopeManager = null,
        currentFilename = null,
        controller = null,
        reportingConfig = [],
        sourceCode = null;

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

            // If the message includes a leading line number, strip it:
            var message = ex.message.replace(/^line \d+:/i, "").trim();

            messages.push({
                fatal: true,
                severity: 2,

                message: "Parsing error: " + message,

                line: ex.lineNumber,
                column: ex.column + 1
            });

            return null;
        }
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
        currentConfig = null;
        currentScopes = null;
        scopeMap = null;
        scopeManager = null;
        controller = null;
        reportingConfig = [];
        sourceCode = null;
    };

    /**
     * Verifies the text against the rules specified by the second argument.
     * @param {string|SourceCode} textOrSourceCode The text to parse or a SourceCode object.
     * @param {Object} config An object whose keys specify the rules to use.
     * @param {(string|Object)} [filenameOrOptions] The optional filename of the file being checked.
     *      If this is not set, the filename will default to '<input>' in the rule context. If
     *      an object, then it has "filename", "saveState", and "allowInlineConfig" properties.
     * @param {boolean} [saveState] Indicates if the state from the last run should be saved.
     *      Mostly useful for testing purposes.
     * @param {boolean} [filenameOrOptions.allowInlineConfig] Allow/disallow inline comments' ability to change config once it is set. Defaults to true if not supplied.
     *      Useful if you want to validate JS without comments overriding rules.
     * @returns {Object[]} The results as an array of messages or null if no messages.
     */
    api.verify = function(textOrSourceCode, config, filenameOrOptions, saveState) {

        var ast,
            shebang,
            ecmaFeatures,
            ecmaVersion,
            allowInlineConfig,
            text = (typeof textOrSourceCode === "string") ? textOrSourceCode : null;

        // evaluate arguments
        if (typeof filenameOrOptions === "object") {
            currentFilename = filenameOrOptions.filename;
            allowInlineConfig = filenameOrOptions.allowInlineConfig;
            saveState = filenameOrOptions.saveState;
        } else {
            currentFilename = filenameOrOptions;
        }

        if (!saveState) {
            this.reset();
        }

        // search and apply "eslint-env *".
        var envInFile = findEslintEnv(text || textOrSourceCode.text);
        if (envInFile) {
            if (!config || !config.env) {
                config = assign({}, config || {}, {env: envInFile});
            } else {
                config = assign({}, config);
                config.env = assign({}, config.env, envInFile);
            }
        }

        // process initial config to make it safe to extend
        config = prepareConfig(config || {});

        // only do this for text
        if (text !== null) {

            // there's no input, just exit here
            if (text.trim().length === 0) {
                sourceCode = new SourceCode(text, blankScriptAST);
                return messages;
            }

            ast = parse(text.replace(/^#!([^\r\n]+)/, function(match, captured) {
                shebang = captured;
                return "//" + captured;
            }), config);

            if (ast) {
                sourceCode = new SourceCode(text, ast);
            }

        } else {
            sourceCode = textOrSourceCode;
            ast = sourceCode.ast;
        }

        // if espree failed to parse the file, there's no sense in setting up rules
        if (ast) {

            // parse global comments and modify config
            if (allowInlineConfig !== false) {
                config = modifyConfigsFromComments(currentFilename, ast, config, reportingConfig, messages);
            }

            // enable appropriate rules
            Object.keys(config.rules).filter(function(key) {
                return getRuleSeverity(config.rules[key]) > 0;
            }).forEach(function(key) {
                var ruleCreator,
                    severity,
                    options,
                    rule;

                ruleCreator = rules.get(key);
                if (!ruleCreator) {
                    var replacementMsg = getRuleReplacementMessage(key);
                    if (replacementMsg) {
                        ruleCreator = createStubRule(replacementMsg);
                    } else {
                        ruleCreator = createStubRule("Definition for rule '" + key + "' was not found");
                    }
                    rules.define(key, ruleCreator);
                }

                severity = getRuleSeverity(config.rules[key]);
                options = getRuleOptions(config.rules[key]);

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
                } catch (ex) {
                    ex.message = "Error while loading rule '" + key + "': " + ex.message;
                    throw ex;
                }
            });

            // save config so rules can access as necessary
            currentConfig = config;
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
            currentScopes.forEach(function(scope, index) {
                var range = scope.block.range[0];

                // Sometimes two scopes are returned for a given node. This is
                // handled later in a known way, so just don't overwrite here.
                if (!scopeMap[range]) {
                    scopeMap[range] = index;
                }
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

            var eventGenerator = new NodeEventGenerator(api);
            eventGenerator = new CommentEventGenerator(eventGenerator, sourceCode);

            /*
             * Each node has a type property. Whenever a particular type of node is found,
             * an event is fired. This allows any listeners to automatically be informed
             * that this type of node has been found and react accordingly.
             */
            controller.traverse(ast, {
                enter: function(node, parent) {
                    node.parent = parent;
                    eventGenerator.enterNode(node);
                },
                leave: function(node) {
                    eventGenerator.leaveNode(node);
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
     * @param {Object} fix A fix command description.
     * @returns {void}
     */
    api.report = function(ruleId, severity, node, location, message, opts, fix) {
        if (node) {
            assert.strictEqual(typeof node, "object", "Node must be an object");
        }

        if (typeof location === "string") {
            assert.ok(node, "Node must be provided when reporting error if location is not provided");

            fix = opts;
            opts = message;
            message = location;
            location = node.loc.start;
        }
        // else, assume location was provided, so node may be omitted

        if (isDisabledByReportingConfig(reportingConfig, ruleId, location)) {
            return;
        }

        if (opts) {
            message = message.replace(/\{\{\s*(.+?)\s*\}\}/g, function(fullMatch, term) {
                if (term in opts) {
                    return opts[term];
                }

                // Preserve old behavior: If parameter name not provided, don't replace it.
                return fullMatch;
            });
        }

        var problem = {
            ruleId: ruleId,
            severity: severity,
            message: message,
            line: location.line,
            column: location.column + 1,   // switch to 1-base instead of 0-base
            nodeType: node && node.type,
            source: sourceCode.lines[location.line - 1] || ""
        };

        // ensure there's range and text properties, otherwise it's not a valid fix
        if (fix && Array.isArray(fix.range) && (typeof fix.text === "string")) {
            problem.fix = fix;
        }

        messages.push(problem);
    };

    /**
     * Gets the SourceCode object representing the parsed source.
     * @returns {SourceCode} The SourceCode object.
     */
    api.getSourceCode = function() {
        return sourceCode;
    };

    // methods that exist on SourceCode object
    var externalMethods = {
        getSource: "getText",
        getSourceLines: "getLines",
        getAllComments: "getAllComments",
        getNodeByRangeIndex: "getNodeByRangeIndex",
        getComments: "getComments",
        getJSDocComment: "getJSDocComment",
        getFirstToken: "getFirstToken",
        getFirstTokens: "getFirstTokens",
        getLastToken: "getLastToken",
        getLastTokens: "getLastTokens",
        getTokenAfter: "getTokenAfter",
        getTokenBefore: "getTokenBefore",
        getTokenByRangeStart: "getTokenByRangeStart",
        getTokens: "getTokens",
        getTokensAfter: "getTokensAfter",
        getTokensBefore: "getTokensBefore",
        getTokensBetween: "getTokensBetween"
    };

    // copy over methods
    Object.keys(externalMethods).forEach(function(methodName) {
        var exMethodName = externalMethods[methodName];

        // All functions expected to have less arguments than 5.
        api[methodName] = function(a, b, c, d, e) {
            if (sourceCode) {
                return sourceCode[exMethodName](a, b, c, d, e);
            }
            return null;
        };
    });

    /**
     * Gets nodes that are ancestors of current node.
     * @returns {ASTNode[]} Array of objects representing ancestors.
     */
    api.getAncestors = function() {
        return controller.parents();
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

            // if current node introduces a scope, add it to the list
            var current = controller.current();
            if (currentConfig.ecmaFeatures.blockBindings) {
                if (["BlockStatement", "SwitchStatement", "CatchClause", "FunctionDeclaration", "FunctionExpression", "ArrowFunctionExpression"].indexOf(current.type) >= 0) {
                    parents.push(current);
                }
            } else {
                if (["FunctionDeclaration", "FunctionExpression", "ArrowFunctionExpression"].indexOf(current.type) >= 0) {
                    parents.push(current);
                }
            }

            // Ascend the current node's parents
            for (var i = parents.length - 1; i >= 0; --i) {

                // Get the innermost scope
                scope = scopeManager.acquire(parents[i], true);
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

    /**
     * Gets variables that are declared by a specified node.
     *
     * The variables are its `defs[].node` or `defs[].parent` is same as the specified node.
     * Specifically, below:
     *
     * - `VariableDeclaration` - variables of its all declarators.
     * - `VariableDeclarator` - variables.
     * - `FunctionDeclaration`/`FunctionExpression` - its function name and parameters.
     * - `ArrowFunctionExpression` - its parameters.
     * - `ClassDeclaration`/`ClassExpression` - its class name.
     * - `CatchClause` - variables of its exception.
     * - `ImportDeclaration` - variables of  its all specifiers.
     * - `ImportSpecifier`/`ImportDefaultSpecifier`/`ImportNamespaceSpecifier` - a variable.
     * - others - always an empty array.
     *
     * @param {ASTNode} node A node to get.
     * @returns {escope.Variable[]} Variables that are declared by the node.
     */
    api.getDeclaredVariables = function(node) {
        return (scopeManager && scopeManager.getDeclaredVariables(node)) || [];
    };

    return api;

}());
