"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const index_1 = require("../../ContainerBase/index");
class TreeIterator extends index_1.ContainerIterator {
    constructor(node, header, iteratorType) {
        super(iteratorType);
        this.node = node;
        this.header = header;
        if (this.iteratorType === index_1.ContainerIterator.NORMAL) {
            this.pre = function () {
                if (this.node === this.header.left) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre();
                return this;
            };
            this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next();
                return this;
            };
        }
        else {
            this.pre = function () {
                if (this.node === this.header.right) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next();
                return this;
            };
            this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre();
                return this;
            };
        }
    }
    equals(obj) {
        return this.node === obj.node;
    }
}
exports.default = TreeIterator;
