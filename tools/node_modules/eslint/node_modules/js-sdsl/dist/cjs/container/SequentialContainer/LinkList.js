"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.LinkListIterator = exports.LinkNode = void 0;
const index_1 = __importDefault(require("./Base/index"));
const checkParams_1 = require("../../utils/checkParams");
const index_2 = require("../ContainerBase/index");
class LinkNode {
    constructor(element) {
        this.value = undefined;
        this.pre = undefined;
        this.next = undefined;
        this.value = element;
    }
}
exports.LinkNode = LinkNode;
class LinkListIterator extends index_2.ContainerIterator {
    constructor(node, header, iteratorType) {
        super(iteratorType);
        this.node = node;
        this.header = header;
        if (this.iteratorType === index_2.ContainerIterator.NORMAL) {
            this.pre = function () {
                if (this.node.pre === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre;
                return this;
            };
            this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next;
                return this;
            };
        }
        else {
            this.pre = function () {
                if (this.node.next === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next;
                return this;
            };
            this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre;
                return this;
            };
        }
    }
    get pointer() {
        if (this.node === this.header) {
            throw new RangeError('LinkList iterator access denied!');
        }
        return this.node.value;
    }
    set pointer(newValue) {
        if (this.node === this.header) {
            throw new RangeError('LinkList iterator access denied!');
        }
        this.node.value = newValue;
    }
    equals(obj) {
        return this.node === obj.node;
    }
    copy() {
        return new LinkListIterator(this.node, this.header, this.iteratorType);
    }
}
exports.LinkListIterator = LinkListIterator;
class LinkList extends index_1.default {
    constructor(container = []) {
        super();
        this.header = new LinkNode();
        this.head = undefined;
        this.tail = undefined;
        container.forEach(element => this.pushBack(element));
    }
    clear() {
        this.length = 0;
        this.head = this.tail = undefined;
        this.header.pre = this.header.next = undefined;
    }
    begin() {
        return new LinkListIterator(this.head || this.header, this.header);
    }
    end() {
        return new LinkListIterator(this.header, this.header);
    }
    rBegin() {
        return new LinkListIterator(this.tail || this.header, this.header, index_2.ContainerIterator.REVERSE);
    }
    rEnd() {
        return new LinkListIterator(this.header, this.header, index_2.ContainerIterator.REVERSE);
    }
    front() {
        return this.head ? this.head.value : undefined;
    }
    back() {
        return this.tail ? this.tail.value : undefined;
    }
    forEach(callback) {
        if (!this.length)
            return;
        let curNode = this.head;
        let index = 0;
        while (curNode !== this.header) {
            callback(curNode.value, index++);
            curNode = curNode.next;
        }
    }
    getElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        let curNode = this.head;
        while (pos--) {
            curNode = curNode.next;
        }
        return curNode.value;
    }
    eraseElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        if (pos === 0)
            this.popFront();
        else if (pos === this.length - 1)
            this.popBack();
        else {
            let curNode = this.head;
            while (pos--) {
                curNode = curNode.next;
            }
            curNode = curNode;
            const pre = curNode.pre;
            const next = curNode.next;
            next.pre = pre;
            pre.next = next;
            this.length -= 1;
        }
    }
    eraseElementByValue(value) {
        while (this.head && this.head.value === value)
            this.popFront();
        while (this.tail && this.tail.value === value)
            this.popBack();
        if (!this.head)
            return;
        let curNode = this.head;
        while (curNode !== this.header) {
            if (curNode.value === value) {
                const pre = curNode.pre;
                const next = curNode.next;
                if (next)
                    next.pre = pre;
                if (pre)
                    pre.next = next;
                this.length -= 1;
            }
            curNode = curNode.next;
        }
    }
    eraseElementByIterator(iter) {
        // @ts-ignore
        const node = iter.node;
        if (node === this.header) {
            throw new RangeError('Invalid iterator');
        }
        iter = iter.next();
        if (this.head === node)
            this.popFront();
        else if (this.tail === node)
            this.popBack();
        else {
            const pre = node.pre;
            const next = node.next;
            if (next)
                next.pre = pre;
            if (pre)
                pre.next = next;
            this.length -= 1;
        }
        return iter;
    }
    pushBack(element) {
        this.length += 1;
        const newTail = new LinkNode(element);
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
    }
    popBack() {
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
    }
    setElementByPos(pos, element) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        let curNode = this.head;
        while (pos--) {
            curNode = curNode.next;
        }
        curNode.value = element;
    }
    insert(pos, element, num = 1) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length);
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
            let curNode = this.head;
            for (let i = 1; i < pos; ++i) {
                curNode = curNode.next;
            }
            const next = curNode.next;
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
    }
    find(element) {
        if (!this.head)
            return this.end();
        let curNode = this.head;
        while (curNode !== this.header) {
            if (curNode.value === element) {
                return new LinkListIterator(curNode, this.header);
            }
            curNode = curNode.next;
        }
        return this.end();
    }
    reverse() {
        if (this.length <= 1)
            return;
        let pHead = this.head;
        let pTail = this.tail;
        let cnt = 0;
        while ((cnt << 1) < this.length) {
            const tmp = pHead.value;
            pHead.value = pTail.value;
            pTail.value = tmp;
            pHead = pHead.next;
            pTail = pTail.pre;
            cnt += 1;
        }
    }
    unique() {
        if (this.length <= 1)
            return;
        let curNode = this.head;
        while (curNode !== this.header) {
            let tmpNode = curNode;
            while (tmpNode.next && tmpNode.value === tmpNode.next.value) {
                tmpNode = tmpNode.next;
                this.length -= 1;
            }
            curNode.next = tmpNode.next;
            if (curNode.next)
                curNode.next.pre = curNode;
            curNode = curNode.next;
        }
    }
    sort(cmp) {
        if (this.length <= 1)
            return;
        const arr = [];
        this.forEach(element => arr.push(element));
        arr.sort(cmp);
        let curNode = this.head;
        arr.forEach((element) => {
            curNode.value = element;
            curNode = curNode.next;
        });
    }
    /**
     * @description Push an element to the front.
     * @param element The element you want to push.
     */
    pushFront(element) {
        this.length += 1;
        const newHead = new LinkNode(element);
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
    }
    /**
     * @description Removes the first element.
     */
    popFront() {
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
    }
    /**
     * @description Merges two sorted lists.
     * @param list The other list you want to merge (must be sorted).
     */
    merge(list) {
        if (!this.head) {
            list.forEach(element => this.pushBack(element));
            return;
        }
        let curNode = this.head;
        list.forEach(element => {
            while (curNode &&
                curNode !== this.header &&
                curNode.value <= element) {
                curNode = curNode.next;
            }
            if (curNode === this.header) {
                this.pushBack(element);
                curNode = this.tail;
            }
            else if (curNode === this.head) {
                this.pushFront(element);
                curNode = this.head;
            }
            else {
                this.length += 1;
                const pre = curNode.pre;
                pre.next = new LinkNode(element);
                pre.next.pre = pre;
                pre.next.next = curNode;
                curNode.pre = pre.next;
            }
        });
    }
    [Symbol.iterator]() {
        return function* () {
            if (!this.head)
                return;
            let curNode = this.head;
            while (curNode !== this.header) {
                yield curNode.value;
                curNode = curNode.next;
            }
        }.bind(this)();
    }
}
exports.default = LinkList;
