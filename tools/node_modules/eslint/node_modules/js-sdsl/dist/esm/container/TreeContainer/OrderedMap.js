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
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
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
import { ContainerIterator } from "../ContainerBase/index";
import { checkWithinAccessParams } from "../../utils/checkParams";
import TreeContainer from './Base/index';
import TreeIterator from './Base/TreeIterator';
var OrderedMapIterator = /** @class */ (function (_super) {
    __extends(OrderedMapIterator, _super);
    function OrderedMapIterator() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedMapIterator.prototype, "pointer", {
        get: function () {
            var _this = this;
            if (this.node === this.header) {
                throw new RangeError('OrderedMap iterator access denied');
            }
            return new Proxy([], {
                get: function (_, props) {
                    if (props === '0')
                        return _this.node.key;
                    else if (props === '1')
                        return _this.node.value;
                },
                set: function (_, props, newValue) {
                    if (props !== '1') {
                        throw new TypeError('props must be 1');
                    }
                    _this.node.value = newValue;
                    return true;
                }
            });
        },
        enumerable: false,
        configurable: true
    });
    OrderedMapIterator.prototype.copy = function () {
        return new OrderedMapIterator(this.node, this.header, this.iteratorType);
    };
    return OrderedMapIterator;
}(TreeIterator));
export { OrderedMapIterator };
var OrderedMap = /** @class */ (function (_super) {
    __extends(OrderedMap, _super);
    function OrderedMap(container, cmp) {
        if (container === void 0) { container = []; }
        var _this = _super.call(this, cmp) || this;
        _this.iterationFunc = function (curNode) {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        if (curNode === undefined)
                            return [2 /*return*/];
                        return [5 /*yield**/, __values(this.iterationFunc(curNode.left))];
                    case 1:
                        _a.sent();
                        return [4 /*yield*/, [curNode.key, curNode.value]];
                    case 2:
                        _a.sent();
                        return [5 /*yield**/, __values(this.iterationFunc(curNode.right))];
                    case 3:
                        _a.sent();
                        return [2 /*return*/];
                }
            });
        };
        _this.iterationFunc = _this.iterationFunc.bind(_this);
        container.forEach(function (_a) {
            var _b = __read(_a, 2), key = _b[0], value = _b[1];
            return _this.setElement(key, value);
        });
        return _this;
    }
    OrderedMap.prototype.begin = function () {
        return new OrderedMapIterator(this.header.left || this.header, this.header);
    };
    OrderedMap.prototype.end = function () {
        return new OrderedMapIterator(this.header, this.header);
    };
    OrderedMap.prototype.rBegin = function () {
        return new OrderedMapIterator(this.header.right || this.header, this.header, ContainerIterator.REVERSE);
    };
    OrderedMap.prototype.rEnd = function () {
        return new OrderedMapIterator(this.header, this.header, ContainerIterator.REVERSE);
    };
    OrderedMap.prototype.front = function () {
        if (!this.length)
            return undefined;
        var minNode = this.header.left;
        return [minNode.key, minNode.value];
    };
    OrderedMap.prototype.back = function () {
        if (!this.length)
            return undefined;
        var maxNode = this.header.right;
        return [maxNode.key, maxNode.value];
    };
    OrderedMap.prototype.forEach = function (callback) {
        var e_1, _a;
        var index = 0;
        try {
            for (var _b = __values(this), _c = _b.next(); !_c.done; _c = _b.next()) {
                var pair = _c.value;
                callback(pair, index++);
            }
        }
        catch (e_1_1) { e_1 = { error: e_1_1 }; }
        finally {
            try {
                if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
            }
            finally { if (e_1) throw e_1.error; }
        }
    };
    OrderedMap.prototype.lowerBound = function (key) {
        var resNode = this._lowerBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    };
    OrderedMap.prototype.upperBound = function (key) {
        var resNode = this._upperBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    };
    OrderedMap.prototype.reverseLowerBound = function (key) {
        var resNode = this._reverseLowerBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    };
    OrderedMap.prototype.reverseUpperBound = function (key) {
        var resNode = this._reverseUpperBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    };
    /**
     * @description Insert a key-value pair or set value by the given key.
     * @param key The key want to insert.
     * @param value The value want to set.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     */
    OrderedMap.prototype.setElement = function (key, value, hint) {
        this.set(key, value, hint);
    };
    OrderedMap.prototype.find = function (key) {
        var curNode = this.findElementNode(this.root, key);
        if (curNode !== undefined) {
            return new OrderedMapIterator(curNode, this.header);
        }
        return this.end();
    };
    /**
     * @description Get the value of the element of the specified key.
     */
    OrderedMap.prototype.getElementByKey = function (key) {
        var curNode = this.findElementNode(this.root, key);
        return curNode ? curNode.value : undefined;
    };
    OrderedMap.prototype.getElementByPos = function (pos) {
        var e_2, _a;
        checkWithinAccessParams(pos, 0, this.length - 1);
        var res;
        var index = 0;
        try {
            for (var _b = __values(this), _c = _b.next(); !_c.done; _c = _b.next()) {
                var pair = _c.value;
                if (index === pos) {
                    res = pair;
                    break;
                }
                index += 1;
            }
        }
        catch (e_2_1) { e_2 = { error: e_2_1 }; }
        finally {
            try {
                if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
            }
            finally { if (e_2) throw e_2.error; }
        }
        return res;
    };
    OrderedMap.prototype.union = function (other) {
        var _this = this;
        other.forEach(function (_a) {
            var _b = __read(_a, 2), key = _b[0], value = _b[1];
            return _this.setElement(key, value);
        });
    };
    OrderedMap.prototype[Symbol.iterator] = function () {
        return this.iterationFunc(this.root);
    };
    return OrderedMap;
}(TreeContainer));
export default OrderedMap;
