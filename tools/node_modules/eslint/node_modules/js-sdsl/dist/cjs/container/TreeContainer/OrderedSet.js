"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.OrderedSetIterator = void 0;
const index_1 = __importDefault(require("./Base/index"));
const index_2 = require("../ContainerBase/index");
const checkParams_1 = require("../../utils/checkParams");
const TreeIterator_1 = __importDefault(require("./Base/TreeIterator"));
class OrderedSetIterator extends TreeIterator_1.default {
    get pointer() {
        if (this.node === this.header) {
            throw new RangeError('OrderedSet iterator access denied!');
        }
        return this.node.key;
    }
    copy() {
        return new OrderedSetIterator(this.node, this.header, this.iteratorType);
    }
}
exports.OrderedSetIterator = OrderedSetIterator;
class OrderedSet extends index_1.default {
    constructor(container = [], cmp) {
        super(cmp);
        this.iterationFunc = function* (curNode) {
            if (curNode === undefined)
                return;
            yield* this.iterationFunc(curNode.left);
            yield curNode.key;
            yield* this.iterationFunc(curNode.right);
        };
        container.forEach((element) => this.insert(element));
        this.iterationFunc = this.iterationFunc.bind(this);
    }
    begin() {
        return new OrderedSetIterator(this.header.left || this.header, this.header);
    }
    end() {
        return new OrderedSetIterator(this.header, this.header);
    }
    rBegin() {
        return new OrderedSetIterator(this.header.right || this.header, this.header, index_2.ContainerIterator.REVERSE);
    }
    rEnd() {
        return new OrderedSetIterator(this.header, this.header, index_2.ContainerIterator.REVERSE);
    }
    front() {
        return this.header.left ? this.header.left.key : undefined;
    }
    back() {
        return this.header.right ? this.header.right.key : undefined;
    }
    forEach(callback) {
        let index = 0;
        for (const element of this)
            callback(element, index++);
    }
    getElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        let res;
        let index = 0;
        for (const element of this) {
            if (index === pos) {
                res = element;
            }
            index += 1;
        }
        return res;
    }
    /**
     * @description Insert element to set.
     * @param key The key want to insert.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     */
    insert(key, hint) {
        this.set(key, undefined, hint);
    }
    find(element) {
        const curNode = this.findElementNode(this.root, element);
        if (curNode !== undefined) {
            return new OrderedSetIterator(curNode, this.header);
        }
        return this.end();
    }
    lowerBound(key) {
        const resNode = this._lowerBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    }
    upperBound(key) {
        const resNode = this._upperBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    }
    reverseLowerBound(key) {
        const resNode = this._reverseLowerBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    }
    reverseUpperBound(key) {
        const resNode = this._reverseUpperBound(this.root, key);
        return new OrderedSetIterator(resNode, this.header);
    }
    union(other) {
        other.forEach((element) => this.insert(element));
    }
    [Symbol.iterator]() {
        return this.iterationFunc(this.root);
    }
}
exports.default = OrderedSet;
