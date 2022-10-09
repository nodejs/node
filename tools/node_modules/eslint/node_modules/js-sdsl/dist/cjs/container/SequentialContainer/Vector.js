"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = exports.VectorIterator = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _RandomIterator = require("./Base/RandomIterator");

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class VectorIterator extends _RandomIterator.RandomIterator {
    copy() {
        return new VectorIterator(this.I, this.D, this.g, this.m, this.iteratorType);
    }
}

exports.VectorIterator = VectorIterator;

class Vector extends _Base.default {
    constructor(t = [], e = true) {
        super();
        if (Array.isArray(t)) {
            this.H = e ? [ ...t ] : t;
            this.o = t.length;
        } else {
            this.H = [];
            t.forEach((t => this.pushBack(t)));
        }
        this.size = this.size.bind(this);
        this.getElementByPos = this.getElementByPos.bind(this);
        this.setElementByPos = this.setElementByPos.bind(this);
    }
    clear() {
        this.o = 0;
        this.H.length = 0;
    }
    begin() {
        return new VectorIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    }
    end() {
        return new VectorIterator(this.o, this.size, this.getElementByPos, this.setElementByPos);
    }
    rBegin() {
        return new VectorIterator(this.o - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    rEnd() {
        return new VectorIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    front() {
        return this.H[0];
    }
    back() {
        return this.H[this.o - 1];
    }
    forEach(t) {
        for (let e = 0; e < this.o; ++e) {
            t(this.H[e], e);
        }
    }
    getElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        return this.H[t];
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        this.H.splice(t, 1);
        this.o -= 1;
    }
    eraseElementByValue(t) {
        let e = 0;
        for (let r = 0; r < this.o; ++r) {
            if (this.H[r] !== t) {
                this.H[e++] = this.H[r];
            }
        }
        this.o = this.H.length = e;
    }
    eraseElementByIterator(t) {
        const e = t.I;
        t = t.next();
        this.eraseElementByPos(e);
        return t;
    }
    pushBack(t) {
        this.H.push(t);
        this.o += 1;
    }
    popBack() {
        if (!this.o) return;
        this.H.pop();
        this.o -= 1;
    }
    setElementByPos(t, e) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        this.H[t] = e;
    }
    insert(t, e, r = 1) {
        if (t < 0 || t > this.o) {
            throw new RangeError;
        }
        this.H.splice(t, 0, ...new Array(r).fill(e));
        this.o += r;
    }
    find(t) {
        for (let e = 0; e < this.o; ++e) {
            if (this.H[e] === t) {
                return new VectorIterator(e, this.size, this.getElementByPos, this.getElementByPos);
            }
        }
        return this.end();
    }
    reverse() {
        this.H.reverse();
    }
    unique() {
        let t = 1;
        for (let e = 1; e < this.o; ++e) {
            if (this.H[e] !== this.H[e - 1]) {
                this.H[t++] = this.H[e];
            }
        }
        this.o = this.H.length = t;
    }
    sort(t) {
        this.H.sort(t);
    }
    [Symbol.iterator]() {
        return function*() {
            return yield* this.H;
        }.bind(this)();
    }
}

var _default = Vector;

exports.default = _default;