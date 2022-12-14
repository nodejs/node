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

class DequeIterator extends _RandomIterator.RandomIterator {
    copy() {
        return new DequeIterator(this.o, this.D, this.R, this.N, this.iteratorType);
    }
}

class Deque extends _Base.default {
    constructor(t = [], s = 1 << 12) {
        super();
        this.P = 0;
        this.A = 0;
        this.F = 0;
        this.j = 0;
        this.O = 0;
        this.T = [];
        let i;
        if ("size" in t) {
            if (typeof t.size === "number") {
                i = t.size;
            } else {
                i = t.size();
            }
        } else if ("length" in t) {
            i = t.length;
        } else {
            throw new RangeError("Can't get container's size!");
        }
        this.V = s;
        this.O = Math.max(Math.ceil(i / this.V), 1);
        for (let t = 0; t < this.O; ++t) {
            this.T.push(new Array(this.V));
        }
        const h = Math.ceil(i / this.V);
        this.P = this.F = (this.O >> 1) - (h >> 1);
        this.A = this.j = this.V - i % this.V >> 1;
        const e = this;
        t.forEach((function(t) {
            e.pushBack(t);
        }));
        this.size = this.size.bind(this);
        this.getElementByPos = this.getElementByPos.bind(this);
        this.setElementByPos = this.setElementByPos.bind(this);
    }
    G() {
        const t = [];
        const s = Math.max(this.O >> 1, 1);
        for (let i = 0; i < s; ++i) {
            t[i] = new Array(this.V);
        }
        for (let s = this.P; s < this.O; ++s) {
            t[t.length] = this.T[s];
        }
        for (let s = 0; s < this.F; ++s) {
            t[t.length] = this.T[s];
        }
        t[t.length] = [ ...this.T[this.F] ];
        this.P = s;
        this.F = t.length - 1;
        for (let i = 0; i < s; ++i) {
            t[t.length] = new Array(this.V);
        }
        this.T = t;
        this.O = t.length;
    }
    J(t) {
        const s = this.A + t + 1;
        const i = s % this.V;
        let h = i - 1;
        let e = this.P + (s - i) / this.V;
        if (i === 0) e -= 1;
        e %= this.O;
        if (h < 0) h += this.V;
        return {
            curNodeBucketIndex: e,
            curNodePointerIndex: h
        };
    }
    clear() {
        this.T = [ [] ];
        this.O = 1;
        this.P = this.F = this.i = 0;
        this.A = this.j = this.V >> 1;
    }
    begin() {
        return new DequeIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    }
    end() {
        return new DequeIterator(this.i, this.size, this.getElementByPos, this.setElementByPos);
    }
    rBegin() {
        return new DequeIterator(this.i - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    rEnd() {
        return new DequeIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    }
    front() {
        return this.T[this.P][this.A];
    }
    back() {
        return this.T[this.F][this.j];
    }
    pushBack(t) {
        if (this.i) {
            if (this.j < this.V - 1) {
                this.j += 1;
            } else if (this.F < this.O - 1) {
                this.F += 1;
                this.j = 0;
            } else {
                this.F = 0;
                this.j = 0;
            }
            if (this.F === this.P && this.j === this.A) this.G();
        }
        this.i += 1;
        this.T[this.F][this.j] = t;
        return this.i;
    }
    popBack() {
        if (this.i === 0) return;
        const t = this.T[this.F][this.j];
        delete this.T[this.F][this.j];
        if (this.i !== 1) {
            if (this.j > 0) {
                this.j -= 1;
            } else if (this.F > 0) {
                this.F -= 1;
                this.j = this.V - 1;
            } else {
                this.F = this.O - 1;
                this.j = this.V - 1;
            }
        }
        this.i -= 1;
        return t;
    }
    pushFront(t) {
        if (this.i) {
            if (this.A > 0) {
                this.A -= 1;
            } else if (this.P > 0) {
                this.P -= 1;
                this.A = this.V - 1;
            } else {
                this.P = this.O - 1;
                this.A = this.V - 1;
            }
            if (this.P === this.F && this.A === this.j) this.G();
        }
        this.i += 1;
        this.T[this.P][this.A] = t;
        return this.i;
    }
    popFront() {
        if (this.i === 0) return;
        const t = this.T[this.P][this.A];
        delete this.T[this.P][this.A];
        if (this.i !== 1) {
            if (this.A < this.V - 1) {
                this.A += 1;
            } else if (this.P < this.O - 1) {
                this.P += 1;
                this.A = 0;
            } else {
                this.P = 0;
                this.A = 0;
            }
        }
        this.i -= 1;
        return t;
    }
    getElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: s, curNodePointerIndex: i} = this.J(t);
        return this.T[s][i];
    }
    setElementByPos(t, s) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const {curNodeBucketIndex: i, curNodePointerIndex: h} = this.J(t);
        this.T[i][h] = s;
    }
    insert(t, s, i = 1) {
        if (t < 0 || t > this.i) {
            throw new RangeError;
        }
        if (t === 0) {
            while (i--) this.pushFront(s);
        } else if (t === this.i) {
            while (i--) this.pushBack(s);
        } else {
            const h = [];
            for (let s = t; s < this.i; ++s) {
                h.push(this.getElementByPos(s));
            }
            this.cut(t - 1);
            for (let t = 0; t < i; ++t) this.pushBack(s);
            for (let t = 0; t < h.length; ++t) this.pushBack(h[t]);
        }
        return this.i;
    }
    cut(t) {
        if (t < 0) {
            this.clear();
            return 0;
        }
        const {curNodeBucketIndex: s, curNodePointerIndex: i} = this.J(t);
        this.F = s;
        this.j = i;
        this.i = t + 1;
        return this.i;
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.i - 1) this.popBack(); else {
            const s = [];
            for (let i = t + 1; i < this.i; ++i) {
                s.push(this.getElementByPos(i));
            }
            this.cut(t);
            this.popBack();
            const i = this;
            s.forEach((function(t) {
                i.pushBack(t);
            }));
        }
        return this.i;
    }
    eraseElementByValue(t) {
        if (this.i === 0) return 0;
        const s = [];
        for (let i = 0; i < this.i; ++i) {
            const h = this.getElementByPos(i);
            if (h !== t) s.push(h);
        }
        const i = s.length;
        for (let t = 0; t < i; ++t) this.setElementByPos(t, s[t]);
        return this.cut(i - 1);
    }
    eraseElementByIterator(t) {
        const s = t.o;
        this.eraseElementByPos(s);
        t = t.next();
        return t;
    }
    find(t) {
        for (let s = 0; s < this.i; ++s) {
            if (this.getElementByPos(s) === t) {
                return new DequeIterator(s, this.size, this.getElementByPos, this.setElementByPos);
            }
        }
        return this.end();
    }
    reverse() {
        let t = 0;
        let s = this.i - 1;
        while (t < s) {
            const i = this.getElementByPos(t);
            this.setElementByPos(t, this.getElementByPos(s));
            this.setElementByPos(s, i);
            t += 1;
            s -= 1;
        }
    }
    unique() {
        if (this.i <= 1) {
            return this.i;
        }
        let t = 1;
        let s = this.getElementByPos(0);
        for (let i = 1; i < this.i; ++i) {
            const h = this.getElementByPos(i);
            if (h !== s) {
                s = h;
                this.setElementByPos(t++, h);
            }
        }
        while (this.i > t) this.popBack();
        return this.i;
    }
    sort(t) {
        const s = [];
        for (let t = 0; t < this.i; ++t) {
            s.push(this.getElementByPos(t));
        }
        s.sort(t);
        for (let t = 0; t < this.i; ++t) this.setElementByPos(t, s[t]);
    }
    shrinkToFit() {
        if (this.i === 0) return;
        const t = [];
        this.forEach((function(s) {
            t.push(s);
        }));
        this.O = Math.max(Math.ceil(this.i / this.V), 1);
        this.i = this.P = this.F = this.A = this.j = 0;
        this.T = [];
        for (let t = 0; t < this.O; ++t) {
            this.T.push(new Array(this.V));
        }
        for (let s = 0; s < t.length; ++s) this.pushBack(t[s]);
    }
    forEach(t) {
        for (let s = 0; s < this.i; ++s) {
            t(this.getElementByPos(s), s, this);
        }
    }
    [Symbol.iterator]() {
        return function*() {
            for (let t = 0; t < this.i; ++t) {
                yield this.getElementByPos(t);
            }
        }.bind(this)();
    }
}

var _default = Deque;

exports.default = _default;
//# sourceMappingURL=Deque.js.map
