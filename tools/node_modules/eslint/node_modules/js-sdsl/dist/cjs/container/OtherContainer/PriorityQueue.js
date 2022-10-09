"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../ContainerBase");

class PriorityQueue extends _ContainerBase.Base {
    constructor(t = [], s = ((t, s) => {
        if (t > s) return -1;
        if (t < s) return 1;
        return 0;
    }), i = true) {
        super();
        this.p = s;
        if (Array.isArray(t)) {
            this._ = i ? [ ...t ] : t;
        } else {
            this._ = [];
            t.forEach((t => this._.push(t)));
        }
        this.o = this._.length;
        const e = this.o >> 1;
        for (let t = this.o - 1 >> 1; t >= 0; --t) {
            this.v(t, e);
        }
    }
    B(t) {
        const s = this._[t];
        while (t > 0) {
            const i = t - 1 >> 1;
            const e = this._[i];
            if (this.p(e, s) <= 0) break;
            this._[t] = e;
            t = i;
        }
        this._[t] = s;
    }
    v(t, s) {
        const i = this._[t];
        while (t < s) {
            let s = t << 1 | 1;
            const e = s + 1;
            let h = this._[s];
            if (e < this.o && this.p(h, this._[e]) > 0) {
                s = e;
                h = this._[e];
            }
            if (this.p(h, i) >= 0) break;
            this._[t] = h;
            t = s;
        }
        this._[t] = i;
    }
    clear() {
        this.o = 0;
        this._.length = 0;
    }
    push(t) {
        this._.push(t);
        this.B(this.o);
        this.o += 1;
    }
    pop() {
        if (!this.o) return;
        const t = this._.pop();
        this.o -= 1;
        if (this.o) {
            this._[0] = t;
            this.v(0, this.o >> 1);
        }
    }
    top() {
        return this._[0];
    }
    find(t) {
        return this._.indexOf(t) >= 0;
    }
    remove(t) {
        const s = this._.indexOf(t);
        if (s < 0) return false;
        if (s === 0) {
            this.pop();
        } else if (s === this.o - 1) {
            this._.pop();
            this.o -= 1;
        } else {
            this._.splice(s, 1, this._.pop());
            this.o -= 1;
            this.B(s);
            this.v(s, this.o >> 1);
        }
        return true;
    }
    updateItem(t) {
        const s = this._.indexOf(t);
        if (s < 0) return false;
        this.B(s);
        this.v(s, this.o >> 1);
        return true;
    }
    toArray() {
        return [ ...this._ ];
    }
}

var _default = PriorityQueue;

exports.default = _default;