/**
 * @fileoverview Main Linter Class
 * @author Gyandeep Singh
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const eslintScope = require("eslint-scope"),
    evk = require("eslint-visitor-keys"),
    levn = require("levn"),
    lodash = require("lodash"),
    blankScriptAST = require("../conf/blank-script.json"),
    CodePathAnalyzer = require("./code-path-analysis/code-path-analyzer"),
    ConfigOps = require("./config/config-ops"),
    validator = require("./config/config-validator"),
    Environments = require("./config/environments"),
    applyDisableDirectives = require("./util/apply-disable-directives"),
    createEmitter = require("./util/safe-emitter"),
    NodeEventGenerator = require("./util/node-event-generator"),
    SourceCode = require("./util/source-code"),
    Traverser = require("./util/traverser"),
    createReportTranslator = require("./report-translator"),
    Rules = require("./rules"),
    timing = require("./timing"),
    astUtils = require("./ast-utils"),
    pkg = require("../package.json"),
    SourceCodeFixer = require("./util/source-code-fixer");

const debug = require("debug")("eslint:linter");
const MAX_AUTOFIX_PASSES = 10;
const DEFAULT_PARSER_NAME = "espree";

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

/**
 * The result of a parsing operation from parseForESLint()
 * @typedef {Object} CustomParseResult
 * @property {ASTNode} ast The ESTree AST Program node.
 * @property {Object} services An object containing additional services related
 *      to the parser.
 * @property {ScopeManager|null} scopeManager The scope manager object of this AST.
 * @property {Object|null} visitorKeys The visitor keys to traverse this AST.
 */

/**
 * @typedef {Object} DisableDirective
 * @property {("disable"|"enable"|"disable-line"|"disable-next-line")} type
 * @property {number} line
 * @property {number} column
 * @property {(string|null)} ruleId
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
    const trimmedString = string.replace(/\s*([:,])\s*/g, "$1");

    trimmedString.split(/\s|,+/).forEach(name => {
        if (!name) {
            return;
        }
        const pos = name.indexOf(":");

        if (pos === -1) {
            items[name] = {
                value: false,
                comment
            };
        } else {
            items[name.slice(0, pos)] = {
                value: name.slice(pos + 1) === "true",
                comment
            };
        }
    });
    return items;
}

/**
 * Parses a JSON-like config.
 * @param {string} string The string to parse.
 * @param {Object} location Start line and column of comments for potential error message.
 * @returns {({success: true, config: Object}|{success: false, error: Problem})} Result map object
 */
