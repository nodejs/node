"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var util_1 = require("../util");
var cloneNode = function (obj, parent, depth) {
    if (depth === void 0) { depth = 0; }
    // Bound recursion so a pathologically deep node tree raises a catchable
    // error instead of overflowing the call stack (CVE-2026-9358 / CWE-674).
    if (depth > util_1.MAX_NESTING_DEPTH) {
        throw new Error("Cannot clone selector: nesting depth exceeds the maximum of ".concat(util_1.MAX_NESTING_DEPTH, "."));
    }
    if (typeof obj !== "object" || obj === null) {
        return obj;
    }
    var cloned = new obj.constructor();
    for (var i in obj) {
        if (!obj.hasOwnProperty(i)) {
            continue;
        }
        var value = obj[i];
        var type = typeof value;
        if (i === "parent" && type === "object") {
            if (parent) {
                cloned[i] = parent;
            }
        }
        else if (value instanceof Array) {
            cloned[i] = value.map(function (j) { return cloneNode(j, cloned, depth + 1); });
        }
        else {
            cloned[i] = cloneNode(value, cloned, depth + 1);
        }
    }
    return cloned;
};
var Node = /** @class */ (function () {
    function Node(opts) {
        if (opts === void 0) { opts = {}; }
        Object.assign(this, opts);
        this.spaces = this.spaces || {};
        this.spaces.before = this.spaces.before || "";
        this.spaces.after = this.spaces.after || "";
    }
    Node.prototype.remove = function () {
        if (this.parent) {
            this.parent.removeChild(this);
        }
        this.parent = undefined;
        return this;
    };
    Node.prototype.replaceWith = function () {
        if (this.parent) {
            for (var index in arguments) {
                this.parent.insertBefore(this, arguments[index]);
            }
            this.remove();
        }
        return this;
    };
    Node.prototype.next = function () {
        return this.parent.at(this.parent.index(this) + 1);
    };
    Node.prototype.prev = function () {
        return this.parent.at(this.parent.index(this) - 1);
    };
    Node.prototype.clone = function (overrides) {
        if (overrides === void 0) { overrides = {}; }
        var cloned = cloneNode(this);
        for (var name in overrides) {
            cloned[name] = overrides[name];
        }
        return cloned;
    };
    /**
     * Some non-standard syntax doesn't follow normal escaping rules for css.
     * This allows non standard syntax to be appended to an existing property
     * by specifying the escaped value. By specifying the escaped value,
     * illegal characters are allowed to be directly inserted into css output.
     * @param {string} name the property to set
     * @param {any} value the unescaped value of the property
     * @param {string} valueEscaped optional. the escaped value of the property.
     */
    Node.prototype.appendToPropertyAndEscape = function (name, value, valueEscaped) {
        if (!this.raws) {
            this.raws = {};
        }
        var originalValue = this[name];
        var originalEscaped = this.raws[name];
        this[name] = originalValue + value; // this may trigger a setter that updates raws, so it has to be set first.
        if (originalEscaped || valueEscaped !== value) {
            this.raws[name] = (originalEscaped || originalValue) + valueEscaped;
        }
        else {
            delete this.raws[name]; // delete any escaped value that was created by the setter.
        }
    };
    /**
     * Some non-standard syntax doesn't follow normal escaping rules for css.
     * This allows the escaped value to be specified directly, allowing illegal
     * characters to be directly inserted into css output.
     * @param {string} name the property to set
     * @param {any} value the unescaped value of the property
     * @param {string} valueEscaped the escaped value of the property.
     */
    Node.prototype.setPropertyAndEscape = function (name, value, valueEscaped) {
        if (!this.raws) {
            this.raws = {};
        }
        this[name] = value; // this may trigger a setter that updates raws, so it has to be set first.
        this.raws[name] = valueEscaped;
    };
    /**
     * When you want a value to passed through to CSS directly. This method
     * deletes the corresponding raw value causing the stringifier to fallback
     * to the unescaped value.
     * @param {string} name the property to set.
     * @param {any} value The value that is both escaped and unescaped.
     */
    Node.prototype.setPropertyWithoutEscape = function (name, value) {
        this[name] = value; // this may trigger a setter that updates raws, so it has to be set first.
        if (this.raws) {
            delete this.raws[name];
        }
    };
    /**
     *
     * @param {number} line The number (starting with 1)
     * @param {number} column The column number (starting with 1)
     */
    Node.prototype.isAtPosition = function (line, column) {
        if (this.source && this.source.start && this.source.end) {
            if (this.source.start.line > line) {
                return false;
            }
            if (this.source.end.line < line) {
                return false;
            }
            if (this.source.start.line === line && this.source.start.column > column) {
                return false;
            }
            if (this.source.end.line === line && this.source.end.column < column) {
                return false;
            }
            return true;
        }
        return undefined;
    };
    Node.prototype.stringifyProperty = function (name) {
        return (this.raws && this.raws[name]) || this[name];
    };
    Object.defineProperty(Node.prototype, "rawSpaceBefore", {
        get: function () {
            var rawSpace = this.raws && this.raws.spaces && this.raws.spaces.before;
            if (rawSpace === undefined) {
                rawSpace = this.spaces && this.spaces.before;
            }
            return rawSpace || "";
        },
        set: function (raw) {
            (0, util_1.ensureObject)(this, "raws", "spaces");
            this.raws.spaces.before = raw;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Node.prototype, "rawSpaceAfter", {
        get: function () {
            var rawSpace = this.raws && this.raws.spaces && this.raws.spaces.after;
            if (rawSpace === undefined) {
                rawSpace = this.spaces.after;
            }
            return rawSpace || "";
        },
        set: function (raw) {
            (0, util_1.ensureObject)(this, "raws", "spaces");
            this.raws.spaces.after = raw;
        },
        enumerable: false,
        configurable: true
    });
    Node.prototype.valueToString = function () {
        return String(this.stringifyProperty("value"));
    };
    Node.prototype.toString = function () {
        return [this.rawSpaceBefore, this.valueToString(), this.rawSpaceAfter].join("");
    };
    // Internal recursion entry point used by Container serialization. Leaf
    // nodes don't recurse, so they ignore the depth/limit and stringify
    // themselves. Containers override this to thread the nesting depth.
    Node.prototype._stringify = function () {
        return this.toString();
    };
    return Node;
}());
exports.default = Node;
//# sourceMappingURL=node.js.map