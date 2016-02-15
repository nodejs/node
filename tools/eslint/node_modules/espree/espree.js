/**
 * @fileoverview Main Espree file that converts Acorn into Esprima output.
 * Copyright 2015 Nicholas C. Zakas. All rights reserved.
 *
 * This file contains code from the following MIT-licensed projects:
 * 1. Acorn
 * 2. Babylon
 * 3. Babel-ESLint
 *
 * This file also contains code from Esprima, which is BSD licensed.
 *
 * Acorn is Copyright 2012-2015 Acorn Contributors (https://github.com/marijnh/acorn/blob/master/AUTHORS)
 * Babylon is Copyright 2014-2015 various contributors (https://github.com/babel/babel/blob/master/packages/babylon/AUTHORS)
 * Babel-ESLint is Copyright 2014-2015 Sebastian McKenzie <sebmck@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Esprima is Copyright (c) jQuery Foundation, Inc. and Contributors, All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* eslint no-undefined:0, no-use-before-define: 0 */

"use strict";

var astNodeTypes = require("./lib/ast-node-types"),
    commentAttachment = require("./lib/comment-attachment"),
    TokenTranslator = require("./lib/token-translator"),
    acornJSX = require("acorn-jsx/inject"),
    rawAcorn = require("acorn");


var acorn = acornJSX(rawAcorn);

var lookahead,
    extra,
    lastToken;

/**
 * Resets the extra object to its default.
 * @returns {void}
 * @private
 */
function resetExtra() {
    extra = {
        tokens: null,
        range: false,
        loc: false,
        comment: false,
        comments: [],
        tolerant: false,
        errors: [],
        strict: false,
        ecmaFeatures: {},
        ecmaVersion: 5,
        isModule: false
    };
}



var tt = acorn.tokTypes,
    Parser = acorn.Parser,
    pp = Parser.prototype,
    getLineInfo = acorn.getLineInfo;

// custom type for JSX attribute values
tt.jsxAttrValueToken = {};

/**
 * Determines if a node is valid given the set of ecmaFeatures.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} True if the node is allowed, false if not.
 * @private
 */
function isValidNode(node) {
    var ecma = extra.ecmaFeatures;

    switch (node.type) {
        case "Identifier":
            return !extra.isModule || node.name !== "await";

        case "ExperimentalSpreadProperty":
        case "ExperimentalRestProperty":
            return ecma.experimentalObjectRestSpread;

        case "ImportDeclaration":
        case "ExportNamedDeclaration":
        case "ExportDefaultDeclaration":
        case "ExportAllDeclaration":
            return extra.isModule;

        default:
            return true;
    }
}

/**
 * Performs last-minute Esprima-specific compatibility checks and fixes.
 * @param {ASTNode} result The node to check.
 * @returns {ASTNode} The finished node.
 * @private
 * @this acorn.Parser
 */
function esprimaFinishNode(result) {
    // ensure that parsed node was allowed through ecmaFeatures
    if (!isValidNode(result)) {
        this.unexpected(result.start);
    }

    // https://github.com/marijnh/acorn/issues/323
    if (result.type === "TryStatement") {
        delete result.guardedHandlers;
    } else if (result.type === "CatchClause") {
        delete result.guard;
    }

    // Acorn doesn't count the opening and closing backticks as part of templates
    // so we have to adjust ranges/locations appropriately.
    if (result.type === "TemplateElement") {

        // additional adjustment needed if ${ is the last token
        var terminalDollarBraceL = this.input.slice(result.end, result.end + 2) === "${";

        if (result.range) {
            result.range[0]--;
            result.range[1] += (terminalDollarBraceL ? 2 : 1);
        }

        if (result.loc) {
            result.loc.start.column--;
            result.loc.end.column += (terminalDollarBraceL ? 2 : 1);
        }
    }

    // Acorn currently uses expressions instead of declarations in default exports
    if (result.type === "ExportDefaultDeclaration") {
        if (/^(Class|Function)Expression$/.test(result.declaration.type)) {
            result.declaration.type = result.declaration.type.replace("Expression", "Declaration");
        }
    }

    // Acorn uses undefined instead of null, which affects serialization
    if (result.type === "Literal" && result.value === undefined) {
        result.value = null;
    }

    if (extra.attachComment) {
        commentAttachment.processComment(result);
    }

    if (result.type.indexOf("Function") > -1 && !result.generator) {
        result.generator = false;
    }

    return result;
}

