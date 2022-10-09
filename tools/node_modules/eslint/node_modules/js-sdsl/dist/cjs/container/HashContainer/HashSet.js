"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _Vector = _interopRequireDefault(require("../SequentialContainer/Vector"));

var _OrderedSet = _interopRequireDefault(require("../TreeContainer/OrderedSet"));

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class HashSet extends _Base.default {
    constructor(e = [], t, s) {
        super(t, s);
        this.i = [];
        e.forEach((e => this.insert(e)));
    }
    h() {
        if (this.u >= 1073741824) return;
        const e = [];
        const t = this.u;
        this.u <<= 1;
        const s = Object.keys(this.i);
        const i = s.length;
        for (let r = 0; r < i; ++r) {
            const i = parseInt(s[r]);
            const n = this.i[i];
            const o = n.size();
            if (o === 0) continue;
            if (o === 1) {
                const t = n.front();
                e[this.l(t) & this.u - 1] = new _Vector.default([ t ], false);
                continue;
            }
            const c = [];
            const f = [];
            n.forEach((e => {
                const s = this.l(e);
                if ((s & t) === 0) {
                    c.push(e);
                } else f.push(e);
            }));
            if (n instanceof _OrderedSet.default) {
                if (c.length > 6) {
                    e[i] = new _OrderedSet.default(c);
                } else {
                    e[i] = new _Vector.default(c, false);
                }
                if (f.length > 6) {
                    e[i + t] = new _OrderedSet.default(f);
                } else {
                    e[i + t] = new _Vector.default(f, false);
                }
            } else {
                e[i] = new _Vector.default(c, false);
                e[i + t] = new _Vector.default(f, false);
            }
        }
        this.i = e;
    }
    forEach(e) {
        const t = Object.values(this.i);
        const s = t.length;
        let i = 0;
        for (let r = 0; r < s; ++r) {
            t[r].forEach((t => e(t, i++)));
        }
    }
    insert(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) {
            this.i[t] = new _Vector.default([ e ], false);
            this.o += 1;
        } else {
            const i = s.size();
            if (s instanceof _Vector.default) {
                if (!s.find(e).equals(s.end())) return;
                s.pushBack(e);
                if (i + 1 >= 8) {
                    if (this.u <= 64) {
                        this.o += 1;
                        this.h();
                        return;
                    }
                    this.i[t] = new _OrderedSet.default(s);
                }
                this.o += 1;
            } else {
                s.insert(e);
                const t = s.size();
                this.o += t - i;
            }
        }
        if (this.o > this.u * .75) {
            this.h();
        }
    }
    eraseElementByKey(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) return;
        const i = s.size();
        if (i === 0) return;
        if (s instanceof _Vector.default) {
            s.eraseElementByValue(e);
            const t = s.size();
            this.o += t - i;
        } else {
            s.eraseElementByKey(e);
            const r = s.size();
            this.o += r - i;
            if (r <= 6) {
                this.i[t] = new _Vector.default(s);
            }
        }
    }
    find(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) return false;
        return !s.find(e).equals(s.end());
    }
    [Symbol.iterator]() {
        return function*() {
            const e = Object.values(this.i);
            const t = e.length;
            for (let s = 0; s < t; ++s) {
                const t = e[s];
                for (const e of t) {
                    yield e;
                }
            }
        }.bind(this)();
    }
}

var _default = HashSet;

exports.default = _default;