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
import TreeContainer from './Base/index';
import { ContainerIterator } from "../ContainerBase/index";
import { checkWithinAccessParams } from "../../utils/checkParams";
import TreeIterator from './Base/TreeIterator';
var OrderedSetIterator = /** @class */ (function (_super) {
    __extends(OrderedSetIterator, _super);
    function OrderedSetIterator() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedSetIterator.prototype, "pointer", {
        get: function () {
            if (this.node === this.header) {
                throw new RangeError('OrderedSet iterator access denied!');
            }
            return this.node.key;
        },
        enumerable: false,
        configurable: true
    });
    OrderedSetIterator.prototype.copy = function () {
        return new OrderedSetIterator(this.node, this.header, this.iteratorType);
    };
    return OrderedSetIterator;
}(TreeIterator));
export { OrderedSetIterator };
var OrderedSet = /** @class */ (function (_super) {
    __extends(OrderedSet, _super);
    function OrderedSet(container, cmp) {
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
                        return [4 /*yield*/, curNode.key];
                    case 2:
                        _a.sent();
                        return [5 /*yield**/, __values(this.iterationFunc(curNode.right))];
                    case 3:
                        _a.sent();
                        return [2 /*return*/];
                }
            });
        };
        container.forEach(function (element) { return _this.insert(element); });
        _this.iterationFunc = _this.iterationFunc.bind(_this);
        return _this;
    }
    OrderedSet.prototype.begin = function () {
        return new OrderedSetIterator(this.header.left || this.header, this.header);
    };
    OrderedSet.prototype.end = function () {
        return new OrderedSetIterator(this.header, this.header);
    };
    OrderedSet.prototype.rBegin = function () {
        return new OrderedSetIterator(this.header.right || this.header, this.header, ContainerIterator.REVERSE);
    };
    OrderedSet.prototype.rEnd = function () {
        return new OrderedSetIterator(this.header, this.header, ContainerIterator.REVERSE);
    };
    OrderedSet.prototype.front = function () {
        return this.header.left ? this.header.left.key : undefined;
    };
    OrderedSet.prototype.back = function () {
        return this.header.right ? this.header.right.key : undefined;
    };
    OrderedSet.prototype.forEach = function (callback) {
        var e_1, _a;
        var index = 0;
        try {
            for (var _b = __values(this), _c = _b.next(); !_c.done; _c = _b.next()) {
                var element = _c.value;
                callback(element, index++);
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
    OrderedSet.prototype.getElementByPos = function (pos) {
        var e_2, _a;
        checkWithinAccessParams(pos, 0, this.length - 1);
        var res;
        var index = 0;
        try {
            for (var _b = __values(this), _c = _b.next(); !_c.done; _c = _b.next()) {
                var element = _c.value;
                if (index === pos) {
                    res = element;
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
    /**
     * @description Insert element to set.
     * @param key The key want to insert.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     */
    OrderedSet.prototype.insert = function (key, hint) {
        this.set(key, undefined, hint);
    };
    OrderedSet.prototype.find = function (element) {
        var curNode = this.findElementNode(this.root, element);
        if (curNode !== undefined) {
            return new OrderedSetIterator(curNode, this.header);
        }
        return this.end();
    };
    OrderedSet.prototype.lowerBound = function (key) {
        var resNode = this._lowerBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    };
    OrderedSet.prototype.upperBound = function (key) {
        var resNode = this._upperBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    };
    OrderedSet.prototype.reverseLowerBound = function (key) {
        var resNode = this._reverseLowerBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    };
    OrderedSet.prototype.reverseUpperBound = function (key) {
        var resNode = this._reverseUpperBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    };
    OrderedSet.prototype.union = function (other) {
        var _this = this;
        other.forEach(function (element) { return _this.insert(element); });
    };
    OrderedSet.prototype[Symbol.iterator] = function () {
        return this.iterationFunc(this.root);
    };
    return OrderedSet;
}(TreeContainer));
export default OrderedSet;