/**
 * Determines if a token is valid given the set of ecmaFeatures.
 * @param {acorn.Parser} parser The parser to check.
 * @returns {boolean} True if the token is allowed, false if not.
 * @private
 */
function isValidToken(parser) {
    var ecma = extra.ecmaFeatures;
    var type = parser.type;

    switch (type) {
        case tt.jsxName:
        case tt.jsxText:
        case tt.jsxTagStart:
        case tt.jsxTagEnd:
            return ecma.jsx;

        // https://github.com/ternjs/acorn/issues/363
        case tt.regexp:
            if (extra.ecmaVersion < 6 && parser.value.flags && parser.value.flags.indexOf("y") > -1) {
                return false;
            }

            return true;

        default:
            return true;
    }
}

/**
 * Injects esprimaFinishNode into the finishNode process.
 * @param {Function} finishNode Original finishNode function.
 * @returns {ASTNode} The finished node.
 * @private
 */
function wrapFinishNode(finishNode) {
    return /** @this acorn.Parser */ function(node, type, pos, loc) {
        var result = finishNode.call(this, node, type, pos, loc);
        return esprimaFinishNode.call(this, result);
    };
}

pp.extend("finishNode", wrapFinishNode);

pp.extend("finishNodeAt", wrapFinishNode);

pp.extend("next", function(next) {
    return /** @this acorn.Parser */ function() {
        if (!isValidToken(this)) {
            this.unexpected();
        }
        return next.call(this);
    };
});

// needed for experimental object rest/spread
pp.extend("checkLVal", function(checkLVal) {

    return /** @this acorn.Parser */ function(expr, isBinding, checkClashes) {

        if (extra.ecmaFeatures.experimentalObjectRestSpread && expr.type === "ObjectPattern") {
            for (var i = 0; i < expr.properties.length; i++) {
                if (expr.properties[i].type.indexOf("Experimental") === -1) {
                    this.checkLVal(expr.properties[i].value, isBinding, checkClashes);
                }
            }
            return undefined;
        }

        return checkLVal.call(this, expr, isBinding, checkClashes);
    };
});

pp.extend("parseTopLevel", function(parseTopLevel) {
    return /** @this acorn.Parser */ function(node) {
        if (extra.ecmaFeatures.impliedStrict && this.options.ecmaVersion >= 5) {
            this.strict = true;
        }
        return parseTopLevel.call(this, node);
    };
});

pp.extend("toAssignable", function(toAssignable) {

    return /** @this acorn.Parser */ function(node, isBinding) {

        if (extra.ecmaFeatures.experimentalObjectRestSpread &&
                node.type === "ObjectExpression"
        ) {
            node.type = "ObjectPattern";

            for (var i = 0; i < node.properties.length; i++) {
                var prop = node.properties[i];

                if (prop.type === "ExperimentalSpreadProperty") {
                    prop.type = "ExperimentalRestProperty";
                } else if (prop.kind !== "init") {
                    this.raise(prop.key.start, "Object pattern can't contain getter or setter");
                } else {
                    this.toAssignable(prop.value, isBinding);
                }
            }

            return node;
        } else {
            return toAssignable.call(this, node, isBinding);
        }
    };

});

/**
 * Method to parse an object rest or object spread.
 * @returns {ASTNode} The node representing object rest or object spread.
 * @this acorn.Parser
 */
pp.parseObjectRest = function() {
    var node = this.startNode();
    this.next();
    node.argument = this.parseIdent();
    return this.finishNode(node, "ExperimentalRestProperty");
};

/**
 * Method to parse an object with object rest or object spread.
 * @param {boolean} isPattern True if the object is a destructuring pattern.
 * @param {Object} refShorthandDefaultPos ?
 * @returns {ASTNode} The node representing object rest or object spread.
 * @this acorn.Parser
 */
