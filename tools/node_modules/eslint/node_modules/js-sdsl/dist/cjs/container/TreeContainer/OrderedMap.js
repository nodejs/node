"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.OrderedMapIterator = void 0;
const index_1 = require("../ContainerBase/index");
const checkParams_1 = require("../../utils/checkParams");
const index_2 = __importDefault(require("./Base/index"));
const TreeIterator_1 = __importDefault(require("./Base/TreeIterator"));
class OrderedMapIterator extends TreeIterator_1.default {
    get pointer() {
        if (this.node === this.header) {
            throw new RangeError('OrderedMap iterator access denied');
        }
        return new Proxy([], {
            get: (_, props) => {
                if (props === '0')
                    return this.node.key;
                else if (props === '1')
                    return this.node.value;
            },
            set: (_, props, newValue) => {
                if (props !== '1') {
                    throw new TypeError('props must be 1');
                }
                this.node.value = newValue;
                return true;
            }
        });
    }
    copy() {
        return new OrderedMapIterator(this.node, this.header, this.iteratorType);
    }
}
exports.OrderedMapIterator = OrderedMapIterator;
class OrderedMap extends index_2.default {
    constructor(container = [], cmp) {
        super(cmp);
        this.iterationFunc = function* (curNode) {
            if (curNode === undefined)
                return;
            yield* this.iterationFunc(curNode.left);
            yield [curNode.key, curNode.value];
            yield* this.iterationFunc(curNode.right);
        };
        this.iterationFunc = this.iterationFunc.bind(this);
        container.forEach(([key, value]) => this.setElement(key, value));
    }
    begin() {
        return new OrderedMapIterator(this.header.left || this.header, this.header);
    }
    end() {
        return new OrderedMapIterator(this.header, this.header);
    }
    rBegin() {
        return new OrderedMapIterator(this.header.right || this.header, this.header, index_1.ContainerIterator.REVERSE);
    }
    rEnd() {
        return new OrderedMapIterator(this.header, this.header, index_1.ContainerIterator.REVERSE);
    }
    front() {
        if (!this.length)
            return undefined;
        const minNode = this.header.left;
        return [minNode.key, minNode.value];
    }
    back() {
        if (!this.length)
            return undefined;
        const maxNode = this.header.right;
        return [maxNode.key, maxNode.value];
    }
    forEach(callback) {
        let index = 0;
        for (const pair of this)
            callback(pair, index++);
    }
    lowerBound(key) {
        const resNode = this._lowerBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    }
    upperBound(key) {
        const resNode = this._upperBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    }
    reverseLowerBound(key) {
        const resNode = this._reverseLowerBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    }
    reverseUpperBound(key) {
        const resNode = this._reverseUpperBound(this.root, key);
        return new OrderedMapIterator(resNode, this.header);
    }
    /**
     * @description Insert a key-value pair or set value by the given key.
     * @param key The key want to insert.
     * @param value The value want to set.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     */
    setElement(key, value, hint) {
        this.set(key, value, hint);
    }
    find(key) {
        const curNode = this.findElementNode(this.root, key);
        if (curNode !== undefined) {
            return new OrderedMapIterator(curNode, this.header);
        }
        return this.end();
    }
    /**
     * @description Get the value of the element of the specified key.
     */
    getElementByKey(key) {
        const curNode = this.findElementNode(this.root, key);
        return curNode ? curNode.value : undefined;
    }
    getElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        let res;
        let index = 0;
        for (const pair of this) {
            if (index === pos) {
                res = pair;
                break;
            }
            index += 1;
        }
        return res;
    }
    union(other) {
        other.forEach(([key, value]) => this.setElement(key, value));
    }
    [Symbol.iterator]() {
        return this.iterationFunc(this.root);
    }
}
exports.default = OrderedMap;
