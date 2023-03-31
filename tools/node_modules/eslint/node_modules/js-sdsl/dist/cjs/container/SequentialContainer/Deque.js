"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _RandomIterator = require("./Base/RandomIterator");

var Math = _interopRequireWildcard(require("../../utils/math"));

function _getRequireWildcardCache(t) {
    if (typeof WeakMap !== "function") return null;
    var i = new WeakMap;
    var s = new WeakMap;
    return (_getRequireWildcardCache = function(t) {
        return t ? s : i;
    })(t);
}

function _interopRequireWildcard(t, i) {
    if (!i && t && t.t) {
        return t;
    }
    if (t === null || typeof t !== "object" && typeof t !== "function") {
        return {
            default: t
        };
    }
    var s = _getRequireWildcardCache(i);
    if (s && s.has(t)) {
        return s.get(t);
    }
    var e = {};
    var h = Object.defineProperty && Object.getOwnPropertyDescriptor;
    for (var r in t) {
        if (r !== "default" && Object.prototype.hasOwnProperty.call(t, r)) {
            var n = h ? Object.getOwnPropertyDescriptor(t, r) : null;
            if (n && (n.get || n.set)) {
                Object.defineProperty(e, r, n);
            } else {
                e[r] = t[r];
            }
        }
    }
    e.default = t;
    if (s) {
        s.set(t, e);
    }
    return e;
}

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class DequeIterator extends _RandomIterator.RandomIterator {
    constructor(t, i, s) {
        super(t, s);
        this.container = i;
    }
    copy() {
        return new DequeIterator(this.o, this.container, this.iteratorType);
    }
}