pp.parseObj = function(isPattern, refShorthandDefaultPos) {
    var node = this.startNode(),
        first = true,
        propHash = {};
    node.properties = [];
    this.next();
    while (!this.eat(tt.braceR)) {

        if (!first) {
            this.expect(tt.comma);

            if (this.afterTrailingComma(tt.braceR)) {
                break;
            }

        } else {
            first = false;
        }

        var prop = this.startNode(),
            isGenerator,
            startPos,
            startLoc;

        if (extra.ecmaFeatures.experimentalObjectRestSpread && this.type === tt.ellipsis) {
            if (isPattern) {
                prop = this.parseObjectRest();
            } else {
                prop = this.parseSpread();
                prop.type = "ExperimentalSpreadProperty";
            }

            node.properties.push(prop);
            continue;
        }

        if (this.options.ecmaVersion >= 6) {
            prop.method = false;
            prop.shorthand = false;

            if (isPattern || refShorthandDefaultPos) {
                startPos = this.start;
                startLoc = this.startLoc;
            }

            if (!isPattern) {
                isGenerator = this.eat(tt.star);
            }
        }

        this.parsePropertyName(prop);
        this.parsePropertyValue(prop, isPattern, isGenerator, startPos, startLoc, refShorthandDefaultPos);
        this.checkPropClash(prop, propHash);
        node.properties.push(this.finishNode(prop, "Property"));
    }

    return this.finishNode(node, isPattern ? "ObjectPattern" : "ObjectExpression");
};

/**
 * Overwrites the default raise method to throw Esprima-style errors.
 * @param {int} pos The position of the error.
 * @param {string} message The error message.
 * @throws {SyntaxError} A syntax error.
 * @returns {void}
 */
pp.raise = function(pos, message) {
    var loc = getLineInfo(this.input, pos);
    var err = new SyntaxError(message);
    err.index = pos;
    err.lineNumber = loc.line;
    err.column = loc.column + 1; // acorn uses 0-based columns
    throw err;
};

/**
 * Overwrites the default unexpected method to throw Esprima-style errors.
 * @param {int} pos The position of the error.
 * @throws {SyntaxError} A syntax error.
 * @returns {void}
 */
pp.unexpected = function(pos) {
    var message = "Unexpected token";

    if (pos !== null && pos !== undefined) {
        this.pos = pos;

        if (this.options.locations) {
            while (this.pos < this.lineStart) {
                this.lineStart = this.input.lastIndexOf("\n", this.lineStart - 2) + 1;
                --this.curLine;
            }
        }

        this.nextToken();
    }

    if (this.end > this.start) {
        message += " " + this.input.slice(this.start, this.end);
    }

    this.raise(this.start, message);
};

/*
 * Esprima-FB represents JSX strings as tokens called "JSXText", but Acorn-JSX
 * uses regular tt.string without any distinction between this and regular JS
 * strings. As such, we intercept an attempt to read a JSX string and set a flag
 * on extra so that when tokens are converted, the next token will be switched
 * to JSXText via onToken.
 */
pp.extend("jsx_readString", function(jsxReadString) {
    return /** @this acorn.Parser */ function(quote) {
        var result = jsxReadString.call(this, quote);
        if (this.type === tt.string) {
            extra.jsxAttrValueToken = true;
        }

        return result;
    };
});

//------------------------------------------------------------------------------
// Tokenizer
//------------------------------------------------------------------------------

/**
 * Tokenizes the given code.
 * @param {string} code The code to tokenize.
 * @param {Object} options Options defining how to tokenize.
 * @returns {Token[]} An array of tokens.
 * @throws {SyntaxError} If the input code is invalid.
 * @private
 */
