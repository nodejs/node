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
import SequentialContainer from './Base/index';
import { checkWithinAccessParams } from "../../utils/checkParams";
import { ContainerIterator } from "../ContainerBase/index";
import { RandomIterator } from "./Base/RandomIterator";
var DequeIterator = /** @class */ (function (_super) {
    __extends(DequeIterator, _super);
    function DequeIterator() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    DequeIterator.prototype.copy = function () {
        return new DequeIterator(this.node, this.size, this.getElementByPos, this.setElementByPos, this.iteratorType);
    };
    return DequeIterator;
}(RandomIterator));
export { DequeIterator };
var Deque = /** @class */ (function (_super) {
    __extends(Deque, _super);
    function Deque(container, bucketSize) {
        if (container === void 0) { container = []; }
        if (bucketSize === void 0) { bucketSize = (1 << 12); }
        var _this = _super.call(this) || this;
        _this.first = 0;
        _this.curFirst = 0;
        _this.last = 0;
        _this.curLast = 0;
        _this.bucketNum = 0;
        _this.map = [];
        var _length;
        if ('size' in container) {
            if (typeof container.size === 'number') {
                _length = container.size;
            }
            else {
                _length = container.size();
            }
        }
        else if ('length' in container) {
            _length = container.length;
        }
        else {
            throw new RangeError('Can\'t get container\'s size!');
        }
        _this.bucketSize = bucketSize;
        _this.bucketNum = Math.max(Math.ceil(_length / _this.bucketSize), 1);
        for (var i = 0; i < _this.bucketNum; ++i) {
            _this.map.push(new Array(_this.bucketSize));
        }
        var needBucketNum = Math.ceil(_length / _this.bucketSize);
        _this.first = _this.last = (_this.bucketNum >> 1) - (needBucketNum >> 1);
        _this.curFirst = _this.curLast = (_this.bucketSize - _length % _this.bucketSize) >> 1;
        container.forEach(function (element) { return _this.pushBack(element); });
        _this.size = _this.size.bind(_this);
        _this.getElementByPos = _this.getElementByPos.bind(_this);
        _this.setElementByPos = _this.setElementByPos.bind(_this);
        return _this;
    }
    /**
     * @description Growth the Deque.
     * @private
     */
    Deque.prototype.reAllocate = function () {
        var newMap = [];
        var addBucketNum = Math.max(this.bucketNum >> 1, 1);
        for (var i = 0; i < addBucketNum; ++i) {
            newMap[i] = new Array(this.bucketSize);
        }
        for (var i = this.first; i < this.bucketNum; ++i) {
            newMap[newMap.length] = this.map[i];
        }
        for (var i = 0; i < this.last; ++i) {
            newMap[newMap.length] = this.map[i];
        }
        newMap[newMap.length] = __spreadArray([], __read(this.map[this.last]), false);
        this.first = addBucketNum;
        this.last = newMap.length - 1;
        for (var i = 0; i < addBucketNum; ++i) {
            newMap[newMap.length] = new Array(this.bucketSize);
        }
        this.map = newMap;
        this.bucketNum = newMap.length;
    };
    /**
     * @description Get the bucket position of the element and the pointer position by index.
     * @param pos The element's index.
     * @private
     */
    Deque.prototype.getElementIndex = function (pos) {
        var offset = this.curFirst + pos + 1;
        var offsetRemainder = offset % this.bucketSize;
        var curNodePointerIndex = offsetRemainder - 1;
        var curNodeBucketIndex = this.first + (offset - offsetRemainder) / this.bucketSize;
        if (offsetRemainder === 0)
            curNodeBucketIndex -= 1;
        curNodeBucketIndex %= this.bucketNum;
        if (curNodePointerIndex < 0)
            curNodePointerIndex += this.bucketSize;
        return { curNodeBucketIndex: curNodeBucketIndex, curNodePointerIndex: curNodePointerIndex };
    };
    Deque.prototype.clear = function () {
        this.map = [[]];
        this.bucketNum = 1;
        this.first = this.last = this.length = 0;
        this.curFirst = this.curLast = this.bucketSize >> 1;
    };
    Deque.prototype.front = function () {
        return this.map[this.first][this.curFirst];
    };
    Deque.prototype.back = function () {
        return this.map[this.last][this.curLast];
    };
    Deque.prototype.begin = function () {
        return new DequeIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.end = function () {
        return new DequeIterator(this.length, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.rBegin = function () {
        return new DequeIterator(this.length - 1, this.size, this.getElementByPos, this.setElementByPos, ContainerIterator.REVERSE);
    };
    Deque.prototype.rEnd = function () {
        return new DequeIterator(-1, this.size, this.getElementByPos, this.setElementByPos, ContainerIterator.REVERSE);
    };
    Deque.prototype.pushBack = function (element) {
        if (this.length) {
            if (this.curLast < this.bucketSize - 1) {
                this.curLast += 1;
            }
            else if (this.last < this.bucketNum - 1) {
                this.last += 1;
                this.curLast = 0;
            }
            else {
                this.last = 0;
                this.curLast = 0;
            }
            if (this.last === this.first &&
                this.curLast === this.curFirst)
                this.reAllocate();
        }
        this.length += 1;
        this.map[this.last][this.curLast] = element;
    };
    Deque.prototype.popBack = function () {
        if (!this.length)
            return;
        this.map[this.last][this.curLast] = undefined;
        if (this.length !== 1) {
            if (this.curLast > 0) {
                this.curLast -= 1;
            }
            else if (this.last > 0) {
                this.last -= 1;
                this.curLast = this.bucketSize - 1;
            }
            else {
                this.last = this.bucketNum - 1;
                this.curLast = this.bucketSize - 1;
            }
        }
        this.length -= 1;
    };
    /**
     * @description Push the element to the front.
     * @param element The element you want to push.
     */
    Deque.prototype.pushFront = function (element) {
        if (this.length) {
            if (this.curFirst > 0) {
                this.curFirst -= 1;
            }
            else if (this.first > 0) {
                this.first -= 1;
                this.curFirst = this.bucketSize - 1;
            }
            else {
                this.first = this.bucketNum - 1;
                this.curFirst = this.bucketSize - 1;
            }
            if (this.first === this.last &&
                this.curFirst === this.curLast)
                this.reAllocate();
        }
        this.length += 1;
        this.map[this.first][this.curFirst] = element;
    };
    /**
     * @description Remove the first element.
     */
    Deque.prototype.popFront = function () {
        if (!this.length)
            return;
        this.map[this.first][this.curFirst] = undefined;
        if (this.length !== 1) {
            if (this.curFirst < this.bucketSize - 1) {
                this.curFirst += 1;
            }
            else if (this.first < this.bucketNum - 1) {
                this.first += 1;
                this.curFirst = 0;
            }
            else {
                this.first = 0;
                this.curFirst = 0;
            }
        }
        this.length -= 1;
    };
    Deque.prototype.forEach = function (callback) {
        for (var i = 0; i < this.length; ++i) {
            callback(this.getElementByPos(i), i);
        }
    };
    Deque.prototype.getElementByPos = function (pos) {
        checkWithinAccessParams(pos, 0, this.length - 1);
        var _a = this.getElementIndex(pos), curNodeBucketIndex = _a.curNodeBucketIndex, curNodePointerIndex = _a.curNodePointerIndex;
        return this.map[curNodeBucketIndex][curNodePointerIndex];
    };
    Deque.prototype.setElementByPos = function (pos, element) {
        checkWithinAccessParams(pos, 0, this.length - 1);
        var _a = this.getElementIndex(pos), curNodeBucketIndex = _a.curNodeBucketIndex, curNodePointerIndex = _a.curNodePointerIndex;
        this.map[curNodeBucketIndex][curNodePointerIndex] = element;
    };
    Deque.prototype.insert = function (pos, element, num) {
        if (num === void 0) { num = 1; }
        checkWithinAccessParams(pos, 0, this.length);
        if (pos === 0) {
            while (num--)
                this.pushFront(element);
        }
        else if (pos === this.length) {
            while (num--)
                this.pushBack(element);
        }
        else {
            var arr = [];
            for (var i = pos; i < this.length; ++i) {
                arr.push(this.getElementByPos(i));
            }
            this.cut(pos - 1);
            for (var i = 0; i < num; ++i)
                this.pushBack(element);
            for (var i = 0; i < arr.length; ++i)
                this.pushBack(arr[i]);
        }
    };
    /**
     * @description Remove all elements after the specified position (excluding the specified position).
     * @param pos The previous position of the first removed element.
     * @example deque.cut(1); // Then deque's size will be 2. deque -> [0, 1]
     */
    Deque.prototype.cut = function (pos) {
        if (pos < 0) {
            this.clear();
            return;
        }
        var _a = this.getElementIndex(pos), curNodeBucketIndex = _a.curNodeBucketIndex, curNodePointerIndex = _a.curNodePointerIndex;
        this.last = curNodeBucketIndex;
        this.curLast = curNodePointerIndex;
        this.length = pos + 1;
    };
    Deque.prototype.eraseElementByPos = function (pos) {
        var _this = this;
        checkWithinAccessParams(pos, 0, this.length - 1);
        if (pos === 0)
            this.popFront();
        else if (pos === this.length - 1)
            this.popBack();
        else {
            var arr = [];
            for (var i = pos + 1; i < this.length; ++i) {
                arr.push(this.getElementByPos(i));
            }
            this.cut(pos);
            this.popBack();
            arr.forEach(function (element) { return _this.pushBack(element); });
        }
    };
    Deque.prototype.eraseElementByValue = function (value) {
        if (!this.length)
            return;
        var arr = [];
        for (var i = 0; i < this.length; ++i) {
            var element = this.getElementByPos(i);
            if (element !== value)
                arr.push(element);
        }
        var _length = arr.length;
        for (var i = 0; i < _length; ++i)
            this.setElementByPos(i, arr[i]);
        this.cut(_length - 1);
    };
    Deque.prototype.eraseElementByIterator = function (iter) {
        // @ts-ignore
        var node = iter.node;
        this.eraseElementByPos(node);
        iter = iter.next();
        return iter;
    };
    Deque.prototype.find = function (element) {
        for (var i = 0; i < this.length; ++i) {
            if (this.getElementByPos(i) === element) {
                return new DequeIterator(i, this.size, this.getElementByPos, this.setElementByPos);
            }
        }
        return this.end();
    };
    Deque.prototype.reverse = function () {
        var l = 0;
        var r = this.length - 1;
        while (l < r) {
            var tmp = this.getElementByPos(l);
            this.setElementByPos(l, this.getElementByPos(r));
            this.setElementByPos(r, tmp);
            l += 1;
            r -= 1;
        }
    };
    Deque.prototype.unique = function () {
        if (this.length <= 1)
            return;
        var index = 1;
        var pre = this.getElementByPos(0);
        for (var i = 1; i < this.length; ++i) {
            var cur = this.getElementByPos(i);
            if (cur !== pre) {
                pre = cur;
                this.setElementByPos(index++, cur);
            }
        }
        while (this.length > index)
            this.popBack();
    };
    Deque.prototype.sort = function (cmp) {
        var arr = [];
        for (var i = 0; i < this.length; ++i) {
            arr.push(this.getElementByPos(i));
        }
        arr.sort(cmp);
        for (var i = 0; i < this.length; ++i)
            this.setElementByPos(i, arr[i]);
    };
    /**
     * @description Remove as much useless space as possible.
     */
    Deque.prototype.shrinkToFit = function () {
        if (!this.length)
            return;
        var arr = [];
        this.forEach(function (element) { return arr.push(element); });
        this.bucketNum = Math.max(Math.ceil(this.length / this.bucketSize), 1);
        this.length = this.first = this.last = this.curFirst = this.curLast = 0;
        this.map = [];
        for (var i = 0; i < this.bucketNum; ++i) {
            this.map.push(new Array(this.bucketSize));
        }
        for (var i = 0; i < arr.length; ++i)
            this.pushBack(arr[i]);
    };
    Deque.prototype[Symbol.iterator] = function () {
        return function () {
            var i;
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        i = 0;
                        _a.label = 1;
                    case 1:
                        if (!(i < this.length)) return [3 /*break*/, 4];
                        return [4 /*yield*/, this.getElementByPos(i)];
                    case 2:
                        _a.sent();
                        _a.label = 3;
                    case 3:
                        ++i;
                        return [3 /*break*/, 1];
                    case 4: return [2 /*return*/];
                }
            });
        }.bind(this)();
    };
    return Deque;
}(SequentialContainer));
export default Deque;
