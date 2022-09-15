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
import { Base } from "../ContainerBase/index";
var PriorityQueue = /** @class */ (function (_super) {
    __extends(PriorityQueue, _super);
    /**
     * @description PriorityQueue's constructor.
     * @param container Initialize container, must have a forEach function.
     * @param cmp Compare function.
     * @param copy When the container is an array, you can choose to directly operate on the original object of
     *             the array or perform a shallow copy. The default is shallow copy.
     */
    function PriorityQueue(container, cmp, copy) {
        var _a;
        if (container === void 0) { container = []; }
        if (cmp === void 0) { cmp = function (x, y) {
            if (x > y)
                return -1;
            if (x < y)
                return 1;
            return 0;
        }; }
        if (copy === void 0) { copy = true; }
        var _this = _super.call(this) || this;
        _this.cmp = cmp;
        if (Array.isArray(container)) {
            _this.priorityQueue = copy ? __spreadArray([], __read(container), false) : container;
        }
        else {
            _this.priorityQueue = [];
            container.forEach(function (element) { return _this.priorityQueue.push(element); });
        }
        _this.length = _this.priorityQueue.length;
        for (var parent_1 = (_this.length - 1) >> 1; parent_1 >= 0; --parent_1) {
            var curParent = parent_1;
            var curChild = (curParent << 1) | 1;
            while (curChild < _this.length) {
                var left = curChild;
                var right = left + 1;
                var minChild = left;
                if (right < _this.length &&
                    _this.cmp(_this.priorityQueue[left], _this.priorityQueue[right]) > 0) {
                    minChild = right;
                }
                if (_this.cmp(_this.priorityQueue[curParent], _this.priorityQueue[minChild]) <= 0)
                    break;
                _a = __read([_this.priorityQueue[minChild], _this.priorityQueue[curParent]], 2), _this.priorityQueue[curParent] = _a[0], _this.priorityQueue[minChild] = _a[1];
                curParent = minChild;
                curChild = (curParent << 1) | 1;
            }
        }
        return _this;
    }
    /**
     * @description Adjusting parent's children to suit the nature of the heap.
     * @param parent Parent's index.
     * @private
     */
    PriorityQueue.prototype.adjust = function (parent) {
        var _a, _b;
        var left = (parent << 1) | 1;
        var right = (parent << 1) + 2;
        if (left < this.length &&
            this.cmp(this.priorityQueue[parent], this.priorityQueue[left]) > 0) {
            _a = __read([this.priorityQueue[left], this.priorityQueue[parent]], 2), this.priorityQueue[parent] = _a[0], this.priorityQueue[left] = _a[1];
        }
        if (right < this.length &&
            this.cmp(this.priorityQueue[parent], this.priorityQueue[right]) > 0) {
            _b = __read([this.priorityQueue[right], this.priorityQueue[parent]], 2), this.priorityQueue[parent] = _b[0], this.priorityQueue[right] = _b[1];
        }
    };
    PriorityQueue.prototype.clear = function () {
        this.length = 0;
        this.priorityQueue.length = 0;
    };
    /**
     * @description Push element into a container in order.
     * @param element The element you want to push.
     */
    PriorityQueue.prototype.push = function (element) {
        this.priorityQueue.push(element);
        this.length += 1;
        if (this.length === 1)
            return;
        var curNode = this.length - 1;
        while (curNode > 0) {
            var parent_2 = (curNode - 1) >> 1;
            if (this.cmp(this.priorityQueue[parent_2], element) <= 0)
                break;
            this.adjust(parent_2);
            curNode = parent_2;
        }
    };
    /**
     * @description Removes the top element.
     */
    PriorityQueue.prototype.pop = function () {
        if (!this.length)
            return;
        var last = this.priorityQueue[this.length - 1];
        this.length -= 1;
        var parent = 0;
        while (parent < this.length) {
            var left = (parent << 1) | 1;
            var right = (parent << 1) + 2;
            if (left >= this.length)
                break;
            var minChild = left;
            if (right < this.length &&
                this.cmp(this.priorityQueue[left], this.priorityQueue[right]) > 0) {
                minChild = right;
            }
            if (this.cmp(this.priorityQueue[minChild], last) >= 0)
                break;
            this.priorityQueue[parent] = this.priorityQueue[minChild];
            parent = minChild;
        }
        this.priorityQueue[parent] = last;
        this.priorityQueue.pop();
    };
    /**
     * @description Accesses the top element.
     */
    PriorityQueue.prototype.top = function () {
        return this.priorityQueue[0];
    };
    return PriorityQueue;
}(Base));
export default PriorityQueue;
