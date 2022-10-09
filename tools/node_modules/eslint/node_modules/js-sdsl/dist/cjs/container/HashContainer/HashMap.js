"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _Vector = _interopRequireDefault(require("../SequentialContainer/Vector"));

var _OrderedMap = _interopRequireDefault(require("../TreeContainer/OrderedMap"));

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class HashMap extends _Base.default {
    constructor(e = [], t, s) {
        super(t, s);
        this.i = [];
        e.forEach((e => this.setElement(e[0], e[1])));
    }
    h() {
        if (this.u >= 1073741824) return;
        const e = [];
        const t = this.u;
        this.u <<= 1;
        const s = Object.keys(this.i);
        const r = s.length;
        for (let n = 0; n < r; ++n) {
            const r = parseInt(s[n]);
            const i = this.i[r];
            const o = i.size();
            if (o === 0) continue;
            if (o === 1) {
                const t = i.front();
                e[this.l(t[0]) & this.u - 1] = new _Vector.default([ t ], false);
                continue;
            }
            const c = [];
            const f = [];
            i.forEach((e => {
                const s = this.l(e[0]);
                if ((s & t) === 0) {
                    c.push(e);
                } else f.push(e);
            }));
            if (i instanceof _OrderedMap.default) {
                if (c.length > 6) {
                    e[r] = new _OrderedMap.default(c);
                } else {
                    e[r] = new _Vector.default(c, false);
                }
                if (f.length > 6) {
                    e[r + t] = new _OrderedMap.default(f);
                } else {
                    e[r + t] = new _Vector.default(f, false);
                }
            } else {
                e[r] = new _Vector.default(c, false);
                e[r + t] = new _Vector.default(f, false);
            }
        }
        this.i = e;
    }
    forEach(e) {
        const t = Object.values(this.i);
        const s = t.length;
        let r = 0;
        for (let n = 0; n < s; ++n) {
            t[n].forEach((t => e(t, r++)));
        }
    }
    setElement(e, t) {
        const s = this.l(e) & this.u - 1;
        const r = this.i[s];
        if (!r) {
            this.o += 1;
            this.i[s] = new _Vector.default([ [ e, t ] ], false);
        } else {
            const n = r.size();
            if (r instanceof _Vector.default) {
                for (const s of r) {
                    if (s[0] === e) {
                        s[1] = t;
                        return;
                    }
                }
                r.pushBack([ e, t ]);
                if (n + 1 >= 8) {
                    if (this.u <= 64) {
                        this.o += 1;
                        this.h();
                        return;
                    }
                    this.i[s] = new _OrderedMap.default(this.i[s]);
                }
                this.o += 1;
            } else {
                r.setElement(e, t);
                const s = r.size();
                this.o += s - n;
            }
        }
        if (this.o > this.u * .75) {
            this.h();
        }
    }
    getElementByKey(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) return undefined;
        if (s instanceof _OrderedMap.default) {
            return s.getElementByKey(e);
        } else {
            for (const t of s) {
                if (t[0] === e) return t[1];
            }
            return undefined;
        }
    }
    eraseElementByKey(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) return;
        if (s instanceof _Vector.default) {
            let t = 0;
            for (const r of s) {
                if (r[0] === e) {
                    s.eraseElementByPos(t);
                    this.o -= 1;
                    return;
                }
                t += 1;
            }
        } else {
            const r = s.size();
            s.eraseElementByKey(e);
            const n = s.size();
            this.o += n - r;
            if (n <= 6) {
                this.i[t] = new _Vector.default(s);
            }
        }
    }
    find(e) {
        const t = this.l(e) & this.u - 1;
        const s = this.i[t];
        if (!s) return false;
        if (s instanceof _OrderedMap.default) {
            return !s.find(e).equals(s.end());
        }
        for (const t of s) {
            if (t[0] === e) return true;
        }
        return false;
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

var _default = HashMap;

exports.default = _default;