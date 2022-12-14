"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _RandomIterator = require("./Base/RandomIterator");

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class VectorIterator extends _RandomIterator.RandomIterator {
    copy() {
        return new VectorIterator(this.o, this.D, this.R, this.N, this.iteratorType);
    }
}

class Vector extends _Base.default {
    constructor(t = [], r = true) {
        super();
        if (Array.isArray(t)) {
            this.W = r ? [ ...t ] : t;
            this.i = t.length;
        } else {
            this.W = [];
            const r = this;
            t.forEach((function(t) {
                r.pushBack(t);
            }));
        }
        this.size = this.size.bind(this);
        this.getElementByPos = this.getElementByPos.bind(this);
        this.setElementByPos = this.setElementByPos.bind(this);
    }
    clear() {
        this.i = 0;
        this.W.length = 0;
    }
    begin() {
        return new VectorIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    }
    end() {
        return new VectorIterator(this.i, this.size, this.getElementByPos, this.setElementByPos);
    }
    rBegin() {
        return new VectorIterator(this.i - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    rEnd() {
        return new VectorIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    front() {
        return this.W[0];
    }
    back() {
        return this.W[this.i - 1];
    }
    getElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        return this.W[t];
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        this.W.splice(t, 1);
        this.i -= 1;
        return this.i;
    }
    eraseElementByValue(t) {
        let r = 0;
        for (let e = 0; e < this.i; ++e) {
            if (this.W[e] !== t) {
                this.W[r++] = this.W[e];
            }
        }
        this.i = this.W.length = r;
        return this.i;
    }
    eraseElementByIterator(t) {
        const r = t.o;
        t = t.next();
        this.eraseElementByPos(r);
        return t;
    }
    pushBack(t) {
        this.W.push(t);
        this.i += 1;
        return this.i;
    }
    popBack() {
        if (this.i === 0) return;
        this.i -= 1;
        return this.W.pop();
    }
    setElementByPos(t, r) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        this.W[t] = r;
    }
    insert(t, r, e = 1) {
        if (t < 0 || t > this.i) {
            throw new RangeError;
        }
        this.W.splice(t, 0, ...new Array(e).fill(r));
        this.i += e;
        return this.i;
    }
    find(t) {
        for (let r = 0; r < this.i; ++r) {
            if (this.W[r] === t) {
                return new VectorIterator(r, this.size, this.getElementByPos, this.getElementByPos);
            }
        }
        return this.end();
    }
    reverse() {
        this.W.reverse();
    }
    unique() {
        let t = 1;
        for (let r = 1; r < this.i; ++r) {
            if (this.W[r] !== this.W[r - 1]) {
                this.W[t++] = this.W[r];
            }
        }
        this.i = this.W.length = t;
        return this.i;
    }
    sort(t) {
        this.W.sort(t);
    }
    forEach(t) {
        for (let r = 0; r < this.i; ++r) {
            t(this.W[r], r, this);
        }
    }
    [Symbol.iterator]() {
        return function*() {
            yield* this.W;
        }.bind(this)();
    }
}

var _default = Vector;

exports.default = _default;
//# sourceMappingURL=Vector.js.map
