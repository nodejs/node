"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.VectorIterator = void 0;
const index_1 = __importDefault(require("./Base/index"));
const checkParams_1 = require("../../utils/checkParams");
const index_2 = require("../ContainerBase/index");
const RandomIterator_1 = require("./Base/RandomIterator");
class VectorIterator extends RandomIterator_1.RandomIterator {
    copy() {
        return new VectorIterator(this.node, this.size, this.getElementByPos, this.setElementByPos, this.iteratorType);
    }
}
exports.VectorIterator = VectorIterator;
class Vector extends index_1.default {
    /**
     * @description Vector's constructor.
     * @param container Initialize container, must have a forEach function.
     * @param copy When the container is an array, you can choose to directly operate on the original object of
     *             the array or perform a shallow copy. The default is shallow copy.
     */
    constructor(container = [], copy = true) {
        super();
        if (Array.isArray(container)) {
            this.vector = copy ? [...container] : container;
            this.length = container.length;
        }
        else {
            this.vector = [];
            container.forEach(element => this.pushBack(element));
        }
        this.size = this.size.bind(this);
        this.getElementByPos = this.getElementByPos.bind(this);
        this.setElementByPos = this.setElementByPos.bind(this);
    }
    clear() {
        this.length = 0;
        this.vector.length = 0;
    }
    begin() {
        return new VectorIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    }
    end() {
        return new VectorIterator(this.length, this.size, this.getElementByPos, this.setElementByPos);
    }
    rBegin() {
        return new VectorIterator(this.length - 1, this.size, this.getElementByPos, this.setElementByPos, index_2.ContainerIterator.REVERSE);
    }
    rEnd() {
        return new VectorIterator(-1, this.size, this.getElementByPos, this.setElementByPos, index_2.ContainerIterator.REVERSE);
    }
    front() {
        return this.vector[0];
    }
    back() {
        return this.vector[this.length - 1];
    }
    forEach(callback) {
        for (let i = 0; i < this.length; ++i) {
            callback(this.vector[i], i);
        }
    }
    getElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        return this.vector[pos];
    }
    eraseElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        this.vector.splice(pos, 1);
        this.length -= 1;
    }
    eraseElementByValue(value) {
        let index = 0;
        for (let i = 0; i < this.length; ++i) {
            if (this.vector[i] !== value) {
                this.vector[index++] = this.vector[i];
            }
        }
        this.length = this.vector.length = index;
    }
    eraseElementByIterator(iter) {
        // @ts-ignore
        const node = iter.node;
        iter = iter.next();
        this.eraseElementByPos(node);
        return iter;
    }
    pushBack(element) {
        this.vector.push(element);
        this.length += 1;
    }
    popBack() {
        if (!this.length)
            return;
        this.vector.pop();
        this.length -= 1;
    }
    setElementByPos(pos, element) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        this.vector[pos] = element;
    }
    insert(pos, element, num = 1) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length);
        this.vector.splice(pos, 0, ...new Array(num).fill(element));
        this.length += num;
    }
    find(element) {
        for (let i = 0; i < this.length; ++i) {
            if (this.vector[i] === element) {
                return new VectorIterator(i, this.size, this.getElementByPos, this.getElementByPos);
            }
        }
        return this.end();
    }
    reverse() {
        this.vector.reverse();
    }
    unique() {
        let index = 1;
        for (let i = 1; i < this.length; ++i) {
            if (this.vector[i] !== this.vector[i - 1]) {
                this.vector[index++] = this.vector[i];
            }
        }
        this.length = this.vector.length = index;
    }
    sort(cmp) {
        this.vector.sort(cmp);
    }
    [Symbol.iterator]() {
        return function* () {
            return yield* this.vector;
        }.bind(this)();
    }
}
exports.default = Vector;
