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
var __spreadArray = (this && this.__spreadArray) || function (to, from, pack) {
    if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
        if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
        }
    }
    return to.concat(ar || Array.prototype.slice.call(from));
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
import SequentialContainer from './Base/index';
import { checkWithinAccessParams } from "../../utils/checkParams";
import { ContainerIterator } from "../ContainerBase/index";
import { RandomIterator } from "./Base/RandomIterator";
var VectorIterator = /** @class */ (function (_super) {
    __extends(VectorIterator, _super);
    function VectorIterator() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    VectorIterator.prototype.copy = function () {
        return new VectorIterator(this.node, this.size, this.getElementByPos, this.setElementByPos, this.iteratorType);
    };
    return VectorIterator;
}(RandomIterator));
export { VectorIterator };
var Vector = /** @class */ (function (_super) {
    __extends(Vector, _super);
    /**
     * @description Vector's constructor.
     * @param container Initialize container, must have a forEach function.
     * @param copy When the container is an array, you can choose to directly operate on the original object of
     *             the array or perform a shallow copy. The default is shallow copy.
     */
    function Vector(container, copy) {
        if (container === void 0) { container = []; }
        if (copy === void 0) { copy = true; }
        var _this = _super.call(this) || this;
        if (Array.isArray(container)) {
            _this.vector = copy ? __spreadArray([], __read(container), false) : container;
            _this.length = container.length;
        }
        else {
            _this.vector = [];
            container.forEach(function (element) { return _this.pushBack(element); });
        }
        _this.size = _this.size.bind(_this);
        _this.getElementByPos = _this.getElementByPos.bind(_this);
        _this.setElementByPos = _this.setElementByPos.bind(_this);
        return _this;
    }
    Vector.prototype.clear = function () {
        this.length = 0;
        this.vector.length = 0;
    };
    Vector.prototype.begin = function () {
        return new VectorIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    };
    Vector.prototype.end = function () {
        return new VectorIterator(this.length, this.size, this.getElementByPos, this.setElementByPos);
    };
    Vector.prototype.rBegin = function () {
        return new VectorIterator(this.length - 1, this.size, this.getElementByPos, this.setElementByPos, ContainerIterator.REVERSE);
    };
    Vector.prototype.rEnd = function () {
        return new VectorIterator(-1, this.size, this.getElementByPos, this.setElementByPos, ContainerIterator.REVERSE);
    };
    Vector.prototype.front = function () {
        return this.vector[0];
    };
    Vector.prototype.back = function () {
        return this.vector[this.length - 1];
    };
    Vector.prototype.forEach = function (callback) {
        for (var i = 0; i < this.length; ++i) {
            callback(this.vector[i], i);
        }
    };
    Vector.prototype.getElementByPos = function (pos) {
        checkWithinAccessParams(pos, 0, this.length - 1);
        return this.vector[pos];
    };
    Vector.prototype.eraseElementByPos = function (pos) {
        checkWithinAccessParams(pos, 0, this.length - 1);
        this.vector.splice(pos, 1);
        this.length -= 1;
    };
    Vector.prototype.eraseElementByValue = function (value) {
        var index = 0;
        for (var i = 0; i < this.length; ++i) {
            if (this.vector[i] !== value) {
                this.vector[index++] = this.vector[i];
            }
        }
        this.length = this.vector.length = index;
    };
    Vector.prototype.eraseElementByIterator = function (iter) {
        // @ts-ignore
        var node = iter.node;
        iter = iter.next();
        this.eraseElementByPos(node);
        return iter;
    };
    Vector.prototype.pushBack = function (element) {
        this.vector.push(element);
        this.length += 1;
    };
    Vector.prototype.popBack = function () {
        if (!this.length)
            return;
        this.vector.pop();
        this.length -= 1;
    };
    Vector.prototype.setElementByPos = function (pos, element) {
        checkWithinAccessParams(pos, 0, this.length - 1);
        this.vector[pos] = element;
    };
    Vector.prototype.insert = function (pos, element, num) {
        var _a;
        if (num === void 0) { num = 1; }
        checkWithinAccessParams(pos, 0, this.length);
        (_a = this.vector).splice.apply(_a, __spreadArray([pos, 0], __read(new Array(num).fill(element)), false));
        this.length += num;
    };
    Vector.prototype.find = function (element) {
        for (var i = 0; i < this.length; ++i) {
            if (this.vector[i] === element) {
                return new VectorIterator(i, this.size, this.getElementByPos, this.getElementByPos);
            }
        }
        return this.end();
    };
    Vector.prototype.reverse = function () {
        this.vector.reverse();
    };
    Vector.prototype.unique = function () {
        var index = 1;
        for (var i = 1; i < this.length; ++i) {
            if (this.vector[i] !== this.vector[i - 1]) {
                this.vector[index++] = this.vector[i];
            }
        }
        this.length = this.vector.length = index;
    };
    Vector.prototype.sort = function (cmp) {
        this.vector.sort(cmp);
    };
    Vector.prototype[Symbol.iterator] = function () {
        return function () {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0: return [5 /*yield**/, __values(this.vector)];
                    case 1: return [2 /*return*/, _a.sent()];
                }
            });
        }.bind(this)();
    };
    return Vector;
}(SequentialContainer));
export default Vector;
