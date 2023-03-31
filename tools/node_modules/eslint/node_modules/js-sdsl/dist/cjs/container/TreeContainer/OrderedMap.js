"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _TreeIterator = _interopRequireDefault(require("./Base/TreeIterator"));

var _throwError = require("../../utils/throwError");

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class OrderedMapIterator extends _TreeIterator.default {
    constructor(t, r, e, s) {
        super(t, r, s);
        this.container = e;
    }
    get pointer() {
        if (this.o === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        const t = this;
        return new Proxy([], {
            get(r, e) {
                if (e === "0") return t.o.u; else if (e === "1") return t.o.l;
            },
            set(r, e, s) {
                if (e !== "1") {
                    throw new TypeError("props must be 1");
                }
                t.o.l = s;
                return true;
            }
        });
    }
    copy() {
        return new OrderedMapIterator(this.o, this.h, this.container, this.iteratorType);
    }
}

class OrderedMap extends _Base.default {
    constructor(t = [], r, e) {
        super(r, e);
        const s = this;
        t.forEach((function(t) {
            s.setElement(t[0], t[1]);
        }));
    }
    begin() {
        return new OrderedMapIterator(this.h.T || this.h, this.h, this);
    }
    end() {
        return new OrderedMapIterator(this.h, this.h, this);
    }
    rBegin() {
        return new OrderedMapIterator(this.h.K || this.h, this.h, this, 1);
    }
    rEnd() {
        return new OrderedMapIterator(this.h, this.h, this, 1);
    }
    front() {
        if (this.i === 0) return;
        const t = this.h.T;
        return [ t.u, t.l ];
    }
    back() {
        if (this.i === 0) return;
        const t = this.h.K;
        return [ t.u, t.l ];
    }
    lowerBound(t) {
        const r = this.U(this.X, t);
        return new OrderedMapIterator(r, this.h, this);
    }
    upperBound(t) {
        const r = this.Y(this.X, t);
        return new OrderedMapIterator(r, this.h, this);
    }
    reverseLowerBound(t) {
        const r = this.Z(this.X, t);
        return new OrderedMapIterator(r, this.h, this);
    }
    reverseUpperBound(t) {
        const r = this.$(this.X, t);
        return new OrderedMapIterator(r, this.h, this);
    }
    forEach(t) {
        this.tt((function(r, e, s) {
            t([ r.u, r.l ], e, s);
        }));
    }
    setElement(t, r, e) {
        return this.M(t, r, e);
    }
    getElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const r = this.tt(t);
        return [ r.u, r.l ];
    }
    find(t) {
        const r = this.rt(this.X, t);
        return new OrderedMapIterator(r, this.h, this);
    }
    getElementByKey(t) {
        const r = this.rt(this.X, t);
        return r.l;
    }
    union(t) {
        const r = this;
        t.forEach((function(t) {
            r.setElement(t[0], t[1]);
        }));
        return this.i;
    }
    * [Symbol.iterator]() {
        const t = this.i;
        const r = this.tt();
        for (let e = 0; e < t; ++e) {
            const t = r[e];
            yield [ t.u, t.l ];
        }
    }
}

var _default = OrderedMap;

exports.default = _default;
//# sourceMappingURL=OrderedMap.js.map
