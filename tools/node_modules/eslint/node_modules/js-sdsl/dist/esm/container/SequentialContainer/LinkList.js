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
import SequentialContainer from './Base/index';
import { checkWithinAccessParams } from "../../utils/checkParams";
import { ContainerIterator } from "../ContainerBase/index";
var LinkNode = /** @class */ (function () {
    function LinkNode(element) {
        this.value = undefined;
        this.pre = undefined;
        this.next = undefined;
        this.value = element;
    }
    return LinkNode;
}());
export { LinkNode };
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
export { LinkListIterator };
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
export default LinkList;
