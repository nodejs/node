/**
 * @fileoverview RuleContext utility for rules
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const assert = require("assert");
const ruleFixer = require("./util/rule-fixer");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

const PASSTHROUGHS = [
    "getAncestors",
    "getDeclaredVariables",
    "getFilename",
    "getScope",
    "getSourceCode",
    "markVariableAsUsed",

    // DEPRECATED
    "getAllComments",
    "getComments",
    "getFirstToken",
    "getFirstTokens",
    "getJSDocComment",
    "getLastToken",
    "getLastTokens",
    "getNodeByRangeIndex",
    "getSource",
    "getSourceLines",
    "getTokenAfter",
    "getTokenBefore",
    "getTokenByRangeStart",
    "getTokens",
    "getTokensAfter",
    "getTokensBefore",
    "getTokensBetween"
];

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

/**
 * An error message description
 * @typedef {Object} MessageDescriptor
 * @property {string} nodeType The type of node.
 * @property {Location} loc The location of the problem.
 * @property {string} message The problem message.
 * @property {Object} [data] Optional data to use to fill in placeholders in the
 *      message.
 * @property {Function} fix The function to call that creates a fix command.
 */

//------------------------------------------------------------------------------
// Module Definition
//------------------------------------------------------------------------------

/**
 * Compares items in a fixes array by range.
 * @param {Fix} a The first message.
 * @param {Fix} b The second message.
 * @returns {int} -1 if a comes before b, 1 if a comes after b, 0 if equal.
 * @private
 */
function compareFixesByRange(a, b) {
    return a.range[0] - b.range[0] || a.range[1] - b.range[1];
}

/**
 * Merges the given fixes array into one.
 * @param {Fix[]} fixes The fixes to merge.
 * @param {SourceCode} sourceCode The source code object to get the text between fixes.
 * @returns {void}
 */
function mergeFixes(fixes, sourceCode) {
    if (fixes.length === 0) {
        return null;
    }
    if (fixes.length === 1) {
        return fixes[0];
    }

    fixes.sort(compareFixesByRange);

    const originalText = sourceCode.text;
    const start = fixes[0].range[0];
    const end = fixes[fixes.length - 1].range[1];
    let text = "";
    let lastPos = Number.MIN_SAFE_INTEGER;

    for (const fix of fixes) {
        assert(fix.range[0] >= lastPos, "Fix objects must not be overlapped in a report.");

        if (fix.range[0] >= 0) {
            text += originalText.slice(Math.max(0, start, lastPos), fix.range[0]);
        }
        text += fix.text;
        lastPos = fix.range[1];
    }
    text += originalText.slice(Math.max(0, start, lastPos), end);

    return { range: [start, end], text };
}

/**
 * Gets one fix object from the given descriptor.
 * If the descriptor retrieves multiple fixes, this merges those to one.
 * @param {Object} descriptor The report descriptor.
 * @param {SourceCode} sourceCode The source code object to get text between fixes.
 * @returns {Fix} The got fix object.
 */
function getFix(descriptor, sourceCode) {
    if (typeof descriptor.fix !== "function") {
        return null;
    }

    // @type {null | Fix | Fix[] | IterableIterator<Fix>}
    const fix = descriptor.fix(ruleFixer);

    // Merge to one.
    if (fix && Symbol.iterator in fix) {
        return mergeFixes(Array.from(fix), sourceCode);
    }
    return fix;
}

/**
 * Rule context class
 * Acts as an abstraction layer between rules and the main linter object.
 */
class RuleContext {

    /**
     * @param {string} ruleId The ID of the rule using this object.
     * @param {Linter} linter The linter object.
     * @param {number} severity The configured severity level of the rule.
     * @param {Array} options The configuration information to be added to the rule.
     * @param {Object} settings The configuration settings passed from the config file.
     * @param {Object} parserOptions The parserOptions settings passed from the config file.
     * @param {Object} parserPath The parser setting passed from the config file.
     * @param {Object} meta The metadata of the rule
     * @param {Object} parserServices The parser services for the rule.
     */
    constructor(ruleId, linter, severity, options, settings, parserOptions, parserPath, meta, parserServices) {

        // public.
        this.id = ruleId;
        this.options = options;
        this.settings = settings;
        this.parserOptions = parserOptions;
        this.parserPath = parserPath;
        this.meta = meta;

        // create a separate copy and freeze it (it's not nice to freeze other people's objects)
        this.parserServices = Object.freeze(Object.assign({}, parserServices));

        // private.
        this._linter = linter;
        this._severity = severity;

        Object.freeze(this);
    }

    /**
     * Passthrough to Linter#report() that automatically assigns the rule ID and severity.
     * @param {ASTNode|MessageDescriptor} nodeOrDescriptor The AST node related to the message or a message
     *      descriptor.
     * @param {Object=} location The location of the error.
     * @param {string} message The message to display to the user.
     * @param {Object} opts Optional template data which produces a formatted message
     *     with symbols being replaced by this object's values.
     * @returns {void}
     */
    report(nodeOrDescriptor, location, message, opts) {

        // check to see if it's a new style call
        if (arguments.length === 1) {
            const descriptor = nodeOrDescriptor;
            const fix = getFix(descriptor, this.getSourceCode());

            if (descriptor.loc) {
                this._linter.report(
                    this.id,
                    this._severity,
                    descriptor.node,
                    descriptor.loc,
                    descriptor.message,
                    descriptor.data,
                    fix,
                    this.meta
                );
            } else {
                this._linter.report(
                    this.id,
                    this._severity,
                    descriptor.node,

                    /* loc not provided */
                    descriptor.message,
                    descriptor.data,
                    fix,
                    this.meta
                );
            }

        } else {

            // old style call
            this._linter.report(
                this.id,
                this._severity,
                nodeOrDescriptor,
                location,
                message,
                opts,
                this.meta
            );
        }
    }
}

// Copy over passthrough methods.
PASSTHROUGHS.forEach(name => {
    Object.defineProperty(RuleContext.prototype, name, {
        value() {
            return this._linter[name].apply(this._linter, arguments);
        },
        configurable: true,
        writable: true,
        enumerable: false
    });
});

module.exports = RuleContext;
