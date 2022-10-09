"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = exports.DequeIterator = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _RandomIterator = require("./Base/RandomIterator");

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class DequeIterator extends _RandomIterator.RandomIterator {
    copy() {
        return new DequeIterator(this.I, this.D, this.g, this.m, this.iteratorType);
    }
}

exports.DequeIterator = DequeIterator;

class Deque extends _Base.default {
    constructor(t = [], i = 1 << 12) {
        super();
        this.R = 0;
        this.k = 0;
        this.N = 0;
        this.M = 0;
        this.u = 0;
        this.P = [];
        let s;
        if ("size" in t) {
            if (typeof t.size === "number") {
                s = t.size;
            } else {
                s = t.size();
            }
        } else if ("length" in t) {
            s = t.length;
        } else {
            throw new RangeError("Can't get container's size!");
        }
        this.A = i;
        this.u = Math.max(Math.ceil(s / this.A), 1);
        for (let t = 0; t < this.u; ++t) {
            this.P.push(new Array(this.A));
        }
        const h = Math.ceil(s / this.A);
        this.R = this.N = (this.u >> 1) - (h >> 1);
        this.k = this.M = this.A - s % this.A >> 1;
        t.forEach((t => this.pushBack(t)));
        this.size = this.size.bind(this);
        this.getElementByPos = this.getElementByPos.bind(this);
        this.setElementByPos = this.setElementByPos.bind(this);
    }
    h() {
        const t = [];
        const i = Math.max(this.u >> 1, 1);
        for (let s = 0; s < i; ++s) {
            t[s] = new Array(this.A);
        }
        for (let i = this.R; i < this.u; ++i) {
            t[t.length] = this.P[i];
        }
        for (let i = 0; i < this.N; ++i) {
            t[t.length] = this.P[i];
        }
        t[t.length] = [ ...this.P[this.N] ];
        this.R = i;
        this.N = t.length - 1;
        for (let s = 0; s < i; ++s) {
            t[t.length] = new Array(this.A);
        }
        this.P = t;
        this.u = t.length;
    }
    F(t) {
        const i = this.k + t + 1;
        const s = i % this.A;
        let h = s - 1;
        let e = this.R + (i - s) / this.A;
        if (s === 0) e -= 1;
        e %= this.u;
        if (h < 0) h += this.A;
        return {
            curNodeBucketIndex: e,
            curNodePointerIndex: h
        };
    }
    clear() {
        this.P = [ [] ];
        this.u = 1;
        this.R = this.N = this.o = 0;
        this.k = this.M = this.A >> 1;
    }
    front() {
        return this.P[this.R][this.k];
    }
    back() {
        return this.P[this.N][this.M];
    }
    begin() {
        return new DequeIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    }
    end() {
        return new DequeIterator(this.o, this.size, this.getElementByPos, this.setElementByPos);
    }
    rBegin() {
        return new DequeIterator(this.o - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    rEnd() {
        return new DequeIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    pushBack(t) {
        if (this.o) {
            if (this.M < this.A - 1) {
                this.M += 1;
            } else if (this.N < this.u - 1) {
                this.N += 1;
                this.M = 0;
            } else {
                this.N = 0;
                this.M = 0;
            }
            if (this.N === this.R && this.M === this.k) this.h();
        }
        this.o += 1;
        this.P[this.N][this.M] = t;
    }
    popBack() {
        if (!this.o) return;
        this.P[this.N][this.M] = undefined;
        if (this.o !== 1) {
            if (this.M > 0) {
                this.M -= 1;
            } else if (this.N > 0) {
                this.N -= 1;
                this.M = this.A - 1;
            } else {
                this.N = this.u - 1;
                this.M = this.A - 1;
            }
        }
        this.o -= 1;
    }
    pushFront(t) {
        if (this.o) {
            if (this.k > 0) {
                this.k -= 1;
            } else if (this.R > 0) {
                this.R -= 1;
                this.k = this.A - 1;
            } else {
                this.R = this.u - 1;
                this.k = this.A - 1;
            }
            if (this.R === this.N && this.k === this.M) this.h();
        }
        this.o += 1;
        this.P[this.R][this.k] = t;
    }
    popFront() {
        if (!this.o) return;
        this.P[this.R][this.k] = undefined;
        if (this.o !== 1) {
            if (this.k < this.A - 1) {
                this.k += 1;
            } else if (this.R < this.u - 1) {
                this.R += 1;
                this.k = 0;
            } else {
                this.R = 0;
                this.k = 0;
            }
        }
        this.o -= 1;
    }
    forEach(t) {
        for (let i = 0; i < this.o; ++i) {
            t(this.getElementByPos(i), i);
        }
    }
    getElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: i, curNodePointerIndex: s} = this.F(t);
        return this.P[i][s];
    }
    setElementByPos(t, i) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: s, curNodePointerIndex: h} = this.F(t);
        this.P[s][h] = i;
    }
    insert(t, i, s = 1) {
        if (t < 0 || t > this.o) {
            throw new RangeError;
        }
        if (t === 0) {
            while (s--) this.pushFront(i);
        } else if (t === this.o) {
            while (s--) this.pushBack(i);
        } else {
            const h = [];
            for (let i = t; i < this.o; ++i) {
                h.push(this.getElementByPos(i));
            }
            this.cut(t - 1);
            for (let t = 0; t < s; ++t) this.pushBack(i);
            for (let t = 0; t < h.length; ++t) this.pushBack(h[t]);
        }
    }
    cut(t) {
        if (t < 0) {
            this.clear();
            return;
        }
        const {curNodeBucketIndex: i, curNodePointerIndex: s} = this.F(t);
        this.N = i;
        this.M = s;
        this.o = t + 1;
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.o - 1) this.popBack(); else {
            const i = [];
            for (let s = t + 1; s < this.o; ++s) {
                i.push(this.getElementByPos(s));
            }
            this.cut(t);
            this.popBack();
            i.forEach((t => this.pushBack(t)));
        }
    }
    eraseElementByValue(t) {
        if (!this.o) return;
        const i = [];
        for (let s = 0; s < this.o; ++s) {
            const h = this.getElementByPos(s);
            if (h !== t) i.push(h);
        }
        const s = i.length;
        for (let t = 0; t < s; ++t) this.setElementByPos(t, i[t]);
        this.cut(s - 1);
    }
    eraseElementByIterator(t) {
        const i = t.I;
        this.eraseElementByPos(i);
        t = t.next();
        return t;
    }
    find(t) {
        for (let i = 0; i < this.o; ++i) {
            if (this.getElementByPos(i) === t) {
                return new DequeIterator(i, this.size, this.getElementByPos, this.setElementByPos);
            }
        }
        return this.end();
    }
    reverse() {
        let t = 0;
        let i = this.o - 1;
        while (t < i) {
            const s = this.getElementByPos(t);
            this.setElementByPos(t, this.getElementByPos(i));
            this.setElementByPos(i, s);
            t += 1;
            i -= 1;
        }
    }
    unique() {
        if (this.o <= 1) return;
        let t = 1;
        let i = this.getElementByPos(0);
        for (let s = 1; s < this.o; ++s) {
            const h = this.getElementByPos(s);
            if (h !== i) {
                i = h;
                this.setElementByPos(t++, h);
            }
        }
        while (this.o > t) this.popBack();
    }
    sort(t) {
        const i = [];
        for (let t = 0; t < this.o; ++t) {
            i.push(this.getElementByPos(t));
        }
        i.sort(t);
        for (let t = 0; t < this.o; ++t) this.setElementByPos(t, i[t]);
    }
    shrinkToFit() {
        if (!this.o) return;
        const t = [];
        this.forEach((i => t.push(i)));
        this.u = Math.max(Math.ceil(this.o / this.A), 1);
        this.o = this.R = this.N = this.k = this.M = 0;
        this.P = [];
        for (let t = 0; t < this.u; ++t) {
            this.P.push(new Array(this.A));
        }
        for (let i = 0; i < t.length; ++i) this.pushBack(t[i]);
    }
    [Symbol.iterator]() {
        return function*() {
            for (let t = 0; t < this.o; ++t) {
                yield this.getElementByPos(t);
            }
        }.bind(this)();
    }
}

var _default = Deque;

exports.default = _default;