function parseJsonConfig(string, location) {
    let items = {};

    // Parses a JSON-like comment by the same way as parsing CLI option.
    try {
        items = levn.parse("Object", string) || {};

        // Some tests say that it should ignore invalid comments such as `/*eslint no-alert:abc*/`.
        // Also, commaless notations have invalid severity:
        //     "no-alert: 2 no-console: 2" --> {"no-alert": "2 no-console: 2"}
        // Should ignore that case as well.
        if (ConfigOps.isEverySeverityValid(items)) {
            return {
                success: true,
                config: items
            };
        }
    } catch (ex) {

        // ignore to parse the string by a fallback.
    }

    /*
     * Optionator cannot parse commaless notations.
     * But we are supporting that. So this is a fallback for that.
     */
    items = {};
    const normalizedString = string.replace(/([a-zA-Z0-9\-/]+):/g, "\"$1\":").replace(/(]|[0-9])\s+(?=")/, "$1,");

    try {
        items = JSON.parse(`{${normalizedString}}`);
    } catch (ex) {
        return {
            success: false,
            error: {
                ruleId: null,
                fatal: true,
                severity: 2,
                source: null,
                message: `Failed to parse JSON from '${normalizedString}': ${ex.message}`,
                line: location.start.line,
                column: location.start.column + 1
            }
        };

    }

    return {
        success: true,
        config: items
    };
}

/**
 * Parses a config of values separated by comma.
 * @param {string} string The string to parse.
 * @returns {Object} Result map of values and true values
 */
function parseListConfig(string) {
    const items = {};

    // Collapse whitespace around ,
    string.replace(/\s*,\s*/g, ",").split(/,+/).forEach(name => {
        const trimmedName = name.trim();

        if (trimmedName) {
            items[trimmedName] = true;
        }
    });
    return items;
}

/**
 * Ensures that variables representing built-in properties of the Global Object,
 * and any globals declared by special block comments, are present in the global
 * scope.
 * @param {Scope} globalScope The global scope.
 * @param {Object} configGlobals The globals declared in configuration
 * @param {{exportedVariables: Object, enabledGlobals: Object}} commentDirectives Directives from comment configuration
 * @returns {void}
 */
function addDeclaredGlobals(globalScope, configGlobals, commentDirectives) {
    Object.keys(configGlobals).forEach(name => {
        let variable = globalScope.set.get(name);

        if (!variable) {
            variable = new eslintScope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = false;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = configGlobals[name];
    });

    Object.keys(commentDirectives.enabledGlobals).forEach(name => {
        let variable = globalScope.set.get(name);

        if (!variable) {
            variable = new eslintScope.Variable(name, globalScope);
            variable.eslintExplicitGlobal = true;
            variable.eslintExplicitGlobalComment = commentDirectives.enabledGlobals[name].comment;
            globalScope.variables.push(variable);
            globalScope.set.set(name, variable);
        }
        variable.writeable = commentDirectives.enabledGlobals[name].value;
    });

    // mark all exported variables as such
    Object.keys(commentDirectives.exportedVariables).forEach(name => {
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
 * Creates a collection of disable directives from a comment
 * @param {("disable"|"enable"|"disable-line"|"disable-next-line")} type The type of directive comment
 * @param {{line: number, column: number}} loc The 0-based location of the comment token
 * @param {string} value The value after the directive in the comment
 * comment specified no specific rules, so it applies to all rules (e.g. `eslint-disable`)
 * @returns {DisableDirective[]} Directives from the comment
 */
function createDisableDirectives(type, loc, value) {
    const ruleIds = Object.keys(parseListConfig(value));
    const directiveRules = ruleIds.length ? ruleIds : [null];

    return directiveRules.map(ruleId => ({ type, line: loc.line, column: loc.column + 1, ruleId }));
}

/**
 * Parses comments in file to extract file-specific config of rules, globals
 * and environments and merges them with global config; also code blocks
 * where reporting is disabled or enabled and merges them with reporting config.
 * @param {string} filename The file being checked.
 * @param {ASTNode} ast The top node of the AST.
 * @param {function(string): {create: Function}} ruleMapper A map from rule IDs to defined rules
 * @returns {{configuredRules: Object, enabledGlobals: Object, exportedVariables: Object, problems: Problem[], disableDirectives: DisableDirective[]}}
 * A collection of the directive comments that were found, along with any problems that occurred when parsing
 */
function getDirectiveComments(filename, ast, ruleMapper) {
    const configuredRules = {};
    const enabledGlobals = {};
    const exportedVariables = {};
    const problems = [];
    const disableDirectives = [];

    ast.comments.filter(token => token.type !== "Shebang").forEach(comment => {
        const trimmedCommentText = comment.value.trim();
        const match = /^(eslint(-\w+){0,3}|exported|globals?)(\s|$)/.exec(trimmedCommentText);

        if (!match) {
            return;
        }

        const directiveValue = trimmedCommentText.slice(match.index + match[1].length);

        if (/^eslint-disable-(next-)?line$/.test(match[1]) && comment.loc.start.line === comment.loc.end.line) {
            const directiveType = match[1].slice("eslint-".length);

            [].push.apply(disableDirectives, createDisableDirectives(directiveType, comment.loc.start, directiveValue));
        } else if (comment.type === "Block") {
            switch (match[1]) {
                case "exported":
                    Object.assign(exportedVariables, parseBooleanConfig(directiveValue, comment));
                    break;

                case "globals":
                case "global":
                    Object.assign(enabledGlobals, parseBooleanConfig(directiveValue, comment));
                    break;

                case "eslint-disable":
                    [].push.apply(disableDirectives, createDisableDirectives("disable", comment.loc.start, directiveValue));
                    break;

                case "eslint-enable":
                    [].push.apply(disableDirectives, createDisableDirectives("enable", comment.loc.start, directiveValue));
                    break;

                case "eslint": {
                    const parseResult = parseJsonConfig(directiveValue, comment.loc);

                    if (parseResult.success) {
                        Object.keys(parseResult.config).forEach(name => {
                            const ruleValue = parseResult.config[name];

                            try {
                                validator.validateRuleOptions(ruleMapper(name), name, ruleValue);
                            } catch (err) {
                                problems.push({
                                    ruleId: name,
                                    severity: 2,
                                    source: null,
                                    message: err.message,
                                    line: comment.loc.start.line,
                                    column: comment.loc.start.column + 1,
                                    endLine: comment.loc.end.line,
                                    endColumn: comment.loc.end.column + 1,
                                    nodeType: null
                                });
                            }
                            configuredRules[name] = ruleValue;
                        });
                    } else {
                        problems.push(parseResult.error);
                    }

                    break;
                }

                // no default
            }
        }
    });

    return {
        configuredRules,
        enabledGlobals,
        exportedVariables,
        problems,
        disableDirectives
    };
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
        return 6;
    }

    /*
     * Calculate ECMAScript edition number from official year version starting with
     * ES2015, which corresponds with ES6 (or a difference of 2009).
     */
    if (ecmaVersion >= 2015) {
        return ecmaVersion - 2009;
    }

    return ecmaVersion;
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
 * Normalizes the possible options for `linter.verify` and `linter.verifyAndFix` to a
 * consistent shape.
 * @param {(string|{reportUnusedDisableDirectives: boolean, filename: string, allowInlineConfig: boolean})} providedOptions Options
 * @returns {{reportUnusedDisableDirectives: boolean, filename: string, allowInlineConfig: boolean}} Normalized options
 */
function normalizeVerifyOptions(providedOptions) {
    const isObjectOptions = typeof providedOptions === "object";
    const providedFilename = isObjectOptions ? providedOptions.filename : providedOptions;

    return {
        filename: typeof providedFilename === "string" ? providedFilename : "<input>",
        allowInlineConfig: !isObjectOptions || providedOptions.allowInlineConfig !== false,
        reportUnusedDisableDirectives: isObjectOptions && !!providedOptions.reportUnusedDisableDirectives
    };
}

/**
 * Combines the provided parserOptions with the options from environments
 * @param {Object} providedOptions The provided 'parserOptions' key in a config
 * @param {Environment[]} enabledEnvironments The environments enabled in configuration and with inline comments
 * @returns {Object} Resulting parser options after merge
 */
function resolveParserOptions(providedOptions, enabledEnvironments) {
    const parserOptionsFromEnv = enabledEnvironments
        .filter(env => env.parserOptions)
        .reduce((parserOptions, env) => ConfigOps.merge(parserOptions, env.parserOptions), {});

    const mergedParserOptions = ConfigOps.merge(parserOptionsFromEnv, providedOptions || {});

    const isModule = mergedParserOptions.sourceType === "module";

    if (isModule) {

        // can't have global return inside of modules
        mergedParserOptions.ecmaFeatures = Object.assign({}, mergedParserOptions.ecmaFeatures, { globalReturn: false });
    }

    mergedParserOptions.ecmaVersion = normalizeEcmaVersion(mergedParserOptions.ecmaVersion, isModule);

    return mergedParserOptions;
}

/**
 * Combines the provided globals object with the globals from environments
 * @param {Object} providedGlobals The 'globals' key in a config
 * @param {Environments[]} enabledEnvironments The environments enabled in configuration and with inline comments
 * @returns {Object} The resolved globals object
 */
function resolveGlobals(providedGlobals, enabledEnvironments) {
    return Object.assign.apply(
        null,
        [{}]
            .concat(enabledEnvironments.filter(env => env.globals).map(env => env.globals))
            .concat(providedGlobals)
    );
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
 * Analyze scope of the given AST.
 * @param {ASTNode} ast The `Program` node to analyze.
 * @param {Object} parserOptions The parser options.
 * @param {Object} visitorKeys The visitor keys.
 * @returns {ScopeManager} The analysis result.
 */
function analyzeScope(ast, parserOptions, visitorKeys) {
    const ecmaFeatures = parserOptions.ecmaFeatures || {};
    const ecmaVersion = parserOptions.ecmaVersion || 5;

    return eslintScope.analyze(ast, {
        ignoreEval: true,
        nodejsScope: ecmaFeatures.globalReturn,
        impliedStrict: ecmaFeatures.impliedStrict,
        ecmaVersion,
        sourceType: parserOptions.sourceType || "script",
        childVisitorKeys: visitorKeys || evk.KEYS,
        fallback: Traverser.getKeys
    });
}

/**
 * Parses text into an AST. Moved out here because the try-catch prevents
 * optimization of functions, so it's best to keep the try-catch as isolated
 * as possible
 * @param {string} text The text to parse.
 * @param {Object} providedParserOptions Options to pass to the parser
 * @param {string} parserName The name of the parser
 * @param {Map<string, Object>} parserMap A map from names to loaded parsers
 * @param {string} filePath The path to the file being parsed.
 * @returns {{success: false, error: Problem}|{success: true, sourceCode: SourceCode}}
 * An object containing the AST and parser services if parsing was successful, or the error if parsing failed
 * @private
 */
function parse(text, providedParserOptions, parserName, parserMap, filePath) {


    const textToParse = stripUnicodeBOM(text).replace(astUtils.SHEBANG_MATCHER, (match, captured) => `//${captured}`);
    const parserOptions = Object.assign({}, providedParserOptions, {
        loc: true,
        range: true,
        raw: true,
        tokens: true,
        comment: true,
        eslintVisitorKeys: true,
        eslintScopeManager: true,
        filePath
    });

    let parser;

    try {
        parser = parserMap.get(parserName) || require(parserName);
    } catch (ex) {
        return {
            success: false,
            error: {
                ruleId: null,
                fatal: true,
                severity: 2,
                source: null,
                message: ex.message,
                line: 0,
                column: 0
            }
        };
    }

    /*
     * Check for parsing errors first. If there's a parsing error, nothing
     * else can happen. However, a parsing error does not throw an error
     * from this method - it's just considered a fatal error message, a
     * problem that ESLint identified just like any other.
     */
    try {
        const parseResult = (typeof parser.parseForESLint === "function")
            ? parser.parseForESLint(textToParse, parserOptions)
            : { ast: parser.parse(textToParse, parserOptions) };
        const ast = parseResult.ast;
        const parserServices = parseResult.services || {};
        const visitorKeys = parseResult.visitorKeys || evk.KEYS;
        const scopeManager = parseResult.scopeManager || analyzeScope(ast, parserOptions, visitorKeys);

        return {
            success: true,

            /*
             * Save all values that `parseForESLint()` returned.
             * If a `SourceCode` object is given as the first parameter instead of source code text,
             * linter skips the parsing process and reuses the source code object.
             * In that case, linter needs all the values that `parseForESLint()` returned.
             */
            sourceCode: new SourceCode({
                text,
                ast,
                parserServices,
                scopeManager,
                visitorKeys
            })
        };
    } catch (ex) {

        // If the message includes a leading line number, strip it:
        const message = `Parsing error: ${ex.message.replace(/^line \d+:/i, "").trim()}`;
        const source = ex.lineNumber ? SourceCode.splitLines(text)[ex.lineNumber - 1] : null;

        return {
            success: false,
            error: {
                ruleId: null,
                fatal: true,
                severity: 2,
                source,
                message,
                line: ex.lineNumber,
                column: ex.column
            }
        };
    }
}

/**
 * Gets the scope for the current node
 * @param {ScopeManager} scopeManager The scope manager for this AST
 * @param {ASTNode} currentNode The node to get the scope of
 * @param {number} ecmaVersion The `ecmaVersion` setting that this code was parsed with
 * @returns {eslint-scope.Scope} The scope information for this node
 */
function getScope(scopeManager, currentNode, ecmaVersion) {
    let initialNode;

    // if current node introduces a scope, add it to the list
    if (
        ["FunctionDeclaration", "FunctionExpression", "ArrowFunctionExpression"].indexOf(currentNode.type) >= 0 ||
        ecmaVersion >= 6 && ["BlockStatement", "SwitchStatement", "CatchClause"].indexOf(currentNode.type) >= 0
    ) {
        initialNode = currentNode;
    } else {
        initialNode = currentNode.parent;
    }

    // Ascend the current node's parents
    for (let node = initialNode; node; node = node.parent) {

        // Get the innermost scope
        const scope = scopeManager.acquire(node, true);

        if (scope) {
            if (scope.type === "function-expression-name") {
                return scope.childScopes[0];
            }
            return scope;
        }
    }

    return scopeManager.scopes[0];
}

/**
 * Marks a variable as used in the current scope
 * @param {ScopeManager} scopeManager The scope manager for this AST. The scope may be mutated by this function.
 * @param {ASTNode} currentNode The node currently being traversed
 * @param {Object} parserOptions The options used to parse this text
 * @param {string} name The name of the variable that should be marked as used.
 * @returns {boolean} True if the variable was found and marked as used, false if not.
 */
function markVariableAsUsed(scopeManager, currentNode, parserOptions, name) {
    const hasGlobalReturn = parserOptions.ecmaFeatures && parserOptions.ecmaFeatures.globalReturn;
    const specialScope = hasGlobalReturn || parserOptions.sourceType === "module";
    const currentScope = getScope(scopeManager, currentNode, parserOptions.ecmaVersion);

    // Special Node.js scope means we need to start one level deeper
    const initialScope = currentScope.type === "global" && specialScope ? currentScope.childScopes[0] : currentScope;

    for (let scope = initialScope; scope; scope = scope.upper) {
        const variable = scope.variables.find(scopeVar => scopeVar.name === name);

        if (variable) {
            variable.eslintUsed = true;
            return true;
        }
    }

    return false;
}

/**
 * Runs a rule, and gets its listeners
 * @param {Rule} rule A normalized rule with a `create` method
 * @param {Context} ruleContext The context that should be passed to the rule
 * @returns {Object} A map of selector listeners provided by the rule
 */
function createRuleListeners(rule, ruleContext) {
    try {
        return rule.create(ruleContext);
    } catch (ex) {
        ex.message = `Error while loading rule '${ruleContext.id}': ${ex.message}`;
        throw ex;
    }
}

// methods that exist on SourceCode object
const DEPRECATED_SOURCECODE_PASSTHROUGHS = {
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

const BASE_TRAVERSAL_CONTEXT = Object.freeze(
    Object.keys(DEPRECATED_SOURCECODE_PASSTHROUGHS).reduce(
        (contextInfo, methodName) =>
            Object.assign(contextInfo, {
                [methodName]() {
                    const sourceCode = this.getSourceCode();

                    return sourceCode[DEPRECATED_SOURCECODE_PASSTHROUGHS[methodName]].apply(sourceCode, arguments);
                }
            }),
        {}
    )
);

/**
 * Runs the given rules on the given SourceCode object
 * @param {SourceCode} sourceCode A SourceCode object for the given text
 * @param {Object} configuredRules The rules configuration
 * @param {function(string): Rule} ruleMapper A mapper function from rule names to rules
 * @param {Object} parserOptions The options that were passed to the parser
 * @param {string} parserName The name of the parser in the config
 * @param {Object} settings The settings that were enabled in the config
 * @param {string} filename The reported filename of the code
 * @returns {Problem[]} An array of reported problems
 */
function runRules(sourceCode, configuredRules, ruleMapper, parserOptions, parserName, settings, filename) {
    const emitter = createEmitter();
    const traverser = new Traverser();

    /*
     * Create a frozen object with the ruleContext properties and methods that are shared by all rules.
     * All rule contexts will inherit from this object. This avoids the performance penalty of copying all the
     * properties once for each rule.
     */
    const sharedTraversalContext = Object.freeze(
        Object.assign(
            Object.create(BASE_TRAVERSAL_CONTEXT),
            {
                getAncestors: () => traverser.parents(),
                getDeclaredVariables: sourceCode.scopeManager.getDeclaredVariables.bind(sourceCode.scopeManager),
                getFilename: () => filename,
                getScope: () => getScope(sourceCode.scopeManager, traverser.current(), parserOptions.ecmaVersion),
                getSourceCode: () => sourceCode,
                markVariableAsUsed: name => markVariableAsUsed(sourceCode.scopeManager, traverser.current(), parserOptions, name),
                parserOptions,
                parserPath: parserName,
                parserServices: sourceCode.parserServices,
                settings,

                /**
                 * This is used to avoid breaking rules that used to monkeypatch the `Linter#report` method
                 * by using the `_linter` property on rule contexts.
                 *
                 * This should be removed in a major release after we create a better way to
                 * lint for unused disable comments.
                 * https://github.com/eslint/eslint/issues/9193
                 */
                _linter: {
                    report() {},
                    on: emitter.on
                }
            }
        )
    );


    const lintingProblems = [];

    Object.keys(configuredRules).forEach(ruleId => {
        const severity = ConfigOps.getRuleSeverity(configuredRules[ruleId]);

        if (severity === 0) {
            return;
        }

        const rule = ruleMapper(ruleId);
        const messageIds = rule.meta && rule.meta.messages;
        let reportTranslator = null;
        const ruleContext = Object.freeze(
            Object.assign(
                Object.create(sharedTraversalContext),
                {
                    id: ruleId,
                    options: getRuleOptions(configuredRules[ruleId]),
                    report() {

                        /*
                         * Create a report translator lazily.
                         * In a vast majority of cases, any given rule reports zero errors on a given
                         * piece of code. Creating a translator lazily avoids the performance cost of
                         * creating a new translator function for each rule that usually doesn't get
                         * called.
                         *
                         * Using lazy report translators improves end-to-end performance by about 3%
                         * with Node 8.4.0.
                         */
                        if (reportTranslator === null) {
                            reportTranslator = createReportTranslator({ ruleId, severity, sourceCode, messageIds });
                        }
                        const problem = reportTranslator.apply(null, arguments);

                        if (problem.fix && rule.meta && !rule.meta.fixable) {
                            throw new Error("Fixable rules should export a `meta.fixable` property.");
                        }
                        lintingProblems.push(problem);

                        /*
                         * This is used to avoid breaking rules that used monkeypatch Linter, and relied on
                         * `linter.report` getting called with report info every time a rule reports a problem.
                         * To continue to support this, make sure that `context._linter.report` is called every
                         * time a problem is reported by a rule, even though `context._linter` is no longer a
                         * `Linter` instance.
                         *
                         * This should be removed in a major release after we create a better way to
                         * lint for unused disable comments.
                         * https://github.com/eslint/eslint/issues/9193
                         */
                        sharedTraversalContext._linter.report( // eslint-disable-line no-underscore-dangle
                            problem.ruleId,
                            problem.severity,
                            { loc: { start: { line: problem.line, column: problem.column - 1 } } },
                            problem.message
                        );
                    }
                }
            )
        );

        const ruleListeners = createRuleListeners(rule, ruleContext);

        // add all the selectors from the rule as listeners
        Object.keys(ruleListeners).forEach(selector => {
            emitter.on(
                selector,
                timing.enabled
                    ? timing.time(ruleId, ruleListeners[selector])
                    : ruleListeners[selector]
            );
        });
    });

    const eventGenerator = new CodePathAnalyzer(new NodeEventGenerator(emitter));

    /*
     * Each node has a type property. Whenever a particular type of
     * node is found, an event is fired. This allows any listeners to
     * automatically be informed that this type of node has been found
     * and react accordingly.
     */
    traverser.traverse(sourceCode.ast, {
        enter(node, parent) {
            node.parent = parent;
            eventGenerator.enterNode(node);
        },
        leave(node) {
            eventGenerator.leaveNode(node);
        },
        visitorKeys: sourceCode.visitorKeys
    });

    return lintingProblems;
}

const lastSourceCodes = new WeakMap();
const loadedParserMaps = new WeakMap();

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Object that is responsible for verifying JavaScript text
 * @name eslint
 */
module.exports = class Linter {

    constructor() {
        lastSourceCodes.set(this, null);
        loadedParserMaps.set(this, new Map());
        this.version = pkg.version;

        this.rules = new Rules();
        this.environments = new Environments();
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
     * Same as linter.verify, except without support for processors.
     * @param {string|SourceCode} textOrSourceCode The text to parse or a SourceCode object.
     * @param {ESLintConfig} providedConfig An ESLintConfig instance to configure everything.
     * @param {(string|Object)} [filenameOrOptions] The optional filename of the file being checked.
     *      If this is not set, the filename will default to '<input>' in the rule context. If
     *      an object, then it has "filename", "saveState", and "allowInlineConfig" properties.
     * @param {boolean} [filenameOrOptions.allowInlineConfig=true] Allow/disallow inline comments' ability to change config once it is set. Defaults to true if not supplied.
     *      Useful if you want to validate JS without comments overriding rules.
     * @param {boolean} [filenameOrOptions.reportUnusedDisableDirectives=false] Adds reported errors for unused
     *      eslint-disable directives
     * @returns {Object[]} The results as an array of messages or an empty array if no messages.
     */
    _verifyWithoutProcessors(textOrSourceCode, providedConfig, filenameOrOptions) {
        const config = providedConfig || {};
        const options = normalizeVerifyOptions(filenameOrOptions);
        let text;

        // evaluate arguments
        if (typeof textOrSourceCode === "string") {
            lastSourceCodes.set(this, null);
            text = textOrSourceCode;
        } else {
            lastSourceCodes.set(this, textOrSourceCode);
            text = textOrSourceCode.text;
        }

        // search and apply "eslint-env *".
        const envInFile = findEslintEnv(text);
        const resolvedEnvConfig = Object.assign({ builtin: true }, config.env, envInFile);
        const enabledEnvs = Object.keys(resolvedEnvConfig)
            .filter(envName => resolvedEnvConfig[envName])
            .map(envName => this.environments.get(envName))
            .filter(env => env);

        const parserOptions = resolveParserOptions(config.parserOptions || {}, enabledEnvs);
        const configuredGlobals = resolveGlobals(config.globals || {}, enabledEnvs);
        const parserName = config.parser || DEFAULT_PARSER_NAME;
        const settings = config.settings || {};

        if (!lastSourceCodes.get(this)) {

            // there's no input, just exit here
            if (text.trim().length === 0) {
                lastSourceCodes.set(this, new SourceCode(text, blankScriptAST));
                return [];
            }

            const parseResult = parse(
                text,
                parserOptions,
                parserName,
                loadedParserMaps.get(this),
                options.filename
            );

            if (!parseResult.success) {
                return [parseResult.error];
            }

            lastSourceCodes.set(this, parseResult.sourceCode);
        } else {

            /*
             * If the given source code object as the first argument does not have scopeManager, analyze the scope.
             * This is for backward compatibility (SourceCode is frozen so it cannot rebind).
             */
            const lastSourceCode = lastSourceCodes.get(this);

            if (!lastSourceCode.scopeManager) {
                lastSourceCodes.set(this, new SourceCode({
                    text: lastSourceCode.text,
                    ast: lastSourceCode.ast,
                    parserServices: lastSourceCode.parserServices,
                    visitorKeys: lastSourceCode.visitorKeys,
                    scopeManager: analyzeScope(lastSourceCode.ast, parserOptions)
                }));
            }
        }

        const sourceCode = lastSourceCodes.get(this);
        const commentDirectives = options.allowInlineConfig
            ? getDirectiveComments(options.filename, sourceCode.ast, ruleId => this.rules.get(ruleId))
            : { configuredRules: {}, enabledGlobals: {}, exportedVariables: {}, problems: [], disableDirectives: [] };

        // augment global scope with declared global variables
        addDeclaredGlobals(
            sourceCode.scopeManager.scopes[0],
            configuredGlobals,
            { exportedVariables: commentDirectives.exportedVariables, enabledGlobals: commentDirectives.enabledGlobals }
        );

        const configuredRules = Object.assign({}, config.rules, commentDirectives.configuredRules);

        const lintingProblems = runRules(
            sourceCode,
            configuredRules,
            ruleId => this.rules.get(ruleId),
            parserOptions,
            parserName,
            settings,
            options.filename
        );

        return applyDisableDirectives({
            directives: commentDirectives.disableDirectives,
            problems: lintingProblems
                .concat(commentDirectives.problems)
                .sort((problemA, problemB) => problemA.line - problemB.line || problemA.column - problemB.column),
            reportUnusedDisableDirectives: options.reportUnusedDisableDirectives
        });
    }

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
     * @param {function(string): string[]} [filenameOrOptions.preprocess] preprocessor for source text. If provided,
     *      this should accept a string of source text, and return an array of code blocks to lint.
     * @param {function(Array<Object[]>): Object[]} [filenameOrOptions.postprocess] postprocessor for report messages. If provided,
     *      this should accept an array of the message lists for each code block returned from the preprocessor,
     *      apply a mapping to the messages as appropriate, and return a one-dimensional array of messages
     * @returns {Object[]} The results as an array of messages or an empty array if no messages.
     */
    verify(textOrSourceCode, config, filenameOrOptions) {
        const preprocess = filenameOrOptions && filenameOrOptions.preprocess || (rawText => [rawText]);
        const postprocess = filenameOrOptions && filenameOrOptions.postprocess || lodash.flatten;

        return postprocess(
            preprocess(textOrSourceCode).map(
                textBlock => this._verifyWithoutProcessors(textBlock, config, filenameOrOptions)
            )
        );
    }

    /**
     * Gets the SourceCode object representing the parsed source.
     * @returns {SourceCode} The SourceCode object.
     */
    getSourceCode() {
        return lastSourceCodes.get(this);
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
     * Gets an object with all loaded rules.
     * @returns {Map} All loaded rules
     */
    getRules() {
        return this.rules.getAllLoadedRules();
    }

    /**
     * Define a new parser module
     * @param {any} parserId Name of the parser
     * @param {any} parserModule The parser object
     * @returns {void}
     */
    defineParser(parserId, parserModule) {
        loadedParserMaps.get(this).set(parserId, parserModule);
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
     * @param {Function} options.preprocess preprocessor for source text. If provided, this should
     *      accept a string of source text, and return an array of code blocks to lint.
     * @param {Function} options.postprocess postprocessor for report messages. If provided,
     *      this should accept an array of the message lists for each code block returned from the preprocessor,
     *      apply a mapping to the messages as appropriate, and return a one-dimensional array of messages
     * @returns {Object} The result of the fix operation as returned from the
     *      SourceCodeFixer.
     */
    verifyAndFix(text, config, options) {
        let messages = [],
            fixedResult,
            fixed = false,
            passNumber = 0,
            currentText = text;
        const debugTextDescription = options && options.filename || `${text.slice(0, 10)}...`;
        const shouldFix = options && typeof options.fix !== "undefined" ? options.fix : true;

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
            messages = this.verify(currentText, config, options);

            debug(`Generating fixed text for ${debugTextDescription} (pass ${passNumber})`);
            fixedResult = SourceCodeFixer.applyFixes(currentText, messages, shouldFix);

            /*
             * stop if there are any syntax errors.
             * 'fixedResult.output' is a empty string.
             */
            if (messages.length === 1 && messages[0].fatal) {
                break;
            }

            // keep track if any fixes were ever applied - important for return value
            fixed = fixed || fixedResult.fixed;

            // update to use the fixed output instead of the original text
            currentText = fixedResult.output;

        } while (
            fixedResult.fixed &&
            passNumber < MAX_AUTOFIX_PASSES
        );

        /*
         * If the last result had fixes, we need to lint again to be sure we have
         * the most up-to-date information.
         */
        if (fixedResult.fixed) {
            fixedResult.messages = this.verify(currentText, config, options);
        }

        // ensure the last result properly reflects if fixes were done
        fixedResult.fixed = fixed;
        fixedResult.output = currentText;

        return fixedResult;
    }
};