function tokenize(code, options) {
    var toString,
        tokens,
        impliedStrict,
        translator = new TokenTranslator(tt, code);

    toString = String;
    if (typeof code !== "string" && !(code instanceof String)) {
        code = toString(code);
    }

    lookahead = null;

    // Options matching.
    options = options || {};

    var acornOptions = {
        ecmaVersion: 5
    };

    resetExtra();

    // Of course we collect tokens here.
    options.tokens = true;
    extra.tokens = [];

    extra.range = (typeof options.range === "boolean") && options.range;
    acornOptions.ranges = extra.range;

    extra.loc = (typeof options.loc === "boolean") && options.loc;
    acornOptions.locations = extra.loc;

    extra.comment = typeof options.comment === "boolean" && options.comment;

    if (extra.comment) {
        acornOptions.onComment = function() {
            var comment = convertAcornCommentToEsprimaComment.apply(this, arguments);
            extra.comments.push(comment);
        };
    }

    extra.tolerant = typeof options.tolerant === "boolean" && options.tolerant;

    if (typeof options.ecmaVersion === "number") {
        switch (options.ecmaVersion) {
            case 3:
            case 5:
            case 6:
                acornOptions.ecmaVersion = options.ecmaVersion;
                extra.ecmaVersion = options.ecmaVersion;
                break;

            default:
                throw new Error("ecmaVersion must be 3, 5, or 6.");
        }
    }

    // apply parsing flags
    if (options.ecmaFeatures && typeof options.ecmaFeatures === "object") {
        extra.ecmaFeatures = options.ecmaFeatures;
        impliedStrict = extra.ecmaFeatures.impliedStrict;
        extra.ecmaFeatures.impliedStrict = typeof impliedStrict === "boolean" && impliedStrict;
    }

    try {
        var tokenizer = acorn.tokenizer(code, acornOptions);
        while ((lookahead = tokenizer.getToken()).type !== tt.eof) {
            translator.onToken(lookahead, extra);
        }

        // filterTokenLocation();
        tokens = extra.tokens;

        if (extra.comment) {
            tokens.comments = extra.comments;
        }
        if (extra.tolerant) {
            tokens.errors = extra.errors;
        }
    } catch (e) {
        throw e;
    }
    return tokens;
}

//------------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------------



/**
 * Converts an Acorn comment to a Esprima comment.
 * @param {boolean} block True if it's a block comment, false if not.
 * @param {string} text The text of the comment.
 * @param {int} start The index at which the comment starts.
 * @param {int} end The index at which the comment ends.
 * @param {Location} startLoc The location at which the comment starts.
 * @param {Location} endLoc The location at which the comment ends.
 * @returns {Object} The comment object.
 * @private
 */
function convertAcornCommentToEsprimaComment(block, text, start, end, startLoc, endLoc) {
    var comment = {
        type: block ? "Block" : "Line",
        value: text
    };

    if (typeof start === "number") {
        comment.start = start;
        comment.end = end;
        comment.range = [start, end];
    }

    if (typeof startLoc === "object") {
        comment.loc = {
            start: startLoc,
            end: endLoc
        };
    }

    return comment;
}

/**
 * Parses the given code.
 * @param {string} code The code to tokenize.
 * @param {Object} options Options defining how to tokenize.
 * @returns {ASTNode} The "Program" AST node.
 * @throws {SyntaxError} If the input code is invalid.
 * @private
 */
