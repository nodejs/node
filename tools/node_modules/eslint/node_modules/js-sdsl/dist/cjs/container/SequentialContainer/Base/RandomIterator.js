"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RandomIterator = void 0;
const checkParams_1 = require("../../../utils/checkParams");
const index_1 = require("../../ContainerBase/index");
class RandomIterator extends index_1.ContainerIterator {
    constructor(index, size, getElementByPos, setElementByPos, iteratorType) {
        super(iteratorType);
        this.node = index;
        this.size = size;
        this.getElementByPos = getElementByPos;
        this.setElementByPos = setElementByPos;
        if (this.iteratorType === index_1.ContainerIterator.NORMAL) {
            this.pre = function () {
                if (this.node === 0) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node -= 1;
                return this;
            };
            this.next = function () {
                if (this.node === this.size()) {
                    throw new RangeError('Deque Iterator access denied!');
                }
                this.node += 1;
                return this;
            };
        }
        else {
            this.pre = function () {
                if (this.node === this.size() - 1) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node += 1;
                return this;
            };
            this.next = function () {
                if (this.node === -1) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node -= 1;
                return this;
            };
        }
    }
    get pointer() {
        (0, checkParams_1.checkWithinAccessParams)(this.node, 0, this.size() - 1);
        return this.getElementByPos(this.node);
    }
    set pointer(newValue) {
        (0, checkParams_1.checkWithinAccessParams)(this.node, 0, this.size() - 1);
        this.setElementByPos(this.node, newValue);
    }
    equals(obj) {
        return this.node === obj.node;
    }
}
exports.RandomIterator = RandomIterator;
