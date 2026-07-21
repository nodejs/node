"use strict";
var __assign = (this && this.__assign) || function () {
    __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
                t[p] = s[p];
        }
        return t;
    };
    return __assign.apply(this, arguments);
};
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __read = (this && this.__read) || function (o, n) {
    var m = typeof Symbol === "function" && o[Symbol.iterator];
    if (!m) return o;
    var i = m.call(o), r, ar = [], e;
    try {
        while ((n === void 0 || n-- > 0) && !(r = i.next()).done) ar.push(r.value);
    }
    catch (error) { e = { error: error }; }
    finally {
        try {
            if (r && !r.done && (m = i["return"])) m.call(i);
        }
        finally { if (e) throw e.error; }
    }
    return ar;
};
var __spreadArray = (this && this.__spreadArray) || function (to, from, pack) {
    if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
        if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
        }
    }
    return to.concat(ar || Array.prototype.slice.call(from));
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var _a, _b;
Object.defineProperty(exports, "__esModule", { value: true });
var root_1 = __importDefault(require("./selectors/root"));
var selector_1 = __importDefault(require("./selectors/selector"));
var className_1 = __importDefault(require("./selectors/className"));
var comment_1 = __importDefault(require("./selectors/comment"));
var id_1 = __importDefault(require("./selectors/id"));
var tag_1 = __importDefault(require("./selectors/tag"));
var string_1 = __importDefault(require("./selectors/string"));
var pseudo_1 = __importDefault(require("./selectors/pseudo"));
var attribute_1 = __importStar(require("./selectors/attribute"));
var universal_1 = __importDefault(require("./selectors/universal"));
var combinator_1 = __importDefault(require("./selectors/combinator"));
var nesting_1 = __importDefault(require("./selectors/nesting"));
var sortAscending_1 = __importDefault(require("./sortAscending"));
var tokenize_1 = __importStar(require("./tokenize"));
var tokens = __importStar(require("./tokenTypes"));
var types = __importStar(require("./selectors/types"));
var util_1 = require("./util");
var WHITESPACE_TOKENS = (_a = {},
    _a[tokens.space] = true,
    _a[tokens.cr] = true,
    _a[tokens.feed] = true,
    _a[tokens.newline] = true,
    _a[tokens.tab] = true,
    _a);
