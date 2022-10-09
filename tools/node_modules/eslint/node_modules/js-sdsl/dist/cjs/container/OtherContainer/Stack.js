"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../ContainerBase");

class Stack extends _ContainerBase.Base {
    constructor(t = []) {
        super();
        this.C = [];
        t.forEach((t => this.push(t)));
    }
    clear() {
        this.o = 0;
        this.C.length = 0;
    }
    push(t) {
        this.C.push(t);
        this.o += 1;
    }
    pop() {
        this.C.pop();
        if (this.o > 0) this.o -= 1;
    }
    top() {
        return this.C[this.o - 1];
    }
}

var _default = Stack;

exports.default = _default;