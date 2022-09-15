"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const index_1 = require("../ContainerBase/index");
class Stack extends index_1.Base {
    constructor(container = []) {
        super();
        this.stack = [];
        container.forEach(element => this.push(element));
    }
    clear() {
        this.length = 0;
        this.stack.length = 0;
    }
    /**
     * @description Insert element to stack's end.
     */
    push(element) {
        this.stack.push(element);
        this.length += 1;
    }
    /**
     * @description Removes the end element.
     */
    pop() {
        this.stack.pop();
        if (this.length > 0)
            this.length -= 1;
    }
    /**
     * @description Accesses the end element.
     */
    top() {
        return this.stack[this.length - 1];
    }
}
exports.default = Stack;
