"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var _a;
Object.defineProperty(exports, "__esModule", { value: true });
exports.unescapeValue = unescapeValue;
var cssesc_1 = __importDefault(require("cssesc"));
var unesc_1 = __importDefault(require("../util/unesc"));
var namespace_1 = __importDefault(require("./namespace"));
var types_1 = require("./types");
var deprecate = require("util-deprecate");
var WRAPPED_IN_QUOTES = /^('|")([^]*)\1$/;
var warnOfDeprecatedValueAssignment = deprecate(function () { }, "Assigning an attribute a value containing characters that might need to be escaped is deprecated. " +
    "Call attribute.setValue() instead.");
var warnOfDeprecatedQuotedAssignment = deprecate(function () { }, "Assigning attr.quoted is deprecated and has no effect. Assign to attr.quoteMark instead.");
var warnOfDeprecatedConstructor = deprecate(function () { }, "Constructing an Attribute selector with a value without specifying quoteMark is deprecated. Note: The value should be unescaped now.");
function unescapeValue(value) {
    var deprecatedUsage = false;
    var quoteMark = null;
    var unescaped = value;
    var m = unescaped.match(WRAPPED_IN_QUOTES);
    if (m) {
        quoteMark = m[1];
        unescaped = m[2];
    }
    unescaped = (0, unesc_1.default)(unescaped);
    if (unescaped !== value) {
        deprecatedUsage = true;
    }
    return {
        deprecatedUsage: deprecatedUsage,
        unescaped: unescaped,
        quoteMark: quoteMark,
    };
}
function handleDeprecatedContructorOpts(opts) {
    if (opts.quoteMark !== undefined) {
        return opts;
    }
    if (opts.value === undefined) {
        return opts;
    }
    warnOfDeprecatedConstructor();
    var _a = unescapeValue(opts.value), quoteMark = _a.quoteMark, unescaped = _a.unescaped;
    if (!opts.raws) {
        opts.raws = {};
    }
    if (opts.raws.value === undefined) {
        opts.raws.value = opts.value;
    }
    opts.value = unescaped;
    opts.quoteMark = quoteMark;
    return opts;
}
var Attribute = /** @class */ (function (_super) {
    __extends(Attribute, _super);
    function Attribute(opts) {
        if (opts === void 0) { opts = {}; }
        var _this = _super.call(this, handleDeprecatedContructorOpts(opts)) || this;
        _this.type = types_1.ATTRIBUTE;
        _this.raws = _this.raws || {};
        Object.defineProperty(_this.raws, "unquoted", {
            get: deprecate(function () { return _this.value; }, "attr.raws.unquoted is deprecated. Call attr.value instead."),
            set: deprecate(function () { return _this.value; }, "Setting attr.raws.unquoted is deprecated and has no effect. attr.value is unescaped by default now."),
        });
        _this._constructed = true;
        return _this;
    }
    /**
     * Returns the Attribute's value quoted such that it would be legal to use
     * in the value of a css file. The original value's quotation setting
     * used for stringification is left unchanged. See `setValue(value, options)`
     * if you want to control the quote settings of a new value for the attribute.
     *
     * You can also change the quotation used for the current value by setting quoteMark.
     *
     * Options:
     *   * quoteMark {'"' | "'" | null} - Use this value to quote the value. If this
     *     option is not set, the original value for quoteMark will be used. If
     *     indeterminate, a double quote is used. The legal values are:
     *     * `null` - the value will be unquoted and characters will be escaped as necessary.
     *     * `'` - the value will be quoted with a single quote and single quotes are escaped.
     *     * `"` - the value will be quoted with a double quote and double quotes are escaped.
     *   * preferCurrentQuoteMark {boolean} - if true, prefer the source quote mark
     *     over the quoteMark option value.
     *   * smart {boolean} - if true, will select a quote mark based on the value
     *     and the other options specified here. See the `smartQuoteMark()`
     *     method.
     **/
    Attribute.prototype.getQuotedValue = function (options) {
        if (options === void 0) { options = {}; }
        var quoteMark = this._determineQuoteMark(options);
        var cssescopts = CSSESC_QUOTE_OPTIONS[quoteMark];
        var escaped = (0, cssesc_1.default)(this._value, cssescopts);
        return escaped;
    };
    Attribute.prototype._determineQuoteMark = function (options) {
        return options.smart ? this.smartQuoteMark(options) : this.preferredQuoteMark(options);
    };
    /**
     * Set the unescaped value with the specified quotation options. The value
     * provided must not include any wrapping quote marks -- those quotes will
     * be interpreted as part of the value and escaped accordingly.
     */
    Attribute.prototype.setValue = function (value, options) {
        if (options === void 0) { options = {}; }
        this._value = value;
        this._quoteMark = this._determineQuoteMark(options);
        this._syncRawValue();
    };
    /**
     * Intelligently select a quoteMark value based on the value's contents. If
     * the value is a legal CSS ident, it will not be quoted. Otherwise a quote
     * mark will be picked that minimizes the number of escapes.
     *
     * If there's no clear winner, the quote mark from these options is used,
     * then the source quote mark (this is inverted if `preferCurrentQuoteMark` is
     * true). If the quoteMark is unspecified, a double quote is used.
     *
     * @param options This takes the quoteMark and preferCurrentQuoteMark options
     * from the quoteValue method.
     */
    Attribute.prototype.smartQuoteMark = function (options) {
        var v = this.value;
        var numSingleQuotes = v.replace(/[^']/g, "").length;
        var numDoubleQuotes = v.replace(/[^"]/g, "").length;
        if (numSingleQuotes + numDoubleQuotes === 0) {
            var escaped = (0, cssesc_1.default)(v, { isIdentifier: true });
            if (escaped === v) {
                return Attribute.NO_QUOTE;
            }
            else {
                var pref = this.preferredQuoteMark(options);
                if (pref === Attribute.NO_QUOTE) {
                    // pick a quote mark that isn't none and see if it's smaller
                    var quote = this.quoteMark || options.quoteMark || Attribute.DOUBLE_QUOTE;
                    var opts = CSSESC_QUOTE_OPTIONS[quote];
                    var quoteValue = (0, cssesc_1.default)(v, opts);
                    if (quoteValue.length < escaped.length) {
                        return quote;
                    }
                }
                return pref;
            }
        }
        else if (numDoubleQuotes === numSingleQuotes) {
            return this.preferredQuoteMark(options);
        }
        else if (numDoubleQuotes < numSingleQuotes) {
            return Attribute.DOUBLE_QUOTE;
        }
        else {
            return Attribute.SINGLE_QUOTE;
        }
    };
    /**
     * Selects the preferred quote mark based on the options and the current quote mark value.
     * If you want the quote mark to depend on the attribute value, call `smartQuoteMark(opts)`
     * instead.
     */
    Attribute.prototype.preferredQuoteMark = function (options) {
        var quoteMark = options.preferCurrentQuoteMark ? this.quoteMark : options.quoteMark;
        if (quoteMark === undefined) {
            quoteMark = options.preferCurrentQuoteMark ? options.quoteMark : this.quoteMark;
        }
        if (quoteMark === undefined) {
            quoteMark = Attribute.DOUBLE_QUOTE;
        }
        return quoteMark;
    };
    Object.defineProperty(Attribute.prototype, "quoted", {
        get: function () {
            var qm = this.quoteMark;
            return qm === "'" || qm === '"';
        },
        set: function (value) {
            warnOfDeprecatedQuotedAssignment();
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Attribute.prototype, "quoteMark", {
        /**
         * returns a single (`'`) or double (`"`) quote character if the value is quoted.
         * returns `null` if the value is not quoted.
         * returns `undefined` if the quotation state is unknown (this can happen when
         * the attribute is constructed without specifying a quote mark.)
         */
        get: function () {
            return this._quoteMark;
        },
        /**
         * Set the quote mark to be used by this attribute's value.
         * If the quote mark changes, the raw (escaped) value at `attr.raws.value` of the attribute
         * value is updated accordingly.
         *
         * @param {"'" | '"' | null} quoteMark The quote mark or `null` if the value should be unquoted.
         */
        set: function (quoteMark) {
            if (!this._constructed) {
                this._quoteMark = quoteMark;
                return;
            }
            if (this._quoteMark !== quoteMark) {
                this._quoteMark = quoteMark;
                this._syncRawValue();
            }
        },
        enumerable: false,
        configurable: true
    });
    Attribute.prototype._syncRawValue = function () {
        var rawValue = (0, cssesc_1.default)(this._value, CSSESC_QUOTE_OPTIONS[this.quoteMark]);
        if (rawValue === this._value) {
            if (this.raws) {
                delete this.raws.value;
            }
        }
        else {
            this.raws.value = rawValue;
        }
    };
    Object.defineProperty(Attribute.prototype, "qualifiedAttribute", {
        get: function () {
            return this.qualifiedName(this.raws.attribute || this.attribute);
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Attribute.prototype, "insensitiveFlag", {
        get: function () {
            return this.insensitive ? "i" : "";
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Attribute.prototype, "value", {
        get: function () {
            return this._value;
        },
        /**
         * Before 3.0, the value had to be set to an escaped value including any wrapped
         * quote marks. In 3.0, the semantics of `Attribute.value` changed so that the value
         * is unescaped during parsing and any quote marks are removed.
         *
         * Because the ambiguity of this semantic change, if you set `attr.value = newValue`,
         * a deprecation warning is raised when the new value contains any characters that would
         * require escaping (including if it contains wrapped quotes).
         *
         * Instead, you should call `attr.setValue(newValue, opts)` and pass options that describe
         * how the new value is quoted.
         */
        set: function (v) {
            if (this._constructed) {
                var _a = unescapeValue(v), deprecatedUsage = _a.deprecatedUsage, unescaped = _a.unescaped, quoteMark = _a.quoteMark;
                if (deprecatedUsage) {
                    warnOfDeprecatedValueAssignment();
                }
                if (unescaped === this._value && quoteMark === this._quoteMark) {
                    return;
                }
                this._value = unescaped;
                this._quoteMark = quoteMark;
                this._syncRawValue();
            }
            else {
                this._value = v;
            }
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Attribute.prototype, "insensitive", {
        get: function () {
            return this._insensitive;
        },
        /**
         * Set the case insensitive flag.
         * If the case insensitive flag changes, the raw (escaped) value at `attr.raws.insensitiveFlag`
         * of the attribute is updated accordingly.
         *
         * @param {true | false} insensitive true if the attribute should match case-insensitively.
         */
        set: function (insensitive) {
            if (!insensitive) {
                this._insensitive = false;
                // "i" and "I" can be used in "this.raws.insensitiveFlag" to store the original notation.
                // When setting `attr.insensitive = false` both should be erased to ensure correct serialization.
                if (this.raws && (this.raws.insensitiveFlag === "I" || this.raws.insensitiveFlag === "i")) {
                    this.raws.insensitiveFlag = undefined;
                }
            }
            this._insensitive = insensitive;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Attribute.prototype, "attribute", {
        get: function () {
            return this._attribute;
        },
        set: function (name) {
            this._handleEscapes("attribute", name);
            this._attribute = name;
        },
        enumerable: false,
        configurable: true
    });
    Attribute.prototype._handleEscapes = function (prop, value) {
        if (this._constructed) {
            var escaped = (0, cssesc_1.default)(value, { isIdentifier: true });
            if (escaped !== value) {
                this.raws[prop] = escaped;
            }
            else {
                delete this.raws[prop];
            }
        }
    };
    Attribute.prototype._spacesFor = function (name) {
        var attrSpaces = { before: "", after: "" };
        var spaces = this.spaces[name] || {};
        var rawSpaces = (this.raws.spaces && this.raws.spaces[name]) || {};
        return Object.assign(attrSpaces, spaces, rawSpaces);
    };
    Attribute.prototype._stringFor = function (name, spaceName, concat) {
        if (spaceName === void 0) { spaceName = name; }
        if (concat === void 0) { concat = defaultAttrConcat; }
        var attrSpaces = this._spacesFor(spaceName);
        return concat(this.stringifyProperty(name), attrSpaces);
    };
    /**
     * returns the offset of the attribute part specified relative to the
     * start of the node of the output string.
     *
     * * "ns" - alias for "namespace"
     * * "namespace" - the namespace if it exists.
     * * "attribute" - the attribute name
     * * "attributeNS" - the start of the attribute or its namespace
     * * "operator" - the match operator of the attribute
     * * "value" - The value (string or identifier)
     * * "insensitive" - the case insensitivity flag;
     * @param part One of the possible values inside an attribute.
     * @returns -1 if the name is invalid or the value doesn't exist in this attribute.
     */
    Attribute.prototype.offsetOf = function (name) {
        var count = 1;
        var attributeSpaces = this._spacesFor("attribute");
        count += attributeSpaces.before.length;
        if (name === "namespace" || name === "ns") {
            return this.namespace ? count : -1;
        }
        if (name === "attributeNS") {
            return count;
        }
        count += this.namespaceString.length;
        if (this.namespace) {
            count += 1;
        }
        if (name === "attribute") {
            return count;
        }
        count += this.stringifyProperty("attribute").length;
        count += attributeSpaces.after.length;
        var operatorSpaces = this._spacesFor("operator");
        count += operatorSpaces.before.length;
        var operator = this.stringifyProperty("operator");
        if (name === "operator") {
            return operator ? count : -1;
        }
        count += operator.length;
        count += operatorSpaces.after.length;
        var valueSpaces = this._spacesFor("value");
        count += valueSpaces.before.length;
        var value = this.stringifyProperty("value");
        if (name === "value") {
            return value ? count : -1;
        }
        count += value.length;
        count += valueSpaces.after.length;
        var insensitiveSpaces = this._spacesFor("insensitive");
        count += insensitiveSpaces.before.length;
        if (name === "insensitive") {
            return this.insensitive ? count : -1;
        }
        return -1;
    };
    Attribute.prototype.toString = function () {
        var _this = this;
        var selector = [this.rawSpaceBefore, "["];
        selector.push(this._stringFor("qualifiedAttribute", "attribute"));
        if (this.operator && (this.value || this.value === "")) {
            selector.push(this._stringFor("operator"));
            selector.push(this._stringFor("value"));
            selector.push(this._stringFor("insensitiveFlag", "insensitive", function (attrValue, attrSpaces) {
                if (attrValue.length > 0 &&
                    !_this.quoted &&
                    attrSpaces.before.length === 0 &&
                    !(_this.spaces.value && _this.spaces.value.after)) {
                    attrSpaces.before = " ";
                }
                return defaultAttrConcat(attrValue, attrSpaces);
            }));
        }
        selector.push("]");
        selector.push(this.rawSpaceAfter);
        return selector.join("");
    };
    Attribute.NO_QUOTE = null;
    Attribute.SINGLE_QUOTE = "'";
    Attribute.DOUBLE_QUOTE = '"';
    return Attribute;
}(namespace_1.default));
exports.default = Attribute;
var CSSESC_QUOTE_OPTIONS = (_a = {
        "'": { quotes: "single", wrap: true },
        '"': { quotes: "double", wrap: true }
    },
    _a[null] = { isIdentifier: true },
    _a);
function defaultAttrConcat(attrValue, attrSpaces) {
    return "".concat(attrSpaces.before).concat(attrValue).concat(attrSpaces.after);
}
//# sourceMappingURL=attribute.js.map