var WHITESPACE_EQUIV_TOKENS = __assign(__assign({}, WHITESPACE_TOKENS), (_b = {}, _b[tokens.comment] = true, _b));
function tokenStart(token) {
    return {
        line: token[tokenize_1.FIELDS.START_LINE],
        column: token[tokenize_1.FIELDS.START_COL],
    };
}
function tokenEnd(token) {
    return {
        line: token[tokenize_1.FIELDS.END_LINE],
        column: token[tokenize_1.FIELDS.END_COL],
    };
}
function getSource(startLine, startColumn, endLine, endColumn) {
    return {
        start: {
            line: startLine,
            column: startColumn,
        },
        end: {
            line: endLine,
            column: endColumn,
        },
    };
}
function getTokenSource(token) {
    return getSource(token[tokenize_1.FIELDS.START_LINE], token[tokenize_1.FIELDS.START_COL], token[tokenize_1.FIELDS.END_LINE], token[tokenize_1.FIELDS.END_COL]);
}
function getTokenSourceSpan(startToken, endToken) {
    if (!startToken) {
        return undefined;
    }
    return getSource(startToken[tokenize_1.FIELDS.START_LINE], startToken[tokenize_1.FIELDS.START_COL], endToken[tokenize_1.FIELDS.END_LINE], endToken[tokenize_1.FIELDS.END_COL]);
}
function unescapeProp(node, prop) {
    var value = node[prop];
    if (typeof value !== "string") {
        return;
    }
    if (value.indexOf("\\") !== -1) {
        (0, util_1.ensureObject)(node, "raws");
        node[prop] = (0, util_1.unesc)(value);
        if (node.raws[prop] === undefined) {
            node.raws[prop] = value;
        }
    }
    return node;
}
function indexesOf(array, item) {
    var i = -1;
    var indexes = [];
    while ((i = array.indexOf(item, i + 1)) !== -1) {
        indexes.push(i);
    }
    return indexes;
}
function uniqs() {
    var list = Array.prototype.concat.apply([], arguments);
    return list.filter(function (item, i) { return i === list.indexOf(item); });
}
var Parser = /** @class */ (function () {
    function Parser(rule, options) {
        if (options === void 0) { options = {}; }
        this.rule = rule;
        this.options = Object.assign({ lossy: false, safe: false }, options);
        this.position = 0;
        this.nestingDepth = 0;
        this.maxNestingDepth = (0, util_1.resolveMaxNestingDepth)(this.options.maxNestingDepth);
        this.css = typeof this.rule === "string" ? this.rule : this.rule.selector;
        this.tokens = (0, tokenize_1.default)({
            css: this.css,
            error: this._errorGenerator(),
            safe: this.options.safe,
        });
        var rootSource = getTokenSourceSpan(this.tokens[0], this.tokens[this.tokens.length - 1]);
        this.root = new root_1.default({ source: rootSource });
        this.root.errorGenerator = this._errorGenerator();
        var selector = new selector_1.default({
            source: { start: { line: 1, column: 1 } },
            sourceIndex: 0,
        });
        this.root.append(selector);
        this.current = selector;
        this.loop();
    }
    Parser.prototype._errorGenerator = function () {
        var _this = this;
        return function (message, errorOptions) {
            if (typeof _this.rule === "string") {
                return new Error(message);
            }
            return _this.rule.error(message, errorOptions);
        };
    };
    Parser.prototype.attribute = function () {
        var attr = [];
        var startingToken = this.currToken;
        this.position++;
        while (this.position < this.tokens.length &&
            this.currToken[tokenize_1.FIELDS.TYPE] !== tokens.closeSquare) {
            attr.push(this.currToken);
            this.position++;
        }
        if (this.currToken[tokenize_1.FIELDS.TYPE] !== tokens.closeSquare) {
            return this.expected("closing square bracket", this.currToken[tokenize_1.FIELDS.START_POS]);
        }
        var len = attr.length;
        var node = {
            source: getSource(startingToken[1], startingToken[2], this.currToken[3], this.currToken[4]),
            sourceIndex: startingToken[tokenize_1.FIELDS.START_POS],
        };
        if (len === 1 && !~[tokens.word].indexOf(attr[0][tokenize_1.FIELDS.TYPE])) {
            return this.expected("attribute", attr[0][tokenize_1.FIELDS.START_POS]);
        }
        var pos = 0;
        var spaceBefore = "";
        var commentBefore = "";
        var lastAdded = null;
        var spaceAfterMeaningfulToken = false;
        while (pos < len) {
            var token = attr[pos];
            var content = this.content(token);
            var next = attr[pos + 1];
            switch (token[tokenize_1.FIELDS.TYPE]) {
                case tokens.space:
                    // if (
                    //     len === 1 ||
                    //     pos === 0 && this.content(next) === '|'
                    // ) {
                    //     return this.expected('attribute', token[TOKEN.START_POS], content);
                    // }
                    spaceAfterMeaningfulToken = true;
                    if (this.options.lossy) {
                        break;
                    }
                    if (lastAdded) {
                        (0, util_1.ensureObject)(node, "spaces", lastAdded);
                        var prevContent = node.spaces[lastAdded].after || "";
                        node.spaces[lastAdded].after = prevContent + content;
                        var existingComment = (0, util_1.getProp)(node, "raws", "spaces", lastAdded, "after") || null;
                        if (existingComment) {
                            node.raws.spaces[lastAdded].after = existingComment + content;
                        }
                    }
                    else {
                        spaceBefore = spaceBefore + content;
                        commentBefore = commentBefore + content;
                    }
                    break;
                case tokens.asterisk:
                    if (next[tokenize_1.FIELDS.TYPE] === tokens.equals) {
                        node.operator = content;
                        lastAdded = "operator";
                    }
                    else if ((!node.namespace || (lastAdded === "namespace" && !spaceAfterMeaningfulToken)) &&
                        next) {
                        if (spaceBefore) {
                            (0, util_1.ensureObject)(node, "spaces", "attribute");
                            node.spaces.attribute.before = spaceBefore;
                            spaceBefore = "";
                        }
                        if (commentBefore) {
                            (0, util_1.ensureObject)(node, "raws", "spaces", "attribute");
                            node.raws.spaces.attribute.before = spaceBefore;
                            commentBefore = "";
                        }
                        node.namespace = (node.namespace || "") + content;
                        var rawValue = (0, util_1.getProp)(node, "raws", "namespace") || null;
                        if (rawValue) {
                            node.raws.namespace += content;
                        }
                        lastAdded = "namespace";
                    }
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.dollar:
                    if (lastAdded === "value") {
                        var oldRawValue = (0, util_1.getProp)(node, "raws", "value");
                        node.value += "$";
                        if (oldRawValue) {
                            node.raws.value = oldRawValue + "$";
                        }
                        break;
                    }
                // Falls through
                case tokens.caret:
                    if (next[tokenize_1.FIELDS.TYPE] === tokens.equals) {
                        node.operator = content;
                        lastAdded = "operator";
                    }
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.combinator:
                    if (content === "~" && next[tokenize_1.FIELDS.TYPE] === tokens.equals) {
                        node.operator = content;
                        lastAdded = "operator";
                    }
                    if (content !== "|") {
                        spaceAfterMeaningfulToken = false;
                        break;
                    }
                    if (next[tokenize_1.FIELDS.TYPE] === tokens.equals) {
                        node.operator = content;
                        lastAdded = "operator";
                    }
                    else if (!node.namespace && !node.attribute) {
                        node.namespace = true;
                    }
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.word:
                    if (next &&
                        this.content(next) === "|" &&
                        attr[pos + 2] &&
                        attr[pos + 2][tokenize_1.FIELDS.TYPE] !== tokens.equals && // this look-ahead probably fails with comment nodes involved.
                        !node.operator &&
                        !node.namespace) {
                        node.namespace = content;
                        lastAdded = "namespace";
                    }
                    else if (!node.attribute || (lastAdded === "attribute" && !spaceAfterMeaningfulToken)) {
                        if (spaceBefore) {
                            (0, util_1.ensureObject)(node, "spaces", "attribute");
                            node.spaces.attribute.before = spaceBefore;
                            spaceBefore = "";
                        }
                        if (commentBefore) {
                            (0, util_1.ensureObject)(node, "raws", "spaces", "attribute");
                            node.raws.spaces.attribute.before = commentBefore;
                            commentBefore = "";
                        }
                        node.attribute = (node.attribute || "") + content;
                        var rawValue = (0, util_1.getProp)(node, "raws", "attribute") || null;
                        if (rawValue) {
                            node.raws.attribute += content;
                        }
                        lastAdded = "attribute";
                    }
                    else if ((!node.value && node.value !== "") ||
                        (lastAdded === "value" && !(spaceAfterMeaningfulToken || node.quoteMark))) {
                        var unescaped_1 = (0, util_1.unesc)(content);
                        var oldRawValue = (0, util_1.getProp)(node, "raws", "value") || "";
                        var oldValue = node.value || "";
                        node.value = oldValue + unescaped_1;
                        node.quoteMark = null;
                        if (unescaped_1 !== content || oldRawValue) {
                            (0, util_1.ensureObject)(node, "raws");
                            node.raws.value = (oldRawValue || oldValue) + content;
                        }
                        lastAdded = "value";
                    }
                    else {
                        var insensitive = content === "i" || content === "I";
                        if ((node.value || node.value === "") &&
                            (node.quoteMark || spaceAfterMeaningfulToken)) {
                            node.insensitive = insensitive;
                            if (!insensitive || content === "I") {
                                (0, util_1.ensureObject)(node, "raws");
                                node.raws.insensitiveFlag = content;
                            }
                            lastAdded = "insensitive";
                            if (spaceBefore) {
                                (0, util_1.ensureObject)(node, "spaces", "insensitive");
                                node.spaces.insensitive.before = spaceBefore;
                                spaceBefore = "";
                            }
                            if (commentBefore) {
                                (0, util_1.ensureObject)(node, "raws", "spaces", "insensitive");
                                node.raws.spaces.insensitive.before = commentBefore;
                                commentBefore = "";
                            }
                        }
                        else if (node.value || node.value === "") {
                            lastAdded = "value";
                            node.value += content;
                            if (node.raws.value) {
                                node.raws.value += content;
                            }
                        }
                    }
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.str:
                    if (!node.attribute || !node.operator) {
                        return this.error("Expected an attribute followed by an operator preceding the string.", {
                            index: token[tokenize_1.FIELDS.START_POS],
                        });
                    }
                    var _a = (0, attribute_1.unescapeValue)(content), unescaped = _a.unescaped, quoteMark = _a.quoteMark;
                    node.value = unescaped;
                    node.quoteMark = quoteMark;
                    lastAdded = "value";
                    (0, util_1.ensureObject)(node, "raws");
                    node.raws.value = content;
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.equals:
                    if (!node.attribute) {
                        return this.expected("attribute", token[tokenize_1.FIELDS.START_POS], content);
                    }
                    if (node.value) {
                        return this.error('Unexpected "=" found; an operator was already defined.', {
                            index: token[tokenize_1.FIELDS.START_POS],
                        });
                    }
                    node.operator = node.operator ? node.operator + content : content;
                    lastAdded = "operator";
                    spaceAfterMeaningfulToken = false;
                    break;
                case tokens.comment:
                    if (lastAdded) {
                        if (spaceAfterMeaningfulToken ||
                            (next && next[tokenize_1.FIELDS.TYPE] === tokens.space) ||
                            lastAdded === "insensitive") {
                            var lastComment = (0, util_1.getProp)(node, "spaces", lastAdded, "after") || "";
                            var rawLastComment = (0, util_1.getProp)(node, "raws", "spaces", lastAdded, "after") || lastComment;
                            (0, util_1.ensureObject)(node, "raws", "spaces", lastAdded);
                            node.raws.spaces[lastAdded].after = rawLastComment + content;
                        }
                        else {
                            var lastValue = node[lastAdded] || "";
                            var rawLastValue = (0, util_1.getProp)(node, "raws", lastAdded) || lastValue;
                            (0, util_1.ensureObject)(node, "raws");
                            node.raws[lastAdded] = rawLastValue + content;
                        }
                    }
                    else {
                        commentBefore = commentBefore + content;
                    }
                    break;
                default:
                    return this.error("Unexpected \"".concat(content, "\" found."), { index: token[tokenize_1.FIELDS.START_POS] });
            }
            pos++;
        }
        unescapeProp(node, "attribute");
        unescapeProp(node, "namespace");
        this.newNode(new attribute_1.default(node));
        this.position++;
    };
    /**
     * return a node containing meaningless garbage up to (but not including) the specified token position.
     * if the token position is negative, all remaining tokens are consumed.
     *
     * This returns an array containing a single string node if all whitespace,
     * otherwise an array of comment nodes with space before and after.
     *
     * These tokens are not added to the current selector, the caller can add them or use them to amend
     * a previous node's space metadata.
     *
     * In lossy mode, this returns only comments.
     */
    Parser.prototype.parseWhitespaceEquivalentTokens = function (stopPosition) {
        if (stopPosition < 0) {
            stopPosition = this.tokens.length;
        }
        var startPosition = this.position;
        var nodes = [];
        var space = "";
        var lastComment = undefined;
        do {
            if (WHITESPACE_TOKENS[this.currToken[tokenize_1.FIELDS.TYPE]]) {
                if (!this.options.lossy) {
                    space += this.content();
                }
            }
            else if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.comment) {
                var spaces = {};
                if (space) {
                    spaces.before = space;
                    space = "";
                }
                lastComment = new comment_1.default({
                    value: this.content(),
                    source: getTokenSource(this.currToken),
                    sourceIndex: this.currToken[tokenize_1.FIELDS.START_POS],
                    spaces: spaces,
                });
                nodes.push(lastComment);
            }
        } while (++this.position < stopPosition);
        if (space) {
            if (lastComment) {
                lastComment.spaces.after = space;
            }
            else if (!this.options.lossy) {
                var firstToken = this.tokens[startPosition];
                var lastToken = this.tokens[this.position - 1];
                nodes.push(new string_1.default({
                    value: "",
                    source: getSource(firstToken[tokenize_1.FIELDS.START_LINE], firstToken[tokenize_1.FIELDS.START_COL], lastToken[tokenize_1.FIELDS.END_LINE], lastToken[tokenize_1.FIELDS.END_COL]),
                    sourceIndex: firstToken[tokenize_1.FIELDS.START_POS],
                    spaces: { before: space, after: "" },
                }));
            }
        }
        return nodes;
    };
    /**
     *
     * @param {*} nodes
     */
    Parser.prototype.convertWhitespaceNodesToSpace = function (nodes, requiredSpace) {
        var _this = this;
        if (requiredSpace === void 0) { requiredSpace = false; }
        var space = "";
        var rawSpace = "";
        nodes.forEach(function (n) {
            var spaceBefore = _this.lossySpace(n.spaces.before, requiredSpace);
            var rawSpaceBefore = _this.lossySpace(n.rawSpaceBefore, requiredSpace);
            space +=
                spaceBefore + _this.lossySpace(n.spaces.after, requiredSpace && spaceBefore.length === 0);
            rawSpace +=
                spaceBefore +
                    n.value +
                    _this.lossySpace(n.rawSpaceAfter, requiredSpace && rawSpaceBefore.length === 0);
        });
        if (rawSpace === space) {
            rawSpace = undefined;
        }
        var result = { space: space, rawSpace: rawSpace };
        return result;
    };
    Parser.prototype.isNamedCombinator = function (position) {
        if (position === void 0) { position = this.position; }
        return (this.tokens[position + 0] &&
            this.tokens[position + 0][tokenize_1.FIELDS.TYPE] === tokens.slash &&
            this.tokens[position + 1] &&
            this.tokens[position + 1][tokenize_1.FIELDS.TYPE] === tokens.word &&
            this.tokens[position + 2] &&
            this.tokens[position + 2][tokenize_1.FIELDS.TYPE] === tokens.slash);
    };
    Parser.prototype.namedCombinator = function () {
        if (this.isNamedCombinator()) {
            var nameRaw = this.content(this.tokens[this.position + 1]);
            var name = (0, util_1.unesc)(nameRaw).toLowerCase();
            var raws = {};
            if (name !== nameRaw) {
                raws.value = "/".concat(nameRaw, "/");
            }
            var node = new combinator_1.default({
                value: "/".concat(name, "/"),
                source: getSource(this.currToken[tokenize_1.FIELDS.START_LINE], this.currToken[tokenize_1.FIELDS.START_COL], this.tokens[this.position + 2][tokenize_1.FIELDS.END_LINE], this.tokens[this.position + 2][tokenize_1.FIELDS.END_COL]),
                sourceIndex: this.currToken[tokenize_1.FIELDS.START_POS],
                raws: raws,
            });
            this.position = this.position + 3;
            return node;
        }
        else {
            this.unexpected();
        }
    };
    Parser.prototype.combinator = function () {
        var _this = this;
        if (this.content() === "|") {
            return this.namespace();
        }
        // We need to decide between a space that's a descendant combinator and meaningless whitespace at the end of a selector.
        var nextSigTokenPos = this.locateNextMeaningfulToken(this.position);
        if (nextSigTokenPos < 0 ||
            this.tokens[nextSigTokenPos][tokenize_1.FIELDS.TYPE] === tokens.comma ||
            this.tokens[nextSigTokenPos][tokenize_1.FIELDS.TYPE] === tokens.closeParenthesis) {
            var nodes = this.parseWhitespaceEquivalentTokens(nextSigTokenPos);
            if (nodes.length > 0) {
                var last = this.current.last;
                if (last) {
                    var _a = this.convertWhitespaceNodesToSpace(nodes), space = _a.space, rawSpace = _a.rawSpace;
                    if (rawSpace !== undefined) {
                        last.rawSpaceAfter += rawSpace;
                    }
                    last.spaces.after += space;
                }
                else {
                    nodes.forEach(function (n) { return _this.newNode(n); });
                }
            }
            return;
        }
        var firstToken = this.currToken;
        var spaceOrDescendantSelectorNodes = undefined;
        if (nextSigTokenPos > this.position) {
            spaceOrDescendantSelectorNodes = this.parseWhitespaceEquivalentTokens(nextSigTokenPos);
        }
        var node;
        if (this.isNamedCombinator()) {
            node = this.namedCombinator();
        }
        else if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.combinator) {
            node = new combinator_1.default({
                value: this.content(),
                source: getTokenSource(this.currToken),
                sourceIndex: this.currToken[tokenize_1.FIELDS.START_POS],
            });
            this.position++;
        }
        else if (WHITESPACE_TOKENS[this.currToken[tokenize_1.FIELDS.TYPE]]) {
            // pass
        }
        else if (!spaceOrDescendantSelectorNodes) {
            this.unexpected();
        }
        if (node) {
            if (spaceOrDescendantSelectorNodes) {
                var _b = this.convertWhitespaceNodesToSpace(spaceOrDescendantSelectorNodes), space = _b.space, rawSpace = _b.rawSpace;
                node.spaces.before = space;
                node.rawSpaceBefore = rawSpace;
            }
        }
        else {
            // descendant combinator
            var _c = this.convertWhitespaceNodesToSpace(spaceOrDescendantSelectorNodes, true), space = _c.space, rawSpace = _c.rawSpace;
            if (!rawSpace) {
                rawSpace = space;
            }
            var spaces = {};
            var raws = { spaces: {} };
            if (space.endsWith(" ") && rawSpace.endsWith(" ")) {
                spaces.before = space.slice(0, space.length - 1);
                raws.spaces.before = rawSpace.slice(0, rawSpace.length - 1);
            }
            else if (space[0] === " " && rawSpace[0] === " ") {
                spaces.after = space.slice(1);
                raws.spaces.after = rawSpace.slice(1);
            }
            else {
                raws.value = rawSpace;
            }
            node = new combinator_1.default({
                value: " ",
                source: getTokenSourceSpan(firstToken, this.tokens[this.position - 1]),
                sourceIndex: firstToken[tokenize_1.FIELDS.START_POS],
                spaces: spaces,
                raws: raws,
            });
        }
        if (this.currToken && this.currToken[tokenize_1.FIELDS.TYPE] === tokens.space) {
            node.spaces.after = this.optionalSpace(this.content());
            this.position++;
        }
        return this.newNode(node);
    };
    Parser.prototype.comma = function () {
        if (this.position === this.tokens.length - 1) {
            this.root.trailingComma = true;
            this.position++;
            return;
        }
        this.current._inferEndPosition();
        var selector = new selector_1.default({
            source: {
                start: tokenStart(this.tokens[this.position + 1]),
            },
            sourceIndex: this.tokens[this.position + 1][tokenize_1.FIELDS.START_POS],
        });
        this.current.parent.append(selector);
        this.current = selector;
        this.position++;
    };
    Parser.prototype.comment = function () {
        var current = this.currToken;
        this.newNode(new comment_1.default({
            value: this.content(),
            source: getTokenSource(current),
            sourceIndex: current[tokenize_1.FIELDS.START_POS],
        }));
        this.position++;
    };
    Parser.prototype.error = function (message, opts) {
        throw this.root.error(message, opts);
    };
    Parser.prototype.missingBackslash = function () {
        return this.error("Expected a backslash preceding the semicolon.", {
            index: this.currToken[tokenize_1.FIELDS.START_POS],
        });
    };
    Parser.prototype.missingParenthesis = function () {
        return this.expected("opening parenthesis", this.currToken[tokenize_1.FIELDS.START_POS]);
    };
    Parser.prototype.missingSquareBracket = function () {
        return this.expected("opening square bracket", this.currToken[tokenize_1.FIELDS.START_POS]);
    };
    Parser.prototype.unexpected = function () {
        return this.error("Unexpected '".concat(this.content(), "'. Escaping special characters with \\ may help."), this.currToken[tokenize_1.FIELDS.START_POS]);
    };
    Parser.prototype.unexpectedPipe = function () {
        return this.error("Unexpected '|'.", this.currToken[tokenize_1.FIELDS.START_POS]);
    };
    Parser.prototype.namespace = function () {
        var before = (this.prevToken && this.content(this.prevToken)) || true;
        if (this.nextToken[tokenize_1.FIELDS.TYPE] === tokens.word) {
            this.position++;
            return this.word(before);
        }
        else if (this.nextToken[tokenize_1.FIELDS.TYPE] === tokens.asterisk) {
            this.position++;
            return this.universal(before);
        }
        this.unexpectedPipe();
    };
    Parser.prototype.nesting = function () {
        if (this.nextToken) {
            var nextContent = this.content(this.nextToken);
            if (nextContent === "|") {
                this.position++;
                return;
            }
        }
        var current = this.currToken;
        this.newNode(new nesting_1.default({
            value: this.content(),
            source: getTokenSource(current),
            sourceIndex: current[tokenize_1.FIELDS.START_POS],
        }));
        this.position++;
    };
    Parser.prototype.parentheses = function () {
        var last = this.current.last;
        var unbalanced = 1;
        this.position++;
        if (last && last.type === types.PSEUDO) {
            var selector = new selector_1.default({
                source: { start: tokenStart(this.tokens[this.position]) },
                sourceIndex: this.tokens[this.position][tokenize_1.FIELDS.START_POS],
            });
            var cache = this.current;
            last.append(selector);
            this.current = selector;
            // Track nesting depth so deeply nested pseudo selectors raise a
            // catchable error instead of overflowing the call stack. The
            // counter is restored in `finally` so the parser is never left in
            // an inconsistent state, even on the error path.
            this.nestingDepth++;
            try {
                if (this.nestingDepth > this.maxNestingDepth) {
                    this.error("Cannot parse selector: nesting depth exceeds the maximum of ".concat(this.maxNestingDepth, "."), { index: this.currToken[tokenize_1.FIELDS.START_POS] });
                }
                while (this.position < this.tokens.length && unbalanced) {
                    if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.openParenthesis) {
                        unbalanced++;
                    }
                    if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.closeParenthesis) {
                        unbalanced--;
                    }
                    if (unbalanced) {
                        this.parse();
                    }
                    else {
                        this.current.source.end = tokenEnd(this.currToken);
                        this.current.parent.source.end = tokenEnd(this.currToken);
                        this.position++;
                    }
                }
            }
            finally {
                this.nestingDepth--;
            }
            this.current = cache;
        }
        else {
            // I think this case should be an error. It's used to implement a basic parse of media queries
            // but I don't think it's a good idea.
            var parenStart = this.currToken;
            var parenValue = "(";
            var parenEnd = void 0;
            while (this.position < this.tokens.length && unbalanced) {
                if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.openParenthesis) {
                    unbalanced++;
                }
                if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.closeParenthesis) {
                    unbalanced--;
                }
                parenEnd = this.currToken;
                parenValue += this.parseParenthesisToken(this.currToken);
                this.position++;
            }
            if (last) {
                last.appendToPropertyAndEscape("value", parenValue, parenValue);
            }
            else {
                this.newNode(new string_1.default({
                    value: parenValue,
                    source: getSource(parenStart[tokenize_1.FIELDS.START_LINE], parenStart[tokenize_1.FIELDS.START_COL], parenEnd[tokenize_1.FIELDS.END_LINE], parenEnd[tokenize_1.FIELDS.END_COL]),
                    sourceIndex: parenStart[tokenize_1.FIELDS.START_POS],
                }));
            }
        }
        if (unbalanced) {
            return this.expected("closing parenthesis", this.currToken[tokenize_1.FIELDS.START_POS]);
        }
    };
    Parser.prototype.pseudo = function () {
        var _this = this;
        var pseudoStr = "";
        var startingToken = this.currToken;
        while (this.currToken && this.currToken[tokenize_1.FIELDS.TYPE] === tokens.colon) {
            pseudoStr += this.content();
            this.position++;
        }
        if (!this.currToken) {
            return this.expected(["pseudo-class", "pseudo-element"], this.position - 1);
        }
        if (this.currToken[tokenize_1.FIELDS.TYPE] === tokens.word) {
            this.splitWord(false, function (first, length) {
                pseudoStr += first;
                _this.newNode(new pseudo_1.default({
                    value: pseudoStr,
                    source: getTokenSourceSpan(startingToken, _this.currToken),
                    sourceIndex: startingToken[tokenize_1.FIELDS.START_POS],
                }));
                if (length > 1 && _this.nextToken && _this.nextToken[tokenize_1.FIELDS.TYPE] === tokens.openParenthesis) {
                    _this.error("Misplaced parenthesis.", {
                        index: _this.nextToken[tokenize_1.FIELDS.START_POS],
                    });
                }
            });
        }
        else {
            return this.expected(["pseudo-class", "pseudo-element"], this.currToken[tokenize_1.FIELDS.START_POS]);
        }
    };
    Parser.prototype.space = function () {
        var content = this.content();
        // Handle space before and after the selector
        if (this.position === 0 ||
            this.prevToken[tokenize_1.FIELDS.TYPE] === tokens.comma ||
            this.prevToken[tokenize_1.FIELDS.TYPE] === tokens.openParenthesis ||
            this.current.nodes.every(function (node) { return node.type === "comment"; })) {
            this.spaces = this.optionalSpace(content);
            this.position++;
        }
        else if (this.position === this.tokens.length - 1 ||
            this.nextToken[tokenize_1.FIELDS.TYPE] === tokens.comma ||
            this.nextToken[tokenize_1.FIELDS.TYPE] === tokens.closeParenthesis) {
            this.current.last.spaces.after = this.optionalSpace(content);
            this.position++;
        }
        else {
            this.combinator();
        }
    };
    Parser.prototype.string = function () {
        var current = this.currToken;
        this.newNode(new string_1.default({
            value: this.content(),
            source: getTokenSource(current),
            sourceIndex: current[tokenize_1.FIELDS.START_POS],
        }));
        this.position++;
    };
    Parser.prototype.universal = function (namespace) {
        var nextToken = this.nextToken;
        if (nextToken && this.content(nextToken) === "|") {
            this.position++;
            return this.namespace();
        }
        var current = this.currToken;
        this.newNode(new universal_1.default({
            value: this.content(),
            source: getTokenSource(current),
            sourceIndex: current[tokenize_1.FIELDS.START_POS],
        }), namespace);
        this.position++;
    };
    Parser.prototype.splitWord = function (namespace, firstCallback) {
        var _this = this;
        var nextToken = this.nextToken;
        var word = this.content();
        while (nextToken &&
            ~[tokens.dollar, tokens.caret, tokens.equals, tokens.word].indexOf(nextToken[tokenize_1.FIELDS.TYPE])) {
            this.position++;
            var current = this.content();
            word += current;
            if (current.lastIndexOf("\\") === current.length - 1) {
                var next = this.nextToken;
                if (next && next[tokenize_1.FIELDS.TYPE] === tokens.space) {
                    word += this.requiredSpace(this.content(next));
                    this.position++;
                }
            }
            nextToken = this.nextToken;
        }
        var hasClass = indexesOf(word, ".").filter(function (i) {
            // Allow escaped dot within class name
            var escapedDot = word[i - 1] === "\\";
            // Allow decimal numbers percent in @keyframes
            var isKeyframesPercent = /^\d+\.\d+%$/.test(word);
            return !escapedDot && !isKeyframesPercent;
        });
        var hasId = indexesOf(word, "#").filter(function (i) { return word[i - 1] !== "\\"; });
        // Eliminate Sass interpolations from the list of id indexes
        var interpolations = indexesOf(word, "#{");
        if (interpolations.length) {
            hasId = hasId.filter(function (hashIndex) { return !~interpolations.indexOf(hashIndex); });
        }
        var indices = (0, sortAscending_1.default)(uniqs(__spreadArray(__spreadArray([0], __read(hasClass), false), __read(hasId), false)));
        indices.forEach(function (ind, i) {
            var index = indices[i + 1] || word.length;
            var value = word.slice(ind, index);
            if (i === 0 && firstCallback) {
                return firstCallback.call(_this, value, indices.length);
            }
            var node;
            var current = _this.currToken;
            var sourceIndex = current[tokenize_1.FIELDS.START_POS] + indices[i];
            var source = getSource(current[1], current[2] + ind, current[3], current[2] + (index - 1));
            if (~hasClass.indexOf(ind)) {
                var classNameOpts = {
                    value: value.slice(1),
                    source: source,
                    sourceIndex: sourceIndex,
                };
                node = new className_1.default(unescapeProp(classNameOpts, "value"));
            }
            else if (~hasId.indexOf(ind)) {
                var idOpts = {
                    value: value.slice(1),
                    source: source,
                    sourceIndex: sourceIndex,
                };
                node = new id_1.default(unescapeProp(idOpts, "value"));
            }
            else {
                var tagOpts = {
                    value: value,
                    source: source,
                    sourceIndex: sourceIndex,
                };
                unescapeProp(tagOpts, "value");
                node = new tag_1.default(tagOpts);
            }
            _this.newNode(node, namespace);
            // Ensure that the namespace is used only once
            namespace = null;
        });
        this.position++;
    };
    Parser.prototype.word = function (namespace) {
        var nextToken = this.nextToken;
        if (nextToken && this.content(nextToken) === "|") {
            this.position++;
            return this.namespace();
        }
        return this.splitWord(namespace);
    };
    Parser.prototype.loop = function () {
        while (this.position < this.tokens.length) {
            this.parse(true);
        }
        this.current._inferEndPosition();
        return this.root;
    };
    Parser.prototype.parse = function (throwOnParenthesis) {
        switch (this.currToken[tokenize_1.FIELDS.TYPE]) {
            case tokens.space:
                this.space();
                break;
            case tokens.comment:
                this.comment();
                break;
            case tokens.openParenthesis:
                this.parentheses();
                break;
            case tokens.closeParenthesis:
                if (throwOnParenthesis) {
                    this.missingParenthesis();
                }
                break;
            case tokens.openSquare:
                this.attribute();
                break;
            case tokens.dollar:
            case tokens.caret:
            case tokens.equals:
            case tokens.word:
                this.word();
                break;
            case tokens.colon:
                this.pseudo();
                break;
            case tokens.comma:
                this.comma();
                break;
            case tokens.asterisk:
                this.universal();
                break;
            case tokens.ampersand:
                this.nesting();
                break;
            case tokens.slash:
            case tokens.combinator:
                this.combinator();
                break;
            case tokens.str:
                this.string();
                break;
            // These cases throw; no break needed.
            case tokens.closeSquare:
                this.missingSquareBracket();
            case tokens.semicolon:
                this.missingBackslash();
            default:
                this.unexpected();
        }
    };
    /**
     * Helpers
     */
    Parser.prototype.expected = function (description, index, found) {
        if (Array.isArray(description)) {
            var last = description.pop();
            description = "".concat(description.join(", "), " or ").concat(last);
        }
        var an = /^[aeiou]/.test(description[0]) ? "an" : "a";
        if (!found) {
            return this.error("Expected ".concat(an, " ").concat(description, "."), { index: index });
        }
        return this.error("Expected ".concat(an, " ").concat(description, ", found \"").concat(found, "\" instead."), { index: index });
    };
    Parser.prototype.requiredSpace = function (space) {
        return this.options.lossy ? " " : space;
    };
    Parser.prototype.optionalSpace = function (space) {
        return this.options.lossy ? "" : space;
    };
    Parser.prototype.lossySpace = function (space, required) {
        if (this.options.lossy) {
            return required ? " " : "";
        }
        else {
            return space;
        }
    };
    Parser.prototype.parseParenthesisToken = function (token) {
        var content = this.content(token);
        if (token[tokenize_1.FIELDS.TYPE] === tokens.space) {
            return this.requiredSpace(content);
        }
        else {
            return content;
        }
    };
    Parser.prototype.newNode = function (node, namespace) {
        if (namespace) {
            if (/^ +$/.test(namespace)) {
                if (!this.options.lossy) {
                    this.spaces = (this.spaces || "") + namespace;
                }
                namespace = true;
            }
            node.namespace = namespace;
            unescapeProp(node, "namespace");
        }
        if (this.spaces) {
            node.spaces.before = this.spaces;
            this.spaces = "";
        }
        return this.current.append(node);
    };
    Parser.prototype.content = function (token) {
        if (token === void 0) { token = this.currToken; }
        return this.css.slice(token[tokenize_1.FIELDS.START_POS], token[tokenize_1.FIELDS.END_POS]);
    };
    Object.defineProperty(Parser.prototype, "currToken", {
        get: function () {
            return this.tokens[this.position];
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Parser.prototype, "nextToken", {
        get: function () {
            return this.tokens[this.position + 1];
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Parser.prototype, "prevToken", {
        get: function () {
            return this.tokens[this.position - 1];
        },
        enumerable: false,
        configurable: true
    });
    /**
     * returns the index of the next non-whitespace, non-comment token.
     * returns -1 if no meaningful token is found.
     */
    Parser.prototype.locateNextMeaningfulToken = function (startPosition) {
        if (startPosition === void 0) { startPosition = this.position + 1; }
        var searchPosition = startPosition;
        while (searchPosition < this.tokens.length) {
            if (WHITESPACE_EQUIV_TOKENS[this.tokens[searchPosition][tokenize_1.FIELDS.TYPE]]) {
                searchPosition++;
                continue;
            }
            else {
                return searchPosition;
            }
        }
        return -1;
    };
    return Parser;
}());
exports.default = Parser;
//# sourceMappingURL=parser.js.map