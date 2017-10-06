/**
 * @fileoverview Main Linter Class
 * @author Gyandeep Singh
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const assert = require("assert"),
    EventEmitter = require("events").EventEmitter,
    eslintScope = require("eslint-scope"),
    levn = require("levn"),
    blankScriptAST = require("../conf/blank-script.json"),
    defaultConfig = require("../conf/default-config-options.js"),
    replacements = require("../conf/replacements.json"),
    CodePathAnalyzer = require("./code-path-analysis/code-path-analyzer"),
    ConfigOps = require("./config/config-ops"),
    validator = require("./config/config-validator"),
    Environments = require("./config/environments"),
    NodeEventGenerator = require("./util/node-event-generator"),
    SourceCode = require("./util/source-code"),
    Traverser = require("./util/traverser"),
    RuleContext = require("./rule-context"),
    Rules = require("./rules"),
    timing = require("./timing"),
    astUtils = require("./ast-utils"),
    pkg = require("../package.json"),
    SourceCodeFixer = require("./util/source-code-fixer");

const debug = require("debug")("eslint:linter");
const MAX_AUTOFIX_PASSES = 10;

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

/**
 * The result of a parsing operation from parseForESLint()
 * @typedef {Object} CustomParseResult
 * @property {ASTNode} ast The ESTree AST Program node.
 * @property {Object} services An object containing additional services related
 *      to the parser.
 */

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
    const items = {};

    // Collapse whitespace around `:` and `,` to make parsing easier
    string = string.replace(/\s*([:,])\s*/g, "$1");

    string.split(/\s|,+/).forEach(name => {
        if (!name) {
            return;
        }
        const pos = name.indexOf(":");
        let value;

        if (pos !== -1) {
            value = name.substring(pos + 1, name.length);
            name = name.substring(0, pos);
        }

        items[name] = {
            value: (value === "true"),
            comment
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
    let items = {};

    // Parses a JSON-like comment by the same way as parsing CLI option.
    try {
        items = levn.parse("Object", string) || {};

        // Some tests say that it should ignore invalid comments such as `/*eslint no-alert:abc*/`.
        // Also, commaless notations have invalid severity:
        //     "no-alert: 2 no-console: 2" --> {"no-alert": "2 no-console: 2"}
        // Should ignore that case as well.
        if (ConfigOps.isEverySeverityValid(items)) {
            return items;
        }
    } catch (ex) {

        // ignore to parse the string by a fallback.
    }

    // Optionator cannot parse commaless notations.
    // But we are supporting that. So this is a fallback for that.
    items = {};
    string = string.replace(/([a-zA-Z0-9\-/]+):/g, "\"$1\":").replace(/(]|[0-9])\s+(?=")/, "$1,");
    try {
        items = JSON.parse(`{${string}}`);
    } catch (ex) {

        messages.push({
            ruleId: null,
            fatal: true,
            severity: 2,
            source: null,
            message: `Failed to parse JSON from '${string}': ${ex.message}`,
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
    const items = {};

    // Collapse whitespace around ,
    string = string.replace(/\s*,\s*/g, ",");

    string.split(/,+/).forEach(name => {
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
 * @param {Environments} envContext Env context
 * @returns {void}
 */
function addDeclaredGlobals(program, globalScope, config, envContext) {
    const declaredGlobals = {},
        exportedGlobals = {},
        explicitGlobals = {},
        builtin = envContext.get("builtin");

    Object.assign(declaredGlobals, builtin);

    Object.keys(config.env).forEach(name => {
        if (config.env[name]) {
            const env = envContext.get(name),
                environmentGlobals = env && env.globals;

            if (environmentGlobals) {
                Object.assign(declaredGlobals, environmentGlobals);
            }
        }
    });

    Object.assign(exportedGlobals, config.exported);
    Object.assign(declaredGlobals, config.globals);
    Object.assign(explicitGlobals, config.astGlobals);

    Object.keys(declaredGlobals).forEach(name => {
        let variable = globalScope.set.get(name);

        if (!variable) {
            variable = new eslintScope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = false;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = declaredGlobals[name];
    });

    Object.keys(explicitGlobals).forEach(name => {
        let variable = globalScope.set.get(name);

        if (!variable) {
            variable = new eslintScope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = true;
            variable.eslintExplicitGlobalComment = explicitGlobals[name].comment;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = explicitGlobals[name].value;
    });

    // mark all exported variables as such
    Object.keys(exportedGlobals).forEach(name => {
        const variable = globalScope.set.get(name);

        if (variable) {
            variable.eslintUsed = true;
        }
    });

    /*
     * "through" contains all references which definitions cannot be found.
     * Since we augment the global scope using configuration, we need to update
     * references and remove the ones that were added by configuration.
     */
    globalScope.through = globalScope.through.filter(reference => {
        const name = reference.identifier.name;
        const variable = globalScope.set.get(name);

        if (variable) {

            /*
             * Links the variable and the reference.
             * And this reference is removed from `Scope#through`.
             */
            reference.resolved = variable;
            variable.references.push(reference);

            return false;
        }

        return true;
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
        rulesToDisable.forEach(rule => {
            reportingConfig.push({
                start,
                end: null,
                rule
            });
        });
    } else {
        reportingConfig.push({
            start,
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
    let i;

    if (rulesToEnable.length) {
        rulesToEnable.forEach(rule => {
            for (i = reportingConfig.length - 1; i >= 0; i--) {
                if (!reportingConfig[i].end && reportingConfig[i].rule === rule) {
                    reportingConfig[i].end = start;
                    break;
                }
            }
        });
    } else {

        // find all previous disabled locations if they was started as list of rules
        let prevStart;

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
 * @param {Linter} linterContext Linter context object
 * @returns {Object} Modified config object
 */
function modifyConfigsFromComments(filename, ast, config, linterContext) {

    let commentConfig = {
        exported: {},
        astGlobals: {},
        rules: {},
        env: {}
    };
    const commentRules = {};
    const messages = linterContext.messages;
    const reportingConfig = linterContext.reportingConfig;

    ast.comments.forEach(comment => {

        let value = comment.value.trim();
        const match = /^(eslint(-\w+){0,3}|exported|globals?)(\s|$)/.exec(value);

        if (match) {
            value = value.substring(match.index + match[1].length);

            if (comment.type === "Block") {
                switch (match[1]) {
                    case "exported":
                        Object.assign(commentConfig.exported, parseBooleanConfig(value, comment));
                        break;

                    case "globals":
                    case "global":
                        Object.assign(commentConfig.astGlobals, parseBooleanConfig(value, comment));
                        break;

                    case "eslint-env":
                        Object.assign(commentConfig.env, parseListConfig(value));
                        break;

                    case "eslint-disable":
                        disableReporting(reportingConfig, comment.loc.start, Object.keys(parseListConfig(value)));
                        break;

                    case "eslint-enable":
                        enableReporting(reportingConfig, comment.loc.start, Object.keys(parseListConfig(value)));
                        break;

                    case "eslint": {
                        const items = parseJsonConfig(value, comment.loc, messages);

                        Object.keys(items).forEach(name => {
                            const ruleValue = items[name];

                            validator.validateRuleOptions(name, ruleValue, `${filename} line ${comment.loc.start.line}`, linterContext.rules);
                            commentRules[name] = ruleValue;
                        });
                        break;
                    }

                    // no default
                }
            } else { // comment.type === "Line"
                if (match[1] === "eslint-disable-line") {
                    disableReporting(reportingConfig, { line: comment.loc.start.line, column: 0 }, Object.keys(parseListConfig(value)));
                    enableReporting(reportingConfig, comment.loc.end, Object.keys(parseListConfig(value)));
                } else if (match[1] === "eslint-disable-next-line") {
                    disableReporting(reportingConfig, comment.loc.start, Object.keys(parseListConfig(value)));
                    enableReporting(reportingConfig, { line: comment.loc.start.line + 2 }, Object.keys(parseListConfig(value)));
                }
            }
        }
    });

    // apply environment configs
    Object.keys(commentConfig.env).forEach(name => {
        const env = linterContext.environments.get(name);

        if (env) {
            commentConfig = ConfigOps.merge(commentConfig, env);
        }
    });
    Object.assign(commentConfig.rules, commentRules);

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

    for (let i = 0, c = reportingConfig.length; i < c; i++) {

        const ignore = reportingConfig[i];

        if ((!ignore.rule || ignore.rule === ruleId) &&
            (location.line > ignore.start.line || (location.line === ignore.start.line && location.column >= ignore.start.column)) &&
            (!ignore.end || (location.line < ignore.end.line || (location.line === ignore.end.line && location.column <= ignore.end.column)))) {
            return true;
        }
    }

    return false;
}

/**
 * Normalize ECMAScript version from the initial config
 * @param  {number} ecmaVersion ECMAScript version from the initial config
 * @param  {boolean} isModule Whether the source type is module or not
 * @returns {number} normalized ECMAScript version
 */
function normalizeEcmaVersion(ecmaVersion, isModule) {

    // Need at least ES6 for modules
    if (isModule && (!ecmaVersion || ecmaVersion < 6)) {
        ecmaVersion = 6;
    }

    // Calculate ECMAScript edition number from official year version starting with
    // ES2015, which corresponds with ES6 (or a difference of 2009).
    if (ecmaVersion >= 2015) {
        ecmaVersion -= 2009;
    }

    return ecmaVersion;
}

/**
 * Process initial config to make it safe to extend by file comment config
 * @param  {Object} config Initial config
 * @param  {Environments} envContext Env context
 * @returns {Object}        Processed config
 */
function prepareConfig(config, envContext) {
    config.globals = config.globals || {};
    const copiedRules = {};
    let parserOptions = {};

    if (typeof config.rules === "object") {
        Object.keys(config.rules).forEach(k => {
            const rule = config.rules[k];

            if (rule === null) {
                throw new Error(`Invalid config for rule '${k}'.`);
            }
            if (Array.isArray(rule)) {
                copiedRules[k] = rule.slice();
            } else {
                copiedRules[k] = rule;
            }
        });
    }

    // merge in environment parserOptions
    if (typeof config.env === "object") {
        Object.keys(config.env).forEach(envName => {
            const env = envContext.get(envName);

            if (config.env[envName] && env && env.parserOptions) {
                parserOptions = ConfigOps.merge(parserOptions, env.parserOptions);
            }
        });
    }

    const preparedConfig = {
        rules: copiedRules,
        parser: config.parser || defaultConfig.parser,
        globals: ConfigOps.merge(defaultConfig.globals, config.globals),
        env: ConfigOps.merge(defaultConfig.env, config.env || {}),
        settings: ConfigOps.merge(defaultConfig.settings, config.settings || {}),
        parserOptions: ConfigOps.merge(parserOptions, config.parserOptions || {})
    };
    const isModule = preparedConfig.parserOptions.sourceType === "module";

    if (isModule) {

        // can't have global return inside of modules
        preparedConfig.parserOptions.ecmaFeatures = Object.assign({}, preparedConfig.parserOptions.ecmaFeatures, { globalReturn: false });
    }

    preparedConfig.parserOptions.ecmaVersion = normalizeEcmaVersion(preparedConfig.parserOptions.ecmaVersion, isModule);

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
     * @param {Object} context context object for each rule
     * @returns {Object} collection of node to listen on
     */
    function createRuleModule(context) {
        return {
            Program(node) {
                context.report(node, message);
            }
        };
    }

    if (message) {
        return createRuleModule;
    }
    throw new Error("No message passed to stub rule");

}

/**
 * Provide a rule replacement message
 * @param  {string} ruleId Name of the rule
 * @returns {string}       Message detailing rule replacement
 */
function getRuleReplacementMessage(ruleId) {
    if (ruleId in replacements.rules) {
        const newRules = replacements.rules[ruleId];

        return `Rule '${ruleId}' was removed and replaced by: ${newRules.join(", ")}`;
    }

    return null;
}

const eslintEnvPattern = /\/\*\s*eslint-env\s(.+?)\*\//g;

/**
 * Checks whether or not there is a comment which has "eslint-env *" in a given text.
 * @param {string} text - A source code text to check.
 * @returns {Object|null} A result of parseListConfig() with "eslint-env *" comment.
 */
function findEslintEnv(text) {
    let match, retv;

    eslintEnvPattern.lastIndex = 0;

    while ((match = eslintEnvPattern.exec(text))) {
        retv = Object.assign(retv || {}, parseListConfig(match[1]));
    }

    return retv;
}

/**
 * Strips Unicode BOM from a given text.
 *
 * @param {string} text - A text to strip.
 * @returns {string} The stripped text.
 */
function stripUnicodeBOM(text) {

    /*
     * Check Unicode BOM.
     * In JavaScript, string data is stored as UTF-16, so BOM is 0xFEFF.
     * http://www.ecma-international.org/ecma-262/6.0/#sec-unicode-format-control-characters
     */
    if (text.charCodeAt(0) === 0xFEFF) {
        return text.slice(1);
    }
    return text;
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
    }
    return 0;

}

/**
 * Get the options for a rule (not including severity), if any
 * @param {Array|number} ruleConfig rule configuration
 * @returns {Array} of rule options, empty Array if none
 */
function getRuleOptions(ruleConfig) {
    if (Array.isArray(ruleConfig)) {
        return ruleConfig.slice(1);
    }
    return [];

}

/**
 * Parses text into an AST. Moved out here because the try-catch prevents
 * optimization of functions, so it's best to keep the try-catch as isolated
 * as possible
 * @param {string} text The text to parse.
 * @param {Object} config The ESLint configuration object.
 * @param {string} filePath The path to the file being parsed.
 * @returns {ASTNode|CustomParseResult} The AST or parse result if successful,
 *      or null if not.
 * @param {Array<Object>} messages Messages array for the linter object
 * @returns {*} parsed text if successful otherwise null
 * @private
 */
function parse(text, config, filePath, messages) {

    let parser,
        parserOptions = {
            loc: true,
            range: true,
            raw: true,
            tokens: true,
            comment: true,
            filePath
        };

    try {
        parser = require(config.parser);
    } catch (ex) {
        messages.push({
            ruleId: null,
            fatal: true,
            severity: 2,
            source: null,
            message: ex.message,
            line: 0,
            column: 0
        });

        return null;
    }

    // merge in any additional parser options
    if (config.parserOptions) {
        parserOptions = Object.assign({}, config.parserOptions, parserOptions);
    }

    /*
     * Check for parsing errors first. If there's a parsing error, nothing
     * else can happen. However, a parsing error does not throw an error
     * from this method - it's just considered a fatal error message, a
     * problem that ESLint identified just like any other.
     */
    try {
        if (typeof parser.parseForESLint === "function") {
            return parser.parseForESLint(text, parserOptions);
        }
        return parser.parse(text, parserOptions);

    } catch (ex) {

        // If the message includes a leading line number, strip it:
        const message = ex.message.replace(/^line \d+:/i, "").trim();
        const source = (ex.lineNumber) ? SourceCode.splitLines(text)[ex.lineNumber - 1] : null;

        messages.push({
            ruleId: null,
            fatal: true,
            severity: 2,
            source,
            message: `Parsing error: ${message}`,

            line: ex.lineNumber,
            column: ex.column
        });

        return null;
    }
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Object that is responsible for verifying JavaScript text
 * @name eslint
 */
class Linter extends EventEmitter {

    constructor() {
        super();
        this.messages = [];
        this.currentConfig = null;
        this.currentScopes = null;
        this.scopeManager = null;
        this.currentFilename = null;
        this.traverser = null;
        this.reportingConfig = [];
        this.sourceCode = null;
        this.version = pkg.version;

        this.rules = new Rules();
        this.environments = new Environments();

        // set unlimited listeners (see https://github.com/eslint/eslint/issues/524)
        this.setMaxListeners(0);
    }

    /**
     * Resets the internal state of the object.
     * @returns {void}
     */
    reset() {
        this.removeAllListeners();
        this.messages = [];
        this.currentConfig = null;
        this.currentScopes = null;
        this.scopeManager = null;
        this.traverser = null;
        this.reportingConfig = [];
        this.sourceCode = null;
    }

    /**
     * Configuration object for the `verify` API. A JS representation of the eslintrc files.
     * @typedef {Object} ESLintConfig
     * @property {Object} rules The rule configuration to verify against.
     * @property {string} [parser] Parser to use when generatig the AST.
     * @property {Object} [parserOptions] Options for the parsed used.
     * @property {Object} [settings] Global settings passed to each rule.
     * @property {Object} [env] The environment to verify in.
     * @property {Object} [globals] Available globals to the code.
     */

    /**
     * Verifies the text against the rules specified by the second argument.
     * @param {string|SourceCode} textOrSourceCode The text to parse or a SourceCode object.
     * @param {ESLintConfig} config An ESLintConfig instance to configure everything.
     * @param {(string|Object)} [filenameOrOptions] The optional filename of the file being checked.
     *      If this is not set, the filename will default to '<input>' in the rule context. If
     *      an object, then it has "filename", "saveState", and "allowInlineConfig" properties.
     * @param {boolean} [saveState] Indicates if the state from the last run should be saved.
     *      Mostly useful for testing purposes.
     * @param {boolean} [filenameOrOptions.allowInlineConfig] Allow/disallow inline comments' ability to change config once it is set. Defaults to true if not supplied.
     *      Useful if you want to validate JS without comments overriding rules.
     * @returns {Object[]} The results as an array of messages or null if no messages.
     */
    verify(textOrSourceCode, config, filenameOrOptions, saveState) {
        const text = (typeof textOrSourceCode === "string") ? textOrSourceCode : null;
        let ast,
            parseResult,
            allowInlineConfig;

        // evaluate arguments
        if (typeof filenameOrOptions === "object") {
            this.currentFilename = filenameOrOptions.filename;
            allowInlineConfig = filenameOrOptions.allowInlineConfig;
            saveState = filenameOrOptions.saveState;
        } else {
            this.currentFilename = filenameOrOptions;
        }

        if (!saveState) {
            this.reset();
        }

        // search and apply "eslint-env *".
        const envInFile = findEslintEnv(text || textOrSourceCode.text);

        config = Object.assign({}, config);

        if (envInFile) {
            if (config.env) {
                config.env = Object.assign({}, config.env, envInFile);
            } else {
                config.env = envInFile;
            }
        }

        // process initial config to make it safe to extend
        config = prepareConfig(config, this.environments);

        // only do this for text
        if (text !== null) {

            // there's no input, just exit here
            if (text.trim().length === 0) {
                this.sourceCode = new SourceCode(text, blankScriptAST);
                return this.messages;
            }

            parseResult = parse(
                stripUnicodeBOM(text).replace(astUtils.SHEBANG_MATCHER, (match, captured) => `//${captured}`),
                config,
                this.currentFilename,
                this.messages
            );

            // if this result is from a parseForESLint() method, normalize
            if (parseResult && parseResult.ast) {
                ast = parseResult.ast;
            } else {
                ast = parseResult;
                parseResult = null;
            }

            if (ast) {
                this.sourceCode = new SourceCode(text, ast);
            }

        } else {
            this.sourceCode = textOrSourceCode;
            ast = this.sourceCode.ast;
        }

        // if espree failed to parse the file, there's no sense in setting up rules
        if (ast) {

            // parse global comments and modify config
            if (allowInlineConfig !== false) {
                config = modifyConfigsFromComments(this.currentFilename, ast, config, this);
            }

            // ensure that severities are normalized in the config
            ConfigOps.normalize(config);

            // enable appropriate rules
            Object.keys(config.rules).filter(key => getRuleSeverity(config.rules[key]) > 0).forEach(key => {
                let ruleCreator;

                ruleCreator = this.rules.get(key);

                if (!ruleCreator) {
                    const replacementMsg = getRuleReplacementMessage(key);

                    if (replacementMsg) {
                        ruleCreator = createStubRule(replacementMsg);
                    } else {
                        ruleCreator = createStubRule(`Definition for rule '${key}' was not found`);
                    }
                    this.rules.define(key, ruleCreator);
                }

                const severity = getRuleSeverity(config.rules[key]);
                const options = getRuleOptions(config.rules[key]);

                try {
                    const ruleContext = new RuleContext(
                        key, this, severity, options,
                        config.settings, config.parserOptions, config.parser,
                        ruleCreator.meta,
                        (parseResult && parseResult.services ? parseResult.services : {})
                    );

                    const rule = ruleCreator.create ? ruleCreator.create(ruleContext)
                        : ruleCreator(ruleContext);

                    // add all the selectors from the rule as listeners
                    Object.keys(rule).forEach(selector => {
                        this.on(selector, timing.enabled
                            ? timing.time(key, rule[selector])
                            : rule[selector]
                        );
                    });
                } catch (ex) {
                    ex.message = `Error while loading rule '${key}': ${ex.message}`;
                    throw ex;
                }
            });

            // save config so rules can access as necessary
            this.currentConfig = config;
            this.traverser = new Traverser();

            const ecmaFeatures = this.currentConfig.parserOptions.ecmaFeatures || {};
            const ecmaVersion = this.currentConfig.parserOptions.ecmaVersion || 5;

            // gather scope data that may be needed by the rules
            this.scopeManager = eslintScope.analyze(ast, {
                ignoreEval: true,
                nodejsScope: ecmaFeatures.globalReturn,
                impliedStrict: ecmaFeatures.impliedStrict,
                ecmaVersion,
                sourceType: this.currentConfig.parserOptions.sourceType || "script",
                fallback: Traverser.getKeys
            });

            this.currentScopes = this.scopeManager.scopes;

            // augment global scope with declared global variables
            addDeclaredGlobals(ast, this.currentScopes[0], this.currentConfig, this.environments);

            let eventGenerator = new NodeEventGenerator(this);

            eventGenerator = new CodePathAnalyzer(eventGenerator);

            /*
             * Each node has a type property. Whenever a particular type of
             * node is found, an event is fired. This allows any listeners to
             * automatically be informed that this type of node has been found
             * and react accordingly.
             */
            this.traverser.traverse(ast, {
                enter(node, parent) {
                    node.parent = parent;
                    eventGenerator.enterNode(node);
                },
                leave(node) {
                    eventGenerator.leaveNode(node);
                }
            });
        }

        // sort by line and column
        this.messages.sort((a, b) => {
            const lineDiff = a.line - b.line;

            if (lineDiff === 0) {
                return a.column - b.column;
            }
            return lineDiff;

        });

        return this.messages;
    }

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
     * @param {Object} meta Metadata of the rule
     * @returns {void}
     */
    report(ruleId, severity, node, location, message, opts, fix, meta) {
        if (node) {
            assert.strictEqual(typeof node, "object", "Node must be an object");
        }

        let endLocation;

        if (typeof location === "string") {
            assert.ok(node, "Node must be provided when reporting error if location is not provided");

            meta = fix;
            fix = opts;
            opts = message;
            message = location;
            location = node.loc.start;
            endLocation = node.loc.end;
        } else {
            endLocation = location.end;
        }

        location = location.start || location;

        if (isDisabledByReportingConfig(this.reportingConfig, ruleId, location)) {
            return;
        }

        if (opts) {
            message = message.replace(/\{\{\s*([^{}]+?)\s*\}\}/g, (fullMatch, term) => {
                if (term in opts) {
                    return opts[term];
                }

                // Preserve old behavior: If parameter name not provided, don't replace it.
                return fullMatch;
            });
        }

        const problem = {
            ruleId,
            severity,
            message,
            line: location.line,
            column: location.column + 1, // switch to 1-base instead of 0-base
            nodeType: node && node.type,
            source: this.sourceCode.lines[location.line - 1] || ""
        };

        // Define endLine and endColumn if exists.
        if (endLocation) {
            problem.endLine = endLocation.line;
            problem.endColumn = endLocation.column + 1; // switch to 1-base instead of 0-base
        }

        // ensure there's range and text properties, otherwise it's not a valid fix
        if (fix && Array.isArray(fix.range) && (typeof fix.text === "string")) {

            // If rule uses fix, has metadata, but has no metadata.fixable, we should throw
            if (meta && !meta.fixable) {
                throw new Error("Fixable rules should export a `meta.fixable` property.");
            }

            problem.fix = fix;
        }

        this.messages.push(problem);
    }

    /**
     * Gets the SourceCode object representing the parsed source.
     * @returns {SourceCode} The SourceCode object.
     */
    getSourceCode() {
        return this.sourceCode;
    }

    /**
     * Gets nodes that are ancestors of current node.
     * @returns {ASTNode[]} Array of objects representing ancestors.
     */
    getAncestors() {
        return this.traverser.parents();
    }

    /**
     * Gets the scope for the current node.
     * @returns {Object} An object representing the current node's scope.
     */
    getScope() {
        const parents = this.traverser.parents();

        // Don't do this for Program nodes - they have no parents
        if (parents.length) {

            // if current node introduces a scope, add it to the list
            const current = this.traverser.current();

            if (this.currentConfig.parserOptions.ecmaVersion >= 6) {
                if (["BlockStatement", "SwitchStatement", "CatchClause", "FunctionDeclaration", "FunctionExpression", "ArrowFunctionExpression"].indexOf(current.type) >= 0) {
                    parents.push(current);
                }
            } else {
                if (["FunctionDeclaration", "FunctionExpression", "ArrowFunctionExpression"].indexOf(current.type) >= 0) {
                    parents.push(current);
                }
            }

            // Ascend the current node's parents
            for (let i = parents.length - 1; i >= 0; --i) {

                // Get the innermost scope
                const scope = this.scopeManager.acquire(parents[i], true);

                if (scope) {
                    if (scope.type === "function-expression-name") {
                        return scope.childScopes[0];
                    }
                    return scope;

                }

            }

        }

        return this.currentScopes[0];
    }

    /**
     * Record that a particular variable has been used in code
     * @param {string} name The name of the variable to mark as used
     * @returns {boolean} True if the variable was found and marked as used,
     *      false if not.
     */
    markVariableAsUsed(name) {
        const hasGlobalReturn = this.currentConfig.parserOptions.ecmaFeatures && this.currentConfig.parserOptions.ecmaFeatures.globalReturn,
            specialScope = hasGlobalReturn || this.currentConfig.parserOptions.sourceType === "module";
        let scope = this.getScope(),
            i,
            len;

        // Special Node.js scope means we need to start one level deeper
        if (scope.type === "global" && specialScope) {
            scope = scope.childScopes[0];
        }

        do {
            const variables = scope.variables;

            for (i = 0, len = variables.length; i < len; i++) {
                if (variables[i].name === name) {
                    variables[i].eslintUsed = true;
                    return true;
                }
            }
        } while ((scope = scope.upper));

        return false;
    }

    /**
     * Gets the filename for the currently parsed source.
     * @returns {string} The filename associated with the source being parsed.
     *     Defaults to "<input>" if no filename info is present.
     */
    getFilename() {
        if (typeof this.currentFilename === "string") {
            return this.currentFilename;
        }
        return "<input>";

    }

    /**
     * Defines a new linting rule.
     * @param {string} ruleId A unique rule identifier
     * @param {Function} ruleModule Function from context to object mapping AST node types to event handlers
     * @returns {void}
     */
    defineRule(ruleId, ruleModule) {
        this.rules.define(ruleId, ruleModule);
    }

    /**
     * Defines many new linting rules.
     * @param {Object} rulesToDefine map from unique rule identifier to rule
     * @returns {void}
     */
    defineRules(rulesToDefine) {
        Object.getOwnPropertyNames(rulesToDefine).forEach(ruleId => {
            this.defineRule(ruleId, rulesToDefine[ruleId]);
        });
    }

    /**
     * Gets the default eslint configuration.
     * @returns {Object} Object mapping rule IDs to their default configurations
     */
    defaults() { // eslint-disable-line class-methods-use-this
        return defaultConfig;
    }

    /**
     * Gets an object with all loaded rules.
     * @returns {Map} All loaded rules
     */
    getRules() {
        return this.rules.getAllLoadedRules();
    }

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
     * @returns {eslint-scope.Variable[]} Variables that are declared by the node.
     */
    getDeclaredVariables(node) {
        return (this.scopeManager && this.scopeManager.getDeclaredVariables(node)) || [];
    }

    /**
     * Performs multiple autofix passes over the text until as many fixes as possible
     * have been applied.
     * @param {string} text The source text to apply fixes to.
     * @param {Object} config The ESLint config object to use.
     * @param {Object} options The ESLint options object to use.
     * @param {string} options.filename The filename from which the text was read.
     * @param {boolean} options.allowInlineConfig Flag indicating if inline comments
     *      should be allowed.
     * @param {boolean|Function} options.fix Determines whether fixes should be applied
     * @returns {Object} The result of the fix operation as returned from the
     *      SourceCodeFixer.
     */
    verifyAndFix(text, config, options) {
        let messages = [],
            fixedResult,
            fixed = false,
            passNumber = 0;
        const debugTextDescription = options && options.filename || `${text.slice(0, 10)}...`;
        const shouldFix = options && options.fix || true;

        /**
         * This loop continues until one of the following is true:
         *
         * 1. No more fixes have been applied.
         * 2. Ten passes have been made.
         *
         * That means anytime a fix is successfully applied, there will be another pass.
         * Essentially, guaranteeing a minimum of two passes.
         */
        do {
            passNumber++;

            debug(`Linting code for ${debugTextDescription} (pass ${passNumber})`);
            messages = this.verify(text, config, options);

            debug(`Generating fixed text for ${debugTextDescription} (pass ${passNumber})`);
            fixedResult = SourceCodeFixer.applyFixes(this.getSourceCode(), messages, shouldFix);

            // stop if there are any syntax errors.
            // 'fixedResult.output' is a empty string.
            if (messages.length === 1 && messages[0].fatal) {
                break;
            }

            // keep track if any fixes were ever applied - important for return value
            fixed = fixed || fixedResult.fixed;

            // update to use the fixed output instead of the original text
            text = fixedResult.output;

        } while (
            fixedResult.fixed &&
            passNumber < MAX_AUTOFIX_PASSES
        );

        /*
         * If the last result had fixes, we need to lint again to be sure we have
         * the most up-to-date information.
         */
        if (fixedResult.fixed) {
            fixedResult.messages = this.verify(text, config, options);
        }

        // ensure the last result properly reflects if fixes were done
        fixedResult.fixed = fixed;
        fixedResult.output = text;

        return fixedResult;
    }
}

// methods that exist on SourceCode object
const externalMethods = {
    getSource: "getText",
    getSourceLines: "getLines",
    getAllComments: "getAllComments",
    getNodeByRangeIndex: "getNodeByRangeIndex",
    getComments: "getComments",
    getCommentsBefore: "getCommentsBefore",
    getCommentsAfter: "getCommentsAfter",
    getCommentsInside: "getCommentsInside",
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
Object.keys(externalMethods).forEach(methodName => {
    const exMethodName = externalMethods[methodName];

    // All functions expected to have less arguments than 5.
    Linter.prototype[methodName] = function(a, b, c, d, e) {
        if (this.sourceCode) {
            return this.sourceCode[exMethodName](a, b, c, d, e);
        }
        return null;
    };
});

module.exports = Linter;