function parse(code, options) {
    var program,
        toString = String,
        translator,
        impliedStrict,
        acornOptions = {
            ecmaVersion: 5
        };

    lastToken = null;

    if (typeof code !== "string" && !(code instanceof String)) {
        code = toString(code);
    }

    resetExtra();
    commentAttachment.reset();

    if (typeof options !== "undefined") {
        extra.range = (typeof options.range === "boolean") && options.range;
        extra.loc = (typeof options.loc === "boolean") && options.loc;
        extra.attachComment = (typeof options.attachComment === "boolean") && options.attachComment;

        if (extra.loc && options.source !== null && options.source !== undefined) {
            extra.source = toString(options.source);
        }

        if (typeof options.tokens === "boolean" && options.tokens) {
            extra.tokens = [];
            translator = new TokenTranslator(tt, code);
        }
        if (typeof options.comment === "boolean" && options.comment) {
            extra.comment = true;
            extra.comments = [];
        }
        if (typeof options.tolerant === "boolean" && options.tolerant) {
            extra.errors = [];
        }
        if (extra.attachComment) {
            extra.range = true;
            extra.comments = [];
            commentAttachment.reset();
        }

        if (typeof options.ecmaVersion === "number") {
            switch (options.ecmaVersion) {
                case 3:
                case 5:
                case 6:
                    acornOptions.ecmaVersion = options.ecmaVersion;
                    extra.ecmaVersion = options.ecmaVersion;
                    break;

                default:
                    throw new Error("ecmaVersion must be 3, 5, or 6.");
            }
        }

        if (options.sourceType === "module") {
            extra.isModule = true;

            // modules must be in 6 at least
            if (acornOptions.ecmaVersion < 6) {
                acornOptions.ecmaVersion = 6;
                extra.ecmaVersion = 6;
            }

            acornOptions.sourceType = "module";
        }

        // apply parsing flags after sourceType to allow overriding
        if (options.ecmaFeatures && typeof options.ecmaFeatures === "object") {
            extra.ecmaFeatures = options.ecmaFeatures;
            impliedStrict = extra.ecmaFeatures.impliedStrict;
            extra.ecmaFeatures.impliedStrict = typeof impliedStrict === "boolean" && impliedStrict;
            if (options.ecmaFeatures.globalReturn) {
                acornOptions.allowReturnOutsideFunction = true;
            }
        }


        acornOptions.onToken = function(token) {
            if (extra.tokens) {
                translator.onToken(token, extra);
            }
            if (token.type !== tt.eof) {
                lastToken = token;
            }
        };

        if (extra.attachComment || extra.comment) {
            acornOptions.onComment = function() {
                var comment = convertAcornCommentToEsprimaComment.apply(this, arguments);
                extra.comments.push(comment);

                if (extra.attachComment) {
                    commentAttachment.addComment(comment);
                }
            };
        }

        if (extra.range) {
            acornOptions.ranges = true;
        }

        if (extra.loc) {
            acornOptions.locations = true;
        }

        if (extra.ecmaFeatures.jsx) {
            acornOptions.plugins = { jsx: true };
        }
    }

    program = acorn.parse(code, acornOptions);
    program.sourceType = extra.isModule ? "module" : "script";

    if (extra.comment || extra.attachComment) {
        program.comments = extra.comments;
    }

    if (extra.tokens) {
        program.tokens = extra.tokens;
    }

    /*
     * Adjust opening and closing position of program to match Esprima.
     * Acorn always starts programs at range 0 whereas Esprima starts at the
     * first AST node's start (the only real difference is when there's leading
     * whitespace or leading comments). Acorn also counts trailing whitespace
     * as part of the program whereas Esprima only counts up to the last token.
     */
    if (program.range) {
        program.range[0] = program.body.length ? program.body[0].range[0] : program.range[0];
        program.range[1] = lastToken ? lastToken.range[1] : program.range[1];
    }

    if (program.loc) {
        program.loc.start = program.body.length ? program.body[0].loc.start : program.loc.start;
        program.loc.end = lastToken ? lastToken.loc.end : program.loc.end;
    }

    return program;
}

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

exports.version = require("./package.json").version;

exports.tokenize = tokenize;

exports.parse = parse;

// Deep copy.
/* istanbul ignore next */
exports.Syntax = (function() {
    var name, types = {};

    if (typeof Object.create === "function") {
        types = Object.create(null);
    }

    for (name in astNodeTypes) {
        if (astNodeTypes.hasOwnProperty(name)) {
            types[name] = astNodeTypes[name];
        }
    }

    if (typeof Object.freeze === "function") {
        Object.freeze(types);
    }

    return types;
}());

/* istanbul ignore next */
exports.VisitorKeys = (function() {
    var visitorKeys = require("./lib/visitor-keys");
    var name,
        keys = {};

    if (typeof Object.create === "function") {
        keys = Object.create(null);
    }

    for (name in visitorKeys) {
        if (visitorKeys.hasOwnProperty(name)) {
            keys[name] = visitorKeys[name];
        }
    }

    if (typeof Object.freeze === "function") {
        Object.freeze(keys);
    }

    return keys;
}());
