(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports) :
    typeof define === 'function' && define.amd ? define(['exports'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.sdsl = {}));
})(this, (function (exports) { 'use strict';

    /******************************************************************************
    Copyright (c) Microsoft Corporation.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
    REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
    LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
    OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
    PERFORMANCE OF THIS SOFTWARE.
    ***************************************************************************** */
    /* global Reflect, Promise */

    var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };

    function __extends(d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    }

    function __generator(thisArg, body) {
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
    }

    function __values(o) {
        var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
        if (m) return m.call(o);
        if (o && typeof o.length === "number") return {
            next: function () {
                if (o && i >= o.length) o = void 0;
                return { value: o && o[i++], done: !o };
            }
        };
        throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
    }

    function __read(o, n) {
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
    }

    function __spreadArray(to, from, pack) {
        if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
            if (ar || !(i in from)) {
                if (!ar) ar = Array.prototype.slice.call(from, 0, i);
                ar[i] = from[i];
            }
        }
        return to.concat(ar || Array.prototype.slice.call(from));
    }

    var ContainerIterator = /** @class */ (function () {
        function ContainerIterator(iteratorType) {
            if (iteratorType === void 0) { iteratorType = ContainerIterator.NORMAL; }
            this.iteratorType = iteratorType;
        }
        ContainerIterator.NORMAL = false;
        ContainerIterator.REVERSE = true;
        return ContainerIterator;
    }());
    var Base = /** @class */ (function () {
        function Base() {
            /**
             * @description Container's size.
             * @protected
             */
            this.length = 0;
        }
        /**
         * @return The size of the container.
         */
        Base.prototype.size = function () {
            return this.length;
        };
        /**
         * @return Boolean about if the container is empty.
         */
        Base.prototype.empty = function () {
            return this.length === 0;
        };
        return Base;
    }());
    var Container = /** @class */ (function (_super) {
        __extends(Container, _super);
        function Container() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        return Container;
    }(Base));

    var Stack = /** @class */ (function (_super) {
        __extends(Stack, _super);
        function Stack(container) {
            if (container === void 0) { container = []; }
            var _this = _super.call(this) || this;
            _this.stack = [];
            container.forEach(function (element) { return _this.push(element); });
            return _this;
        }
        Stack.prototype.clear = function () {
            this.length = 0;
            this.stack.length = 0;
        };
        /**
         * @description Insert element to stack's end.
         */
        Stack.prototype.push = function (element) {
            this.stack.push(element);
            this.length += 1;
        };
        /**
         * @description Removes the end element.
         */
        Stack.prototype.pop = function () {
            this.stack.pop();
            if (this.length > 0)
                this.length -= 1;
        };
        /**
         * @description Accesses the end element.
         */
        Stack.prototype.top = function () {
            return this.stack[this.length - 1];
        };
        return Stack;
    }(Base));

    var SequentialContainer = /** @class */ (function (_super) {
        __extends(SequentialContainer, _super);
        function SequentialContainer() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        return SequentialContainer;
    }(Container));

    /**
     * @description Check if access is out of bounds.
     * @param pos The position want to access.
     * @param lower The lower bound.
     * @param upper The upper bound.
     * @return Boolean about if access is out of bounds.
     */
    function checkWithinAccessParams(pos, lower, upper) {
        if (pos < lower || pos > upper) {
            throw new RangeError();
        }
    }

    var RandomIterator = /** @class */ (function (_super) {
        __extends(RandomIterator, _super);
        function RandomIterator(index, size, getElementByPos, setElementByPos, iteratorType) {
            var _this = _super.call(this, iteratorType) || this;
            _this.node = index;
            _this.size = size;
            _this.getElementByPos = getElementByPos;
            _this.setElementByPos = setElementByPos;
            if (_this.iteratorType === ContainerIterator.NORMAL) {
                _this.pre = function () {
                    if (this.node === 0) {
                        throw new RangeError('Deque iterator access denied!');
                    }
                    this.node -= 1;
                    return this;
                };
                _this.next = function () {
                    if (this.node === this.size()) {
                        throw new RangeError('Deque Iterator access denied!');
                    }
                    this.node += 1;
                    return this;
                };
            }
            else {
                _this.pre = function () {
                    if (this.node === this.size() - 1) {
                        throw new RangeError('Deque iterator access denied!');
                    }
                    this.node += 1;
                    return this;
                };
                _this.next = function () {
                    if (this.node === -1) {
                        throw new RangeError('Deque iterator access denied!');
                    }
                    this.node -= 1;
                    return this;
                };
            }
            return _this;
        }
        Object.defineProperty(RandomIterator.prototype, "pointer", {
            get: function () {
                checkWithinAccessParams(this.node, 0, this.size() - 1);
                return this.getElementByPos(this.node);
            },
            set: function (newValue) {
                checkWithinAccessParams(this.node, 0, this.size() - 1);
                this.setElementByPos(this.node, newValue);
            },
            enumerable: false,
            configurable: true
        });
        RandomIterator.prototype.equals = function (obj) {
            return this.node === obj.node;
        };
        return RandomIterator;
    }(ContainerIterator));

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

    var Queue = /** @class */ (function (_super) {
        __extends(Queue, _super);
        function Queue(container) {
            if (container === void 0) { container = []; }
            var _this = _super.call(this) || this;
            _this.queue = new Deque(container);
            _this.length = _this.queue.size();
            return _this;
        }
        Queue.prototype.clear = function () {
            this.queue.clear();
            this.length = 0;
        };
        /**
         * @description Inserts element to queue's end.
         */
        Queue.prototype.push = function (element) {
            this.queue.pushBack(element);
            this.length += 1;
        };
        /**
         * @description Removes the first element.
         */
        Queue.prototype.pop = function () {
            this.queue.popFront();
            if (this.length)
                this.length -= 1;
        };
        /**
         * @description Access the first element.
         */
        Queue.prototype.front = function () {
            return this.queue.front();
        };
        return Queue;
    }(Base));

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

    var LinkNode = /** @class */ (function () {
        function LinkNode(element) {
            this.value = undefined;
            this.pre = undefined;
            this.next = undefined;
            this.value = element;
        }
        return LinkNode;
    }());
    var LinkListIterator = /** @class */ (function (_super) {
        __extends(LinkListIterator, _super);
        function LinkListIterator(node, header, iteratorType) {
            var _this = _super.call(this, iteratorType) || this;
            _this.node = node;
            _this.header = header;
            if (_this.iteratorType === ContainerIterator.NORMAL) {
                _this.pre = function () {
                    if (this.node.pre === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.pre;
                    return this;
                };
                _this.next = function () {
                    if (this.node === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.next;
                    return this;
                };
            }
            else {
                _this.pre = function () {
                    if (this.node.next === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.next;
                    return this;
                };
                _this.next = function () {
                    if (this.node === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.pre;
                    return this;
                };
            }
            return _this;
        }
        Object.defineProperty(LinkListIterator.prototype, "pointer", {
            get: function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                return this.node.value;
            },
            set: function (newValue) {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node.value = newValue;
            },
            enumerable: false,
            configurable: true
        });
        LinkListIterator.prototype.equals = function (obj) {
            return this.node === obj.node;
        };
        LinkListIterator.prototype.copy = function () {
            return new LinkListIterator(this.node, this.header, this.iteratorType);
        };
        return LinkListIterator;
    }(ContainerIterator));
    var LinkList = /** @class */ (function (_super) {
        __extends(LinkList, _super);
        function LinkList(container) {
            if (container === void 0) { container = []; }
            var _this = _super.call(this) || this;
            _this.header = new LinkNode();
            _this.head = undefined;
            _this.tail = undefined;
            container.forEach(function (element) { return _this.pushBack(element); });
            return _this;
        }
        LinkList.prototype.clear = function () {
            this.length = 0;
            this.head = this.tail = undefined;
            this.header.pre = this.header.next = undefined;
        };
        LinkList.prototype.begin = function () {
            return new LinkListIterator(this.head || this.header, this.header);
        };
        LinkList.prototype.end = function () {
            return new LinkListIterator(this.header, this.header);
        };
        LinkList.prototype.rBegin = function () {
            return new LinkListIterator(this.tail || this.header, this.header, ContainerIterator.REVERSE);
        };
        LinkList.prototype.rEnd = function () {
            return new LinkListIterator(this.header, this.header, ContainerIterator.REVERSE);
        };
        LinkList.prototype.front = function () {
            return this.head ? this.head.value : undefined;
        };
        LinkList.prototype.back = function () {
            return this.tail ? this.tail.value : undefined;
        };
        LinkList.prototype.forEach = function (callback) {
            if (!this.length)
                return;
            var curNode = this.head;
            var index = 0;
            while (curNode !== this.header) {
                callback(curNode.value, index++);
                curNode = curNode.next;
            }
        };
        LinkList.prototype.getElementByPos = function (pos) {
            checkWithinAccessParams(pos, 0, this.length - 1);
            var curNode = this.head;
            while (pos--) {
                curNode = curNode.next;
            }
            return curNode.value;
        };
        LinkList.prototype.eraseElementByPos = function (pos) {
            checkWithinAccessParams(pos, 0, this.length - 1);
            if (pos === 0)
                this.popFront();
            else if (pos === this.length - 1)
                this.popBack();
            else {
                var curNode = this.head;
                while (pos--) {
                    curNode = curNode.next;
                }
                curNode = curNode;
                var pre = curNode.pre;
                var next = curNode.next;
                next.pre = pre;
                pre.next = next;
                this.length -= 1;
            }
        };
        LinkList.prototype.eraseElementByValue = function (value) {
            while (this.head && this.head.value === value)
                this.popFront();
            while (this.tail && this.tail.value === value)
                this.popBack();
            if (!this.head)
                return;
            var curNode = this.head;
            while (curNode !== this.header) {
                if (curNode.value === value) {
                    var pre = curNode.pre;
                    var next = curNode.next;
                    if (next)
                        next.pre = pre;
                    if (pre)
                        pre.next = next;
                    this.length -= 1;
                }
                curNode = curNode.next;
            }
        };
        LinkList.prototype.eraseElementByIterator = function (iter) {
            // @ts-ignore
            var node = iter.node;
            if (node === this.header) {
                throw new RangeError('Invalid iterator');
            }
            iter = iter.next();
            if (this.head === node)
                this.popFront();
            else if (this.tail === node)
                this.popBack();
            else {
                var pre = node.pre;
                var next = node.next;
                if (next)
                    next.pre = pre;
                if (pre)
                    pre.next = next;
                this.length -= 1;
            }
            return iter;
        };
        LinkList.prototype.pushBack = function (element) {
            this.length += 1;
            var newTail = new LinkNode(element);
            if (!this.tail) {
                this.head = this.tail = newTail;
                this.header.next = this.head;
                this.head.pre = this.header;
            }
            else {
                this.tail.next = newTail;
                newTail.pre = this.tail;
                this.tail = newTail;
            }
            this.tail.next = this.header;
            this.header.pre = this.tail;
        };
        LinkList.prototype.popBack = function () {
            if (!this.tail)
                return;
            this.length -= 1;
            if (this.head === this.tail) {
                this.head = this.tail = undefined;
                this.header.next = undefined;
            }
            else {
                this.tail = this.tail.pre;
                if (this.tail)
                    this.tail.next = undefined;
            }
            this.header.pre = this.tail;
            if (this.tail)
                this.tail.next = this.header;
        };
        LinkList.prototype.setElementByPos = function (pos, element) {
            checkWithinAccessParams(pos, 0, this.length - 1);
            var curNode = this.head;
            while (pos--) {
                curNode = curNode.next;
            }
            curNode.value = element;
        };
        LinkList.prototype.insert = function (pos, element, num) {
            if (num === void 0) { num = 1; }
            checkWithinAccessParams(pos, 0, this.length);
            if (num <= 0)
                return;
            if (pos === 0) {
                while (num--)
                    this.pushFront(element);
            }
            else if (pos === this.length) {
                while (num--)
                    this.pushBack(element);
            }
            else {
                var curNode = this.head;
                for (var i = 1; i < pos; ++i) {
                    curNode = curNode.next;
                }
                var next = curNode.next;
                this.length += num;
                while (num--) {
                    curNode.next = new LinkNode(element);
                    curNode.next.pre = curNode;
                    curNode = curNode.next;
                }
                curNode.next = next;
                if (next)
                    next.pre = curNode;
            }
        };
        LinkList.prototype.find = function (element) {
            if (!this.head)
                return this.end();
            var curNode = this.head;
            while (curNode !== this.header) {
                if (curNode.value === element) {
                    return new LinkListIterator(curNode, this.header);
                }
                curNode = curNode.next;
            }
            return this.end();
        };
        LinkList.prototype.reverse = function () {
            if (this.length <= 1)
                return;
            var pHead = this.head;
            var pTail = this.tail;
            var cnt = 0;
            while ((cnt << 1) < this.length) {
                var tmp = pHead.value;
                pHead.value = pTail.value;
                pTail.value = tmp;
                pHead = pHead.next;
                pTail = pTail.pre;
                cnt += 1;
            }
        };
        LinkList.prototype.unique = function () {
            if (this.length <= 1)
                return;
            var curNode = this.head;
            while (curNode !== this.header) {
                var tmpNode = curNode;
                while (tmpNode.next && tmpNode.value === tmpNode.next.value) {
                    tmpNode = tmpNode.next;
                    this.length -= 1;
                }
                curNode.next = tmpNode.next;
                if (curNode.next)
                    curNode.next.pre = curNode;
                curNode = curNode.next;
            }
        };
        LinkList.prototype.sort = function (cmp) {
            if (this.length <= 1)
                return;
            var arr = [];
            this.forEach(function (element) { return arr.push(element); });
            arr.sort(cmp);
            var curNode = this.head;
            arr.forEach(function (element) {
                curNode.value = element;
                curNode = curNode.next;
            });
        };
        /**
         * @description Push an element to the front.
         * @param element The element you want to push.
         */
        LinkList.prototype.pushFront = function (element) {
            this.length += 1;
            var newHead = new LinkNode(element);
            if (!this.head) {
                this.head = this.tail = newHead;
                this.tail.next = this.header;
                this.header.pre = this.tail;
            }
            else {
                newHead.next = this.head;
                this.head.pre = newHead;
                this.head = newHead;
            }
            this.header.next = this.head;
            this.head.pre = this.header;
        };
        /**
         * @description Removes the first element.
         */
        LinkList.prototype.popFront = function () {
            if (!this.head)
                return;
            this.length -= 1;
            if (this.head === this.tail) {
                this.head = this.tail = undefined;
                this.header.pre = this.tail;
            }
            else {
                this.head = this.head.next;
                if (this.head)
                    this.head.pre = this.header;
            }
            this.header.next = this.head;
        };
        /**
         * @description Merges two sorted lists.
         * @param list The other list you want to merge (must be sorted).
         */
        LinkList.prototype.merge = function (list) {
            var _this = this;
            if (!this.head) {
                list.forEach(function (element) { return _this.pushBack(element); });
                return;
            }
            var curNode = this.head;
            list.forEach(function (element) {
                while (curNode &&
                    curNode !== _this.header &&
                    curNode.value <= element) {
                    curNode = curNode.next;
                }
                if (curNode === _this.header) {
                    _this.pushBack(element);
                    curNode = _this.tail;
                }
                else if (curNode === _this.head) {
                    _this.pushFront(element);
                    curNode = _this.head;
                }
                else {
                    _this.length += 1;
                    var pre = curNode.pre;
                    pre.next = new LinkNode(element);
                    pre.next.pre = pre;
                    pre.next.next = curNode;
                    curNode.pre = pre.next;
                }
            });
        };
        LinkList.prototype[Symbol.iterator] = function () {
            return function () {
                var curNode;
                return __generator(this, function (_a) {
                    switch (_a.label) {
                        case 0:
                            if (!this.head)
                                return [2 /*return*/];
                            curNode = this.head;
                            _a.label = 1;
                        case 1:
                            if (!(curNode !== this.header)) return [3 /*break*/, 3];
                            return [4 /*yield*/, curNode.value];
                        case 2:
                            _a.sent();
                            curNode = curNode.next;
                            return [3 /*break*/, 1];
                        case 3: return [2 /*return*/];
                    }
                });
            }.bind(this)();
        };
        return LinkList;
    }(SequentialContainer));

    var TreeNode = /** @class */ (function () {
        function TreeNode(key, value) {
            this.color = true;
            this.key = undefined;
            this.value = undefined;
            this.left = undefined;
            this.right = undefined;
            this.parent = undefined;
            this.key = key;
            this.value = value;
        }
        /**
         * @description Get the pre node.
         * @return TreeNode about the pre node.
         */
        TreeNode.prototype.pre = function () {
            var preNode = this;
            if (preNode.color === TreeNode.RED &&
                preNode.parent.parent === preNode) {
                preNode = preNode.right;
            }
            else if (preNode.left) {
                preNode = preNode.left;
                while (preNode.right) {
                    preNode = preNode.right;
                }
            }
            else {
                var pre = preNode.parent;
                while (pre.left === preNode) {
                    preNode = pre;
                    pre = preNode.parent;
                }
                preNode = pre;
            }
            return preNode;
        };
        /**
         * @description Get the next node.
         * @return TreeNode about the next node.
         */
        TreeNode.prototype.next = function () {
            var nextNode = this;
            if (nextNode.right) {
                nextNode = nextNode.right;
                while (nextNode.left) {
                    nextNode = nextNode.left;
                }
            }
            else {
                var pre = nextNode.parent;
                while (pre.right === nextNode) {
                    nextNode = pre;
                    pre = nextNode.parent;
                }
                if (nextNode.right !== pre) {
                    nextNode = pre;
                }
            }
            return nextNode;
        };
        /**
         * @description Rotate left.
         * @return TreeNode about moved to original position after rotation.
         */
        TreeNode.prototype.rotateLeft = function () {
            var PP = this.parent;
            var V = this.right;
            var R = V.left;
            if (PP.parent === this)
                PP.parent = V;
            else if (PP.left === this)
                PP.left = V;
            else
                PP.right = V;
            V.parent = PP;
            V.left = this;
            this.parent = V;
            this.right = R;
            if (R)
                R.parent = this;
            return V;
        };
        /**
         * @description Rotate left.
         * @return TreeNode about moved to original position after rotation.
         */
        TreeNode.prototype.rotateRight = function () {
            var PP = this.parent;
            var F = this.left;
            var K = F.right;
            if (PP.parent === this)
                PP.parent = F;
            else if (PP.left === this)
                PP.left = F;
            else
                PP.right = F;
            F.parent = PP;
            F.right = this;
            this.parent = F;
            this.left = K;
            if (K)
                K.parent = this;
            return F;
        };
        /**
         * @description Remove this.
         */
        TreeNode.prototype.remove = function () {
            var parent = this.parent;
            if (this === parent.left) {
                parent.left = undefined;
            }
            else
                parent.right = undefined;
        };
        TreeNode.RED = true;
        TreeNode.BLACK = false;
        return TreeNode;
    }());

    var TreeContainer = /** @class */ (function (_super) {
        __extends(TreeContainer, _super);
        function TreeContainer(cmp) {
            if (cmp === void 0) { cmp = function (x, y) {
                if (x < y)
                    return -1;
                if (x > y)
                    return 1;
                return 0;
            }; }
            var _this = _super.call(this) || this;
            _this.root = undefined;
            _this.header = new TreeNode();
            /**
             * @description InOrder traversal the tree.
             * @protected
             */
            _this.inOrderTraversal = function (curNode, callback) {
                if (curNode === undefined)
                    return false;
                var ifReturn = _this.inOrderTraversal(curNode.left, callback);
                if (ifReturn)
                    return true;
                if (callback(curNode))
                    return true;
                return _this.inOrderTraversal(curNode.right, callback);
            };
            _this.cmp = cmp;
            return _this;
        }
        /**
         * @param curNode The starting node of the search.
         * @param key The key you want to search.
         * @return TreeNode which key is greater than or equals to the given key.
         * @protected
         */
        TreeContainer.prototype._lowerBound = function (curNode, key) {
            var resNode;
            while (curNode) {
                var cmpResult = this.cmp(curNode.key, key);
                if (cmpResult < 0) {
                    curNode = curNode.right;
                }
                else if (cmpResult > 0) {
                    resNode = curNode;
                    curNode = curNode.left;
                }
                else
                    return curNode;
            }
            return resNode === undefined ? this.header : resNode;
        };
        /**
         * @param curNode The starting node of the search.
         * @param key The key you want to search.
         * @return TreeNode which key is greater than the given key.
         * @protected
         */
        TreeContainer.prototype._upperBound = function (curNode, key) {
            var resNode;
            while (curNode) {
                var cmpResult = this.cmp(curNode.key, key);
                if (cmpResult <= 0) {
                    curNode = curNode.right;
                }
                else if (cmpResult > 0) {
                    resNode = curNode;
                    curNode = curNode.left;
                }
            }
            return resNode === undefined ? this.header : resNode;
        };
        /**
         * @param curNode The starting node of the search.
         * @param key The key you want to search.
         * @return TreeNode which key is less than or equals to the given key.
         * @protected
         */
        TreeContainer.prototype._reverseLowerBound = function (curNode, key) {
            var resNode;
            while (curNode) {
                var cmpResult = this.cmp(curNode.key, key);
                if (cmpResult < 0) {
                    resNode = curNode;
                    curNode = curNode.right;
                }
                else if (cmpResult > 0) {
                    curNode = curNode.left;
                }
                else
                    return curNode;
            }
            return resNode === undefined ? this.header : resNode;
        };
        /**
         * @param curNode The starting node of the search.
         * @param key The key you want to search.
         * @return TreeNode which key is less than the given key.
         * @protected
         */
        TreeContainer.prototype._reverseUpperBound = function (curNode, key) {
            var resNode;
            while (curNode) {
                var cmpResult = this.cmp(curNode.key, key);
                if (cmpResult < 0) {
                    resNode = curNode;
                    curNode = curNode.right;
                }
                else if (cmpResult >= 0) {
                    curNode = curNode.left;
                }
            }
            return resNode === undefined ? this.header : resNode;
        };
        /**
         * @description Make self balance after erase a node.
         * @param curNode The node want to remove.
         * @protected
         */
        TreeContainer.prototype.eraseNodeSelfBalance = function (curNode) {
            while (true) {
                var parentNode = curNode.parent;
                if (parentNode === this.header)
                    return;
                if (curNode.color === TreeNode.RED) {
                    curNode.color = TreeNode.BLACK;
                    return;
                }
                if (curNode === parentNode.left) {
                    var brother = parentNode.right;
                    if (brother.color === TreeNode.RED) {
                        brother.color = TreeNode.BLACK;
                        parentNode.color = TreeNode.RED;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateLeft();
                        }
                        else
                            parentNode.rotateLeft();
                    }
                    else if (brother.color === TreeNode.BLACK) {
                        if (brother.right && brother.right.color === TreeNode.RED) {
                            brother.color = parentNode.color;
                            parentNode.color = TreeNode.BLACK;
                            brother.right.color = TreeNode.BLACK;
                            if (parentNode === this.root) {
                                this.root = parentNode.rotateLeft();
                            }
                            else
                                parentNode.rotateLeft();
                            return;
                        }
                        else if (brother.left && brother.left.color === TreeNode.RED) {
                            brother.color = TreeNode.RED;
                            brother.left.color = TreeNode.BLACK;
                            brother.rotateRight();
                        }
                        else {
                            brother.color = TreeNode.RED;
                            curNode = parentNode;
                        }
                    }
                }
                else {
                    var brother = parentNode.left;
                    if (brother.color === TreeNode.RED) {
                        brother.color = TreeNode.BLACK;
                        parentNode.color = TreeNode.RED;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateRight();
                        }
                        else
                            parentNode.rotateRight();
                    }
                    else {
                        if (brother.left && brother.left.color === TreeNode.RED) {
                            brother.color = parentNode.color;
                            parentNode.color = TreeNode.BLACK;
                            brother.left.color = TreeNode.BLACK;
                            if (parentNode === this.root) {
                                this.root = parentNode.rotateRight();
                            }
                            else
                                parentNode.rotateRight();
                            return;
                        }
                        else if (brother.right && brother.right.color === TreeNode.RED) {
                            brother.color = TreeNode.RED;
                            brother.right.color = TreeNode.BLACK;
                            brother.rotateLeft();
                        }
                        else {
                            brother.color = TreeNode.RED;
                            curNode = parentNode;
                        }
                    }
                }
            }
        };
        /**
         * @description Remove a node.
         * @param curNode The node you want to remove.
         * @protected
         */
        TreeContainer.prototype.eraseNode = function (curNode) {
            var _a, _b;
            if (this.length === 1) {
                this.clear();
                return;
            }
            var swapNode = curNode;
            while (swapNode.left || swapNode.right) {
                if (swapNode.right) {
                    swapNode = swapNode.right;
                    while (swapNode.left)
                        swapNode = swapNode.left;
                }
                else if (swapNode.left) {
                    swapNode = swapNode.left;
                }
                _a = __read([swapNode.key, curNode.key], 2), curNode.key = _a[0], swapNode.key = _a[1];
                _b = __read([swapNode.value, curNode.value], 2), curNode.value = _b[0], swapNode.value = _b[1];
                curNode = swapNode;
            }
            if (this.header.left === swapNode) {
                this.header.left = swapNode.parent;
            }
            else if (this.header.right === swapNode) {
                this.header.right = swapNode.parent;
            }
            this.eraseNodeSelfBalance(swapNode);
            swapNode.remove();
            this.length -= 1;
            this.root.color = TreeNode.BLACK;
        };
        /**
         * @description Make self balance after insert a node.
         * @param curNode The node want to insert.
         * @protected
         */
        TreeContainer.prototype.insertNodeSelfBalance = function (curNode) {
            while (true) {
                var parentNode = curNode.parent;
                if (parentNode.color === TreeNode.BLACK)
                    return;
                var grandParent = parentNode.parent;
                if (parentNode === grandParent.left) {
                    var uncle = grandParent.right;
                    if (uncle && uncle.color === TreeNode.RED) {
                        uncle.color = parentNode.color = TreeNode.BLACK;
                        if (grandParent === this.root)
                            return;
                        grandParent.color = TreeNode.RED;
                        curNode = grandParent;
                        continue;
                    }
                    else if (curNode === parentNode.right) {
                        curNode.color = TreeNode.BLACK;
                        if (curNode.left)
                            curNode.left.parent = parentNode;
                        if (curNode.right)
                            curNode.right.parent = grandParent;
                        parentNode.right = curNode.left;
                        grandParent.left = curNode.right;
                        curNode.left = parentNode;
                        curNode.right = grandParent;
                        if (grandParent === this.root) {
                            this.root = curNode;
                            this.header.parent = curNode;
                        }
                        else {
                            var GP = grandParent.parent;
                            if (GP.left === grandParent) {
                                GP.left = curNode;
                            }
                            else
                                GP.right = curNode;
                        }
                        curNode.parent = grandParent.parent;
                        parentNode.parent = curNode;
                        grandParent.parent = curNode;
                    }
                    else {
                        parentNode.color = TreeNode.BLACK;
                        if (grandParent === this.root) {
                            this.root = grandParent.rotateRight();
                        }
                        else
                            grandParent.rotateRight();
                    }
                    grandParent.color = TreeNode.RED;
                }
                else {
                    var uncle = grandParent.left;
                    if (uncle && uncle.color === TreeNode.RED) {
                        uncle.color = parentNode.color = TreeNode.BLACK;
                        if (grandParent === this.root)
                            return;
                        grandParent.color = TreeNode.RED;
                        curNode = grandParent;
                        continue;
                    }
                    else if (curNode === parentNode.left) {
                        curNode.color = TreeNode.BLACK;
                        if (curNode.left)
                            curNode.left.parent = grandParent;
                        if (curNode.right)
                            curNode.right.parent = parentNode;
                        grandParent.right = curNode.left;
                        parentNode.left = curNode.right;
                        curNode.left = grandParent;
                        curNode.right = parentNode;
                        if (grandParent === this.root) {
                            this.root = curNode;
                            this.header.parent = curNode;
                        }
                        else {
                            var GP = grandParent.parent;
                            if (GP.left === grandParent) {
                                GP.left = curNode;
                            }
                            else
                                GP.right = curNode;
                        }
                        curNode.parent = grandParent.parent;
                        parentNode.parent = curNode;
                        grandParent.parent = curNode;
                    }
                    else {
                        parentNode.color = TreeNode.BLACK;
                        if (grandParent === this.root) {
                            this.root = grandParent.rotateLeft();
                        }
                        else
                            grandParent.rotateLeft();
                    }
                    grandParent.color = TreeNode.RED;
                }
                return;
            }
        };
        /**
         * @description Find node which key is equals to the given key.
         * @param curNode The starting node of the search.
         * @param key The key you want to search.
         * @protected
         */
        TreeContainer.prototype.findElementNode = function (curNode, key) {
            while (curNode) {
                var cmpResult = this.cmp(curNode.key, key);
                if (cmpResult < 0) {
                    curNode = curNode.right;
                }
                else if (cmpResult > 0) {
                    curNode = curNode.left;
                }
                else
                    return curNode;
            }
            return curNode;
        };
        /**
         * @description Insert a key-value pair or set value by the given key.
         * @param key The key want to insert.
         * @param value The value want to set.
         * @param hint You can give an iterator hint to improve insertion efficiency.
         * @protected
         */
        TreeContainer.prototype.set = function (key, value, hint) {
            if (this.root === undefined) {
                this.length += 1;
                this.root = new TreeNode(key, value);
                this.root.color = TreeNode.BLACK;
                this.root.parent = this.header;
                this.header.parent = this.root;
                this.header.left = this.root;
                this.header.right = this.root;
                return;
            }
            var curNode;
            var minNode = this.header.left;
            var compareToMin = this.cmp(minNode.key, key);
            if (compareToMin === 0) {
                minNode.value = value;
                return;
            }
            else if (compareToMin > 0) {
                minNode.left = new TreeNode(key, value);
                minNode.left.parent = minNode;
                curNode = minNode.left;
                this.header.left = curNode;
            }
            else {
                var maxNode = this.header.right;
                var compareToMax = this.cmp(maxNode.key, key);
                if (compareToMax === 0) {
                    maxNode.value = value;
                    return;
                }
                else if (compareToMax < 0) {
                    maxNode.right = new TreeNode(key, value);
                    maxNode.right.parent = maxNode;
                    curNode = maxNode.right;
                    this.header.right = curNode;
                }
                else {
                    if (hint !== undefined) {
                        // @ts-ignore
                        var iterNode = hint.node;
                        if (iterNode !== this.header) {
                            var iterCmpRes = this.cmp(iterNode.key, key);
                            if (iterCmpRes === 0) {
                                iterNode.value = value;
                                return;
                            }
                            else if (iterCmpRes > 0) {
                                var preNode = iterNode.pre();
                                var preCmpRes = this.cmp(preNode.key, key);
                                if (preCmpRes === 0) {
                                    preNode.value = value;
                                    return;
                                }
                                else if (preCmpRes < 0) {
                                    curNode = new TreeNode(key, value);
                                    if (preNode.right === undefined) {
                                        preNode.right = curNode;
                                        curNode.parent = preNode;
                                    }
                                    else {
                                        iterNode.left = curNode;
                                        curNode.parent = iterNode;
                                    }
                                }
                            }
                        }
                    }
                    if (curNode === undefined) {
                        curNode = this.root;
                        while (true) {
                            var cmpResult = this.cmp(curNode.key, key);
                            if (cmpResult > 0) {
                                if (curNode.left === undefined) {
                                    curNode.left = new TreeNode(key, value);
                                    curNode.left.parent = curNode;
                                    curNode = curNode.left;
                                    break;
                                }
                                curNode = curNode.left;
                            }
                            else if (cmpResult < 0) {
                                if (curNode.right === undefined) {
                                    curNode.right = new TreeNode(key, value);
                                    curNode.right.parent = curNode;
                                    curNode = curNode.right;
                                    break;
                                }
                                curNode = curNode.right;
                            }
                            else {
                                curNode.value = value;
                                return;
                            }
                        }
                    }
                }
            }
            this.length += 1;
            this.insertNodeSelfBalance(curNode);
        };
        TreeContainer.prototype.clear = function () {
            this.length = 0;
            this.root = undefined;
            this.header.parent = undefined;
            this.header.left = this.header.right = undefined;
        };
        /**
         * @description Update node's key by iterator.
         * @param iter The iterator you want to change.
         * @param key The key you want to update.
         * @return Boolean about if the modification is successful.
         */
        TreeContainer.prototype.updateKeyByIterator = function (iter, key) {
            // @ts-ignore
            var node = iter.node;
            if (node === this.header) {
                throw new TypeError('Invalid iterator!');
            }
            if (this.length === 1) {
                node.key = key;
                return true;
            }
            if (node === this.header.left) {
                if (this.cmp(node.next().key, key) > 0) {
                    node.key = key;
                    return true;
                }
                return false;
            }
            if (node === this.header.right) {
                if (this.cmp(node.pre().key, key) < 0) {
                    node.key = key;
                    return true;
                }
                return false;
            }
            var preKey = node.pre().key;
            if (this.cmp(preKey, key) >= 0)
                return false;
            var nextKey = node.next().key;
            if (this.cmp(nextKey, key) <= 0)
                return false;
            node.key = key;
            return true;
        };
        TreeContainer.prototype.eraseElementByPos = function (pos) {
            var _this = this;
            checkWithinAccessParams(pos, 0, this.length - 1);
            var index = 0;
            this.inOrderTraversal(this.root, function (curNode) {
                if (pos === index) {
                    _this.eraseNode(curNode);
                    return true;
                }
                index += 1;
                return false;
            });
        };
        /**
         * @description Remove the element of the specified key.
         * @param key The key you want to remove.
         */
        TreeContainer.prototype.eraseElementByKey = function (key) {
            if (!this.length)
                return;
            var curNode = this.findElementNode(this.root, key);
            if (curNode === undefined)
                return;
            this.eraseNode(curNode);
        };
        TreeContainer.prototype.eraseElementByIterator = function (iter) {
            // @ts-ignore
            var node = iter.node;
            if (node === this.header) {
                throw new RangeError('Invalid iterator');
            }
            if (node.right === undefined) {
                iter = iter.next();
            }
            this.eraseNode(node);
            return iter;
        };
        /**
         * @description Get the height of the tree.
         * @return Number about the height of the RB-tree.
         */
        TreeContainer.prototype.getHeight = function () {
            if (!this.length)
                return 0;
            var traversal = function (curNode) {
                if (!curNode)
                    return 0;
                return Math.max(traversal(curNode.left), traversal(curNode.right)) + 1;
            };
            return traversal(this.root);
        };
        return TreeContainer;
    }(Container));

    var TreeIterator = /** @class */ (function (_super) {
        __extends(TreeIterator, _super);
        function TreeIterator(node, header, iteratorType) {
            var _this = _super.call(this, iteratorType) || this;
            _this.node = node;
            _this.header = header;
            if (_this.iteratorType === ContainerIterator.NORMAL) {
                _this.pre = function () {
                    if (this.node === this.header.left) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.pre();
                    return this;
                };
                _this.next = function () {
                    if (this.node === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.next();
                    return this;
                };
            }
            else {
                _this.pre = function () {
                    if (this.node === this.header.right) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.next();
                    return this;
                };
                _this.next = function () {
                    if (this.node === this.header) {
                        throw new RangeError('LinkList iterator access denied!');
                    }
                    this.node = this.node.pre();
                    return this;
                };
            }
            return _this;
        }
        TreeIterator.prototype.equals = function (obj) {
            return this.node === obj.node;
        };
        return TreeIterator;
    }(ContainerIterator));

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

    var HashContainer = /** @class */ (function (_super) {
        __extends(HashContainer, _super);
        function HashContainer(initBucketNum, hashFunc) {
            if (initBucketNum === void 0) { initBucketNum = 16; }
            if (hashFunc === void 0) { hashFunc = function (x) {
                var str;
                if (typeof x !== 'string') {
                    str = JSON.stringify(x);
                }
                else
                    str = x;
                var hashCode = 0;
                var strLength = str.length;
                for (var i = 0; i < strLength; i++) {
                    var ch = str.charCodeAt(i);
                    hashCode = ((hashCode << 5) - hashCode) + ch;
                    hashCode |= 0;
                }
                return hashCode >>> 0;
            }; }
            var _this = _super.call(this) || this;
            if (initBucketNum < 16 || (initBucketNum & (initBucketNum - 1)) !== 0) {
                throw new RangeError('InitBucketNum range error');
            }
            _this.bucketNum = _this.initBucketNum = initBucketNum;
            _this.hashFunc = hashFunc;
            return _this;
        }
        HashContainer.prototype.clear = function () {
            this.length = 0;
            this.bucketNum = this.initBucketNum;
            this.hashTable = [];
        };
        HashContainer.sigma = 0.75;
        HashContainer.treeifyThreshold = 8;
        HashContainer.untreeifyThreshold = 6;
        HashContainer.minTreeifySize = 64;
        HashContainer.maxBucketNum = (1 << 30);
        return HashContainer;
    }(Base));

    var HashSet = /** @class */ (function (_super) {
        __extends(HashSet, _super);
        function HashSet(container, initBucketNum, hashFunc) {
            if (container === void 0) { container = []; }
            var _this = _super.call(this, initBucketNum, hashFunc) || this;
            _this.hashTable = [];
            container.forEach(function (element) { return _this.insert(element); });
            return _this;
        }
        HashSet.prototype.reAllocate = function () {
            var _this = this;
            if (this.bucketNum >= HashContainer.maxBucketNum)
                return;
            var newHashTable = [];
            var originalBucketNum = this.bucketNum;
            this.bucketNum <<= 1;
            var keys = Object.keys(this.hashTable);
            var keyNums = keys.length;
            var _loop_1 = function (i) {
                var index = parseInt(keys[i]);
                var container = this_1.hashTable[index];
                var size = container.size();
                if (size === 0)
                    return "continue";
                if (size === 1) {
                    var element = container.front();
                    newHashTable[this_1.hashFunc(element) & (this_1.bucketNum - 1)] = new Vector([element], false);
                    return "continue";
                }
                var lowList = [];
                var highList = [];
                container.forEach(function (element) {
                    var hashCode = _this.hashFunc(element);
                    if ((hashCode & originalBucketNum) === 0) {
                        lowList.push(element);
                    }
                    else
                        highList.push(element);
                });
                if (container instanceof OrderedSet) {
                    if (lowList.length > HashContainer.untreeifyThreshold) {
                        newHashTable[index] = new OrderedSet(lowList);
                    }
                    else if (lowList.length) {
                        newHashTable[index] = new Vector(lowList, false);
                    }
                    if (highList.length > HashContainer.untreeifyThreshold) {
                        newHashTable[index + originalBucketNum] = new OrderedSet(highList);
                    }
                    else if (highList.length) {
                        newHashTable[index + originalBucketNum] = new Vector(highList, false);
                    }
                }
                else {
                    if (lowList.length >= HashContainer.treeifyThreshold) {
                        newHashTable[index] = new OrderedSet(lowList);
                    }
                    else if (lowList.length) {
                        newHashTable[index] = new Vector(lowList, false);
                    }
                    if (highList.length >= HashContainer.treeifyThreshold) {
                        newHashTable[index + originalBucketNum] = new OrderedSet(highList);
                    }
                    else if (highList.length) {
                        newHashTable[index + originalBucketNum] = new Vector(highList, false);
                    }
                }
            };
            var this_1 = this;
            for (var i = 0; i < keyNums; ++i) {
                _loop_1(i);
            }
            this.hashTable = newHashTable;
        };
        HashSet.prototype.forEach = function (callback) {
            var containers = Object.values(this.hashTable);
            var containersNum = containers.length;
            var index = 0;
            for (var i = 0; i < containersNum; ++i) {
                containers[i].forEach(function (element) { return callback(element, index++); });
            }
        };
        /**
         * @description Insert element to hash set.
         * @param element The element you want to insert.
         */
        HashSet.prototype.insert = function (element) {
            var index = this.hashFunc(element) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container) {
                this.hashTable[index] = new Vector([element], false);
                this.length += 1;
            }
            else {
                var preSize = container.size();
                if (container instanceof Vector) {
                    if (!container.find(element)
                        .equals(container.end()))
                        return;
                    container.pushBack(element);
                    if (preSize + 1 >= HashContainer.treeifyThreshold) {
                        if (this.bucketNum <= HashContainer.minTreeifySize) {
                            this.length += 1;
                            this.reAllocate();
                            return;
                        }
                        this.hashTable[index] = new OrderedSet(container);
                    }
                    this.length += 1;
                }
                else {
                    container.insert(element);
                    var curSize = container.size();
                    this.length += curSize - preSize;
                }
            }
            if (this.length > this.bucketNum * HashContainer.sigma) {
                this.reAllocate();
            }
        };
        HashSet.prototype.eraseElementByKey = function (key) {
            var index = this.hashFunc(key) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container)
                return;
            var preSize = container.size();
            if (preSize === 0)
                return;
            if (container instanceof Vector) {
                container.eraseElementByValue(key);
                var curSize = container.size();
                this.length += curSize - preSize;
            }
            else {
                container.eraseElementByKey(key);
                var curSize = container.size();
                this.length += curSize - preSize;
                if (curSize <= HashContainer.untreeifyThreshold) {
                    this.hashTable[index] = new Vector(container);
                }
            }
        };
        HashSet.prototype.find = function (element) {
            var index = this.hashFunc(element) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container)
                return false;
            return !container.find(element)
                .equals(container.end());
        };
        HashSet.prototype[Symbol.iterator] = function () {
            return function () {
                var containers, containersNum, i, container, container_1, container_1_1, element, e_1_1;
                var e_1, _a;
                return __generator(this, function (_b) {
                    switch (_b.label) {
                        case 0:
                            containers = Object.values(this.hashTable);
                            containersNum = containers.length;
                            i = 0;
                            _b.label = 1;
                        case 1:
                            if (!(i < containersNum)) return [3 /*break*/, 10];
                            container = containers[i];
                            _b.label = 2;
                        case 2:
                            _b.trys.push([2, 7, 8, 9]);
                            container_1 = (e_1 = void 0, __values(container)), container_1_1 = container_1.next();
                            _b.label = 3;
                        case 3:
                            if (!!container_1_1.done) return [3 /*break*/, 6];
                            element = container_1_1.value;
                            return [4 /*yield*/, element];
                        case 4:
                            _b.sent();
                            _b.label = 5;
                        case 5:
                            container_1_1 = container_1.next();
                            return [3 /*break*/, 3];
                        case 6: return [3 /*break*/, 9];
                        case 7:
                            e_1_1 = _b.sent();
                            e_1 = { error: e_1_1 };
                            return [3 /*break*/, 9];
                        case 8:
                            try {
                                if (container_1_1 && !container_1_1.done && (_a = container_1.return)) _a.call(container_1);
                            }
                            finally { if (e_1) throw e_1.error; }
                            return [7 /*endfinally*/];
                        case 9:
                            ++i;
                            return [3 /*break*/, 1];
                        case 10: return [2 /*return*/];
                    }
                });
            }.bind(this)();
        };
        return HashSet;
    }(HashContainer));

    var HashMap = /** @class */ (function (_super) {
        __extends(HashMap, _super);
        function HashMap(container, initBucketNum, hashFunc) {
            if (container === void 0) { container = []; }
            var _this = _super.call(this, initBucketNum, hashFunc) || this;
            _this.hashTable = [];
            container.forEach(function (element) { return _this.setElement(element[0], element[1]); });
            return _this;
        }
        HashMap.prototype.reAllocate = function () {
            var _this = this;
            if (this.bucketNum >= HashContainer.maxBucketNum)
                return;
            var newHashTable = [];
            var originalBucketNum = this.bucketNum;
            this.bucketNum <<= 1;
            var keys = Object.keys(this.hashTable);
            var keyNums = keys.length;
            var _loop_1 = function (i) {
                var index = parseInt(keys[i]);
                var container = this_1.hashTable[index];
                var size = container.size();
                if (size === 0)
                    return "continue";
                if (size === 1) {
                    var element = container.front();
                    newHashTable[this_1.hashFunc(element[0]) & (this_1.bucketNum - 1)] = new Vector([element], false);
                    return "continue";
                }
                var lowList = [];
                var highList = [];
                container.forEach(function (element) {
                    var hashCode = _this.hashFunc(element[0]);
                    if ((hashCode & originalBucketNum) === 0) {
                        lowList.push(element);
                    }
                    else
                        highList.push(element);
                });
                if (container instanceof OrderedMap) {
                    if (lowList.length > HashContainer.untreeifyThreshold) {
                        newHashTable[index] = new OrderedMap(lowList);
                    }
                    else if (lowList.length) {
                        newHashTable[index] = new Vector(lowList, false);
                    }
                    if (highList.length > HashContainer.untreeifyThreshold) {
                        newHashTable[index + originalBucketNum] = new OrderedMap(highList);
                    }
                    else if (highList.length) {
                        newHashTable[index + originalBucketNum] = new Vector(highList, false);
                    }
                }
                else {
                    if (lowList.length >= HashContainer.treeifyThreshold) {
                        newHashTable[index] = new OrderedMap(lowList);
                    }
                    else if (lowList.length) {
                        newHashTable[index] = new Vector(lowList, false);
                    }
                    if (highList.length >= HashContainer.treeifyThreshold) {
                        newHashTable[index + originalBucketNum] = new OrderedMap(highList);
                    }
                    else if (highList.length) {
                        newHashTable[index + originalBucketNum] = new Vector(highList, false);
                    }
                }
            };
            var this_1 = this;
            for (var i = 0; i < keyNums; ++i) {
                _loop_1(i);
            }
            this.hashTable = newHashTable;
        };
        HashMap.prototype.forEach = function (callback) {
            var containers = Object.values(this.hashTable);
            var containersNum = containers.length;
            var index = 0;
            for (var i = 0; i < containersNum; ++i) {
                containers[i].forEach(function (element) { return callback(element, index++); });
            }
        };
        /**
         * @description Insert a new key-value pair to hash map or set value by key.
         * @param key The key you want to insert.
         * @param value The value you want to insert.
         * @example HashMap.setElement(1, 2); // insert a key-value pair [1, 2]
         */
        HashMap.prototype.setElement = function (key, value) {
            var e_1, _a;
            var index = this.hashFunc(key) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container) {
                this.length += 1;
                this.hashTable[index] = new Vector([[key, value]], false);
            }
            else {
                var preSize = container.size();
                if (container instanceof Vector) {
                    try {
                        for (var container_1 = __values(container), container_1_1 = container_1.next(); !container_1_1.done; container_1_1 = container_1.next()) {
                            var pair = container_1_1.value;
                            if (pair[0] === key) {
                                pair[1] = value;
                                return;
                            }
                        }
                    }
                    catch (e_1_1) { e_1 = { error: e_1_1 }; }
                    finally {
                        try {
                            if (container_1_1 && !container_1_1.done && (_a = container_1.return)) _a.call(container_1);
                        }
                        finally { if (e_1) throw e_1.error; }
                    }
                    container.pushBack([key, value]);
                    if (preSize + 1 >= HashMap.treeifyThreshold) {
                        if (this.bucketNum <= HashMap.minTreeifySize) {
                            this.length += 1;
                            this.reAllocate();
                            return;
                        }
                        this.hashTable[index] = new OrderedMap(this.hashTable[index]);
                    }
                    this.length += 1;
                }
                else {
                    container.setElement(key, value);
                    var curSize = container.size();
                    this.length += curSize - preSize;
                }
            }
            if (this.length > this.bucketNum * HashMap.sigma) {
                this.reAllocate();
            }
        };
        /**
         * @description Get the value of the element which has the specified key.
         * @param key The key you want to get.
         */
        HashMap.prototype.getElementByKey = function (key) {
            var e_2, _a;
            var index = this.hashFunc(key) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container)
                return undefined;
            if (container instanceof OrderedMap) {
                return container.getElementByKey(key);
            }
            else {
                try {
                    for (var container_2 = __values(container), container_2_1 = container_2.next(); !container_2_1.done; container_2_1 = container_2.next()) {
                        var pair = container_2_1.value;
                        if (pair[0] === key)
                            return pair[1];
                    }
                }
                catch (e_2_1) { e_2 = { error: e_2_1 }; }
                finally {
                    try {
                        if (container_2_1 && !container_2_1.done && (_a = container_2.return)) _a.call(container_2);
                    }
                    finally { if (e_2) throw e_2.error; }
                }
                return undefined;
            }
        };
        HashMap.prototype.eraseElementByKey = function (key) {
            var e_3, _a;
            var index = this.hashFunc(key) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container)
                return;
            if (container instanceof Vector) {
                var pos = 0;
                try {
                    for (var container_3 = __values(container), container_3_1 = container_3.next(); !container_3_1.done; container_3_1 = container_3.next()) {
                        var pair = container_3_1.value;
                        if (pair[0] === key) {
                            container.eraseElementByPos(pos);
                            this.length -= 1;
                            return;
                        }
                        pos += 1;
                    }
                }
                catch (e_3_1) { e_3 = { error: e_3_1 }; }
                finally {
                    try {
                        if (container_3_1 && !container_3_1.done && (_a = container_3.return)) _a.call(container_3);
                    }
                    finally { if (e_3) throw e_3.error; }
                }
            }
            else {
                var preSize = container.size();
                container.eraseElementByKey(key);
                var curSize = container.size();
                this.length += curSize - preSize;
                if (curSize <= HashContainer.untreeifyThreshold) {
                    this.hashTable[index] = new Vector(container);
                }
            }
        };
        HashMap.prototype.find = function (key) {
            var e_4, _a;
            var index = this.hashFunc(key) & (this.bucketNum - 1);
            var container = this.hashTable[index];
            if (!container)
                return false;
            if (container instanceof OrderedMap) {
                return !container.find(key)
                    .equals(container.end());
            }
            try {
                for (var container_4 = __values(container), container_4_1 = container_4.next(); !container_4_1.done; container_4_1 = container_4.next()) {
                    var pair = container_4_1.value;
                    if (pair[0] === key)
                        return true;
                }
            }
            catch (e_4_1) { e_4 = { error: e_4_1 }; }
            finally {
                try {
                    if (container_4_1 && !container_4_1.done && (_a = container_4.return)) _a.call(container_4);
                }
                finally { if (e_4) throw e_4.error; }
            }
            return false;
        };
        HashMap.prototype[Symbol.iterator] = function () {
            return function () {
                var containers, containersNum, i, container, container_5, container_5_1, element, e_5_1;
                var e_5, _a;
                return __generator(this, function (_b) {
                    switch (_b.label) {
                        case 0:
                            containers = Object.values(this.hashTable);
                            containersNum = containers.length;
                            i = 0;
                            _b.label = 1;
                        case 1:
                            if (!(i < containersNum)) return [3 /*break*/, 10];
                            container = containers[i];
                            _b.label = 2;
                        case 2:
                            _b.trys.push([2, 7, 8, 9]);
                            container_5 = (e_5 = void 0, __values(container)), container_5_1 = container_5.next();
                            _b.label = 3;
                        case 3:
                            if (!!container_5_1.done) return [3 /*break*/, 6];
                            element = container_5_1.value;
                            return [4 /*yield*/, element];
                        case 4:
                            _b.sent();
                            _b.label = 5;
                        case 5:
                            container_5_1 = container_5.next();
                            return [3 /*break*/, 3];
                        case 6: return [3 /*break*/, 9];
                        case 7:
                            e_5_1 = _b.sent();
                            e_5 = { error: e_5_1 };
                            return [3 /*break*/, 9];
                        case 8:
                            try {
                                if (container_5_1 && !container_5_1.done && (_a = container_5.return)) _a.call(container_5);
                            }
                            finally { if (e_5) throw e_5.error; }
                            return [7 /*endfinally*/];
                        case 9:
                            ++i;
                            return [3 /*break*/, 1];
                        case 10: return [2 /*return*/];
                    }
                });
            }.bind(this)();
        };
        return HashMap;
    }(HashContainer));

    exports.Container = Container;
    exports.ContainerIterator = ContainerIterator;
    exports.Deque = Deque;
    exports.DequeIterator = DequeIterator;
    exports.HashContainer = HashContainer;
    exports.HashMap = HashMap;
    exports.HashSet = HashSet;
    exports.LinkList = LinkList;
    exports.LinkListIterator = LinkListIterator;
    exports.OrderedMap = OrderedMap;
    exports.OrderedMapIterator = OrderedMapIterator;
    exports.OrderedSet = OrderedSet;
    exports.OrderedSetIterator = OrderedSetIterator;
    exports.PriorityQueue = PriorityQueue;
    exports.Queue = Queue;
    exports.SequentialContainer = SequentialContainer;
    exports.Stack = Stack;
    exports.TreeContainer = TreeContainer;
    exports.Vector = Vector;
    exports.VectorIterator = VectorIterator;

    Object.defineProperty(exports, '__esModule', { value: true });

}));
