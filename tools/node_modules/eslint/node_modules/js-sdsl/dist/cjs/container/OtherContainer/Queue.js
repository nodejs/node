"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Deque = _interopRequireDefault(require("../SequentialContainer/Deque"));

var _ContainerBase = require("../ContainerBase");

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class Queue extends _ContainerBase.Base {
    constructor(e = []) {
        super();
        this.q = new _Deque.default(e);
        this.o = this.q.size();
    }
    clear() {
        this.q.clear();
        this.o = 0;
    }
    push(e) {
        this.q.pushBack(e);
        this.o += 1;
    }
    pop() {
        this.q.popFront();
        if (this.o) this.o -= 1;
    }
    front() {
        return this.q.front();
    }
}

var _default = Queue;

exports.default = _default;