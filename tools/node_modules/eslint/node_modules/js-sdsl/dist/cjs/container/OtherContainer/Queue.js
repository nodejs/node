"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../ContainerBase");

var _Deque = _interopRequireDefault(require("../SequentialContainer/Deque"));

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class Queue extends _ContainerBase.Base {
    constructor(e = []) {
        super();
        this.q = new _Deque.default(e);
        this.i = this.q.size();
    }
    clear() {
        this.q.clear();
        this.i = 0;
    }
    push(e) {
        this.q.pushBack(e);
        this.i += 1;
        return this.i;
    }
    pop() {
        if (this.i === 0) return;
        this.i -= 1;
        return this.q.popFront();
    }
    front() {
        return this.q.front();
    }
}

var _default = Queue;

exports.default = _default;
//# sourceMappingURL=Queue.js.map