class Deque extends _Base.default {
    constructor(t = [], i = 1 << 12) {
        super();
        this.j = 0;
        this.R = 0;
        this.N = 0;
        this.D = 0;
        this.P = 0;
        this.W = [];
        const s = (() => {
            if (typeof t.length === "number") return t.length;
            if (typeof t.size === "number") return t.size;
            if (typeof t.size === "function") return t.size();
            throw new TypeError("Cannot get the length or size of the container");
        })();
        this.O = i;
        this.P = Math.ceil(s, this.O) || 1;
        for (let t = 0; t < this.P; ++t) {
            this.W.push(new Array(this.O));
        }
        const e = Math.ceil(s, this.O);
        this.j = this.N = (this.P >> 1) - (e >> 1);
        this.R = this.D = this.O - s % this.O >> 1;
        const h = this;
        t.forEach((function(t) {
            h.pushBack(t);
        }));
    }
    A(t) {
        const i = [];
        const s = t || this.P >> 1 || 1;
        for (let t = 0; t < s; ++t) {
            i[t] = new Array(this.O);
        }
        for (let t = this.j; t < this.P; ++t) {
            i[i.length] = this.W[t];
        }
        for (let t = 0; t < this.N; ++t) {
            i[i.length] = this.W[t];
        }
        i[i.length] = [ ...this.W[this.N] ];
        this.j = s;
        this.N = i.length - 1;
        for (let t = 0; t < s; ++t) {
            i[i.length] = new Array(this.O);
        }
        this.W = i;
        this.P = i.length;
    }
    F(t) {
        let i, s;
        const e = this.R + t;
        i = this.j + Math.floor(e / this.O);
        if (i >= this.P) {
            i -= this.P;
        }
        s = (e + 1) % this.O - 1;
        if (s < 0) {
            s = this.O - 1;
        }
        return {
            curNodeBucketIndex: i,
            curNodePointerIndex: s
        };
    }
    clear() {
        this.W = [ new Array(this.O) ];
        this.P = 1;
        this.j = this.N = this.i = 0;
        this.R = this.D = this.O >> 1;
    }
    begin() {
        return new DequeIterator(0, this);
    }
    end() {
        return new DequeIterator(this.i, this);
    }
    rBegin() {
        return new DequeIterator(this.i - 1, this, 1);
    }
    rEnd() {
        return new DequeIterator(-1, this, 1);
    }
    front() {
        if (this.i === 0) return;
        return this.W[this.j][this.R];
    }
    back() {
        if (this.i === 0) return;
        return this.W[this.N][this.D];
    }
    pushBack(t) {
        if (this.i) {
            if (this.D < this.O - 1) {
                this.D += 1;
            } else if (this.N < this.P - 1) {
                this.N += 1;
                this.D = 0;
            } else {
                this.N = 0;
                this.D = 0;
            }
            if (this.N === this.j && this.D === this.R) this.A();
        }
        this.i += 1;
        this.W[this.N][this.D] = t;
        return this.i;
    }
    popBack() {
        if (this.i === 0) return;
        const t = this.W[this.N][this.D];
        if (this.i !== 1) {
            if (this.D > 0) {
                this.D -= 1;
            } else if (this.N > 0) {
                this.N -= 1;
                this.D = this.O - 1;
            } else {
                this.N = this.P - 1;
                this.D = this.O - 1;
            }
        }
        this.i -= 1;
        return t;
    }
    pushFront(t) {
        if (this.i) {
            if (this.R > 0) {
                this.R -= 1;
            } else if (this.j > 0) {
                this.j -= 1;
                this.R = this.O - 1;
            } else {
                this.j = this.P - 1;
                this.R = this.O - 1;
            }
            if (this.j === this.N && this.R === this.D) this.A();
        }
        this.i += 1;
        this.W[this.j][this.R] = t;
        return this.i;
    }
    popFront() {
        if (this.i === 0) return;
        const t = this.W[this.j][this.R];
        if (this.i !== 1) {
            if (this.R < this.O - 1) {
                this.R += 1;
            } else if (this.j < this.P - 1) {
                this.j += 1;
                this.R = 0;
            } else {
                this.j = 0;
                this.R = 0;
            }
        }
        this.i -= 1;
        return t;
    }
    getElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: i, curNodePointerIndex: s} = this.F(t);
        return this.W[i][s];
    }
    setElementByPos(t, i) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: s, curNodePointerIndex: e} = this.F(t);
        this.W[s][e] = i;
    }
    insert(t, i, s = 1) {
        const e = this.i;
        if (t < 0 || t > e) {
            throw new RangeError;
        }
        if (t === 0) {
            while (s--) this.pushFront(i);
        } else if (t === this.i) {
            while (s--) this.pushBack(i);
        } else {
            const e = [];
            for (let i = t; i < this.i; ++i) {
                e.push(this.getElementByPos(i));
            }
            this.cut(t - 1);
            for (let t = 0; t < s; ++t) this.pushBack(i);
            for (let t = 0; t < e.length; ++t) this.pushBack(e[t]);
        }
        return this.i;
    }
    cut(t) {
        if (t < 0) {
            this.clear();
            return 0;
        }
        const {curNodeBucketIndex: i, curNodePointerIndex: s} = this.F(t);
        this.N = i;
        this.D = s;
        this.i = t + 1;
        return this.i;
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.i - 1) this.popBack(); else {
            const i = this.i - 1;
            let {curNodeBucketIndex: s, curNodePointerIndex: e} = this.F(t);
            for (let h = t; h < i; ++h) {
                const {curNodeBucketIndex: i, curNodePointerIndex: h} = this.F(t + 1);
                this.W[s][e] = this.W[i][h];
                s = i;
                e = h;
            }
            this.popBack();
        }
        return this.i;
    }
    eraseElementByValue(t) {
        const i = this.i;
        if (i === 0) return 0;
        let s = 0;
        let e = 0;
        while (s < i) {
            const i = this.getElementByPos(s);
            if (i !== t) {
                this.setElementByPos(e, i);
                e += 1;
            }
            s += 1;
        }
        this.cut(e - 1);
        return this.i;
    }
    eraseElementByIterator(t) {
        const i = t.o;
        this.eraseElementByPos(i);
        t = t.next();
        return t;
    }
    find(t) {
        for (let i = 0; i < this.i; ++i) {
            if (this.getElementByPos(i) === t) {
                return new DequeIterator(i, this);
            }
        }
        return this.end();
    }
    reverse() {
        this.W.reverse().forEach((function(t) {
            t.reverse();
        }));
        const {j: t, N: i, R: s, D: e} = this;
        this.j = this.P - i - 1;
        this.N = this.P - t - 1;
        this.R = this.O - e - 1;
        this.D = this.O - s - 1;
        return this;
    }
    unique() {
        if (this.i <= 1) {
            return this.i;
        }
        let t = 1;
        let i = this.getElementByPos(0);
        for (let s = 1; s < this.i; ++s) {
            const e = this.getElementByPos(s);
            if (e !== i) {
                i = e;
                this.setElementByPos(t++, e);
            }
        }
        this.cut(t - 1);
        return this.i;
    }
    sort(t) {
        const i = [];
        for (let t = 0; t < this.i; ++t) {
            i.push(this.getElementByPos(t));
        }
        i.sort(t);
        for (let t = 0; t < this.i; ++t) {
            this.setElementByPos(t, i[t]);
        }
        return this;
    }
    shrinkToFit() {
        if (this.i === 0) return;
        const t = [];
        if (this.j === this.N) return; else if (this.j < this.N) {
            for (let i = this.j; i <= this.N; ++i) {
                t.push(this.W[i]);
            }
        } else {
            for (let i = this.j; i < this.P; ++i) {
                t.push(this.W[i]);
            }
            for (let i = 0; i <= this.N; ++i) {
                t.push(this.W[i]);
            }
        }
        this.j = 0;
        this.N = t.length - 1;
        this.W = t;
    }
    forEach(t) {
        for (let i = 0; i < this.i; ++i) {
            t(this.getElementByPos(i), i, this);
        }
    }
    * [Symbol.iterator]() {
        for (let t = 0; t < this.i; ++t) {
            yield this.getElementByPos(t);
        }
    }
}

var _default = Deque;

exports.default = _default;
//# sourceMappingURL=Deque.js.map
