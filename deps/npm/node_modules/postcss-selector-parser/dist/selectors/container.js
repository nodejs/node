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
var __values = (this && this.__values) || function(o) {
    var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
    if (m) return m.call(o);
    if (o && typeof o.length === "number") return {
        next: function () {
            if (o && i >= o.length) o = void 0;
            return { value: o && o[i++], done: !o };
        }
    };
    throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
};
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
Object.defineProperty(exports, "__esModule", { value: true });
var util_1 = require("../util");
var node_1 = __importDefault(require("./node"));
var types = __importStar(require("./types"));
var Container = /** @class */ (function (_super) {
    __extends(Container, _super);
    function Container(opts) {
        var _this = _super.call(this, opts) || this;
        if (!_this.nodes) {
            _this.nodes = [];
        }
        return _this;
    }
    Container.prototype.append = function (selector) {
        selector.parent = this;
        this.nodes.push(selector);
        return this;
    };
    Container.prototype.prepend = function (selector) {
        selector.parent = this;
        this.nodes.unshift(selector);
        for (var id in this.indexes) {
            this.indexes[id]++;
        }
        return this;
    };
    Container.prototype.at = function (index) {
        return this.nodes[index];
    };
    Container.prototype.index = function (child) {
        if (typeof child === "number") {
            return child;
        }
        return this.nodes.indexOf(child);
    };
    Object.defineProperty(Container.prototype, "first", {
        get: function () {
            return this.at(0);
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Container.prototype, "last", {
        get: function () {
            return this.at(this.length - 1);
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Container.prototype, "length", {
        get: function () {
            return this.nodes.length;
        },
        enumerable: false,
        configurable: true
    });
    Container.prototype.removeChild = function (child) {
        child = this.index(child);
        this.at(child).parent = undefined;
        this.nodes.splice(child, 1);
        var index;
        for (var id in this.indexes) {
            index = this.indexes[id];
            if (index >= child) {
                this.indexes[id] = index - 1;
            }
        }
        return this;
    };
    Container.prototype.removeAll = function () {
        var e_1, _a;
        try {
            for (var _b = __values(this.nodes), _c = _b.next(); !_c.done; _c = _b.next()) {
                var node = _c.value;
                node.parent = undefined;
            }
        }
        catch (e_1_1) { e_1 = { error: e_1_1 }; }
        finally {
            try {
                if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
            }
            finally { if (e_1) throw e_1.error; }
        }
        this.nodes = [];
        return this;
    };
    Container.prototype.empty = function () {
        return this.removeAll();
    };
    Container.prototype.insertAfter = function (oldNode, newNode) {
        var _a;
        newNode.parent = this;
        var oldIndex = this.index(oldNode);
        var resetNode = [];
        for (var i = 2; i < arguments.length; i++) {
            resetNode.push(arguments[i]);
        }
        (_a = this.nodes).splice.apply(_a, __spreadArray([oldIndex + 1, 0, newNode], __read(resetNode), false));
        newNode.parent = this;
        var index;
        for (var id in this.indexes) {
            index = this.indexes[id];
            if (oldIndex < index) {
                this.indexes[id] = index + arguments.length - 1;
            }
        }
        return this;
    };
    Container.prototype.insertBefore = function (oldNode, newNode) {
        var _a;
        newNode.parent = this;
        var oldIndex = this.index(oldNode);
        var resetNode = [];
        for (var i = 2; i < arguments.length; i++) {
            resetNode.push(arguments[i]);
        }
        (_a = this.nodes).splice.apply(_a, __spreadArray([oldIndex, 0, newNode], __read(resetNode), false));
        newNode.parent = this;
        var index;
        for (var id in this.indexes) {
            index = this.indexes[id];
            if (index >= oldIndex) {
                this.indexes[id] = index + arguments.length - 1;
            }
        }
        return this;
    };
    Container.prototype._findChildAtPosition = function (line, col) {
        var found = undefined;
        this.each(function (node) {
            if (node.atPosition) {
                var foundChild = node.atPosition(line, col);
                if (foundChild) {
                    found = foundChild;
                    return false;
                }
            }
            else if (node.isAtPosition(line, col)) {
                found = node;
                return false;
            }
        });
        return found;
    };
    /**
     * Return the most specific node at the line and column number given.
     * The source location is based on the original parsed location, locations aren't
     * updated as selector nodes are mutated.
     *
     * Note that this location is relative to the location of the first character
     * of the selector, and not the location of the selector in the overall document
     * when used in conjunction with postcss.
     *
     * If not found, returns undefined.
     * @param {number} line The line number of the node to find. (1-based index)
     * @param {number} col  The column number of the node to find. (1-based index)
     */
    Container.prototype.atPosition = function (line, col) {
        if (this.isAtPosition(line, col)) {
            return this._findChildAtPosition(line, col) || this;
        }
        else {
            return undefined;
        }
    };
    Container.prototype._inferEndPosition = function () {
        if (this.last && this.last.source && this.last.source.end) {
            this.source = this.source || {};
            this.source.end = this.source.end || {};
            Object.assign(this.source.end, this.last.source.end);
        }
    };
    Container.prototype.each = function (callback) {
        if (!this.lastEach) {
            this.lastEach = 0;
        }
        if (!this.indexes) {
            this.indexes = {};
        }
        this.lastEach++;
        var id = this.lastEach;
        this.indexes[id] = 0;
        if (!this.length) {
            return undefined;
        }
        var index, result;
        while (this.indexes[id] < this.length) {
            index = this.indexes[id];
            result = callback(this.at(index), index);
            if (result === false) {
                break;
            }
            this.indexes[id] += 1;
        }
        delete this.indexes[id];
        if (result === false) {
            return false;
        }
    };
    Container.prototype.walk = function (callback, depth) {
        if (depth === void 0) { depth = 0; }
        // Bound recursion so a pathologically deep node tree raises a catchable
        // error instead of overflowing the call stack (CVE-2026-9358 / CWE-674).
        if (depth > util_1.MAX_NESTING_DEPTH) {
            throw new Error("Cannot walk selector: nesting depth exceeds the maximum of ".concat(util_1.MAX_NESTING_DEPTH, "."));
        }
        return this.each(function (node, i) {
            var result = callback(node, i);
            if (result !== false && node.length) {
                result = node.walk(callback, depth + 1);
            }
            if (result === false) {
                return false;
            }
        });
    };
    Container.prototype.walkAttributes = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.ATTRIBUTE) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkClasses = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.CLASS) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkCombinators = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.COMBINATOR) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkComments = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.COMMENT) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkIds = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.ID) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkNesting = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.NESTING) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkPseudos = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.PSEUDO) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkTags = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.TAG) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.walkUniversals = function (callback) {
        var _this = this;
        return this.walk(function (selector) {
            if (selector.type === types.UNIVERSAL) {
                return callback.call(_this, selector);
            }
        });
    };
    Container.prototype.split = function (callback) {
        var _this = this;
        var current = [];
        return this.reduce(function (memo, node, index) {
            var split = callback.call(_this, node);
            current.push(node);
            if (split) {
                memo.push(current);
                current = [];
            }
            else if (index === _this.length - 1) {
                memo.push(current);
            }
            return memo;
        }, []);
    };
    Container.prototype.map = function (callback) {
        return this.nodes.map(callback);
    };
    Container.prototype.reduce = function (callback, memo) {
        return this.nodes.reduce(callback, memo);
    };
    Container.prototype.every = function (callback) {
        return this.nodes.every(callback);
    };
    Container.prototype.some = function (callback) {
        return this.nodes.some(callback);
    };
    Container.prototype.filter = function (callback) {
        return this.nodes.filter(callback);
    };
    Container.prototype.sort = function (callback) {
        return this.nodes.sort(callback);
    };
    Container.prototype.toString = function (options) {
        if (options === void 0) { options = {}; }
        return this._stringify(options, 0, (0, util_1.resolveMaxNestingDepth)(options.maxNestingDepth));
    };
    Container.prototype._stringify = function (options, depth, max) {
        var _this = this;
        return this.map(function (child) { return _this._stringifyChild(child, options, depth, max); }).join("");
    };
    // Serialize a child node. Historically `toString` used `this.map(String)`,
    // which leniently coerced anything — including raw arrays inserted via
    // `replaceWith(array)` / `insertBefore` / `insertAfter` (e.g. Tailwind's
    // `:merge()` expansion). Fall back to `String(child)` for values that are not
    // parser nodes so that behaviour is preserved.
    Container.prototype._stringifyChild = function (child, options, depth, max) {
        return typeof child._stringify === "function"
            ? child._stringify(options, depth, max)
            : String(child);
    };
    return Container;
}(node_1.default));
exports.default = Container;
//# sourceMappingURL=container.js.map