"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = exports.OrderedMapIterator = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _TreeIterator = _interopRequireDefault(require("./Base/TreeIterator"));

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class OrderedMapIterator extends _TreeIterator.default {
    get pointer() {
        if (this.I === this.S) {
            throw new RangeError("OrderedMap iterator access denied");
        }
        return new Proxy([], {
            get: (e, r) => {
                if (r === "0") return this.I.T; else if (r === "1") return this.I.L;
            },
            set: (e, r, t) => {
                if (r !== "1") {
                    throw new TypeError("props must be 1");
                }
                this.I.L = t;
                return true;
            }
        });
    }
    copy() {
        return new OrderedMapIterator(this.I, this.S, this.iteratorType);
    }
}

exports.OrderedMapIterator = OrderedMapIterator;

class OrderedMap extends _Base.default {
    constructor(e = [], r, t) {
        super(r, t);
        this.K = function*(e) {
            if (e === undefined) return;
            yield* this.K(e.U);
            yield [ e.T, e.L ];
            yield* this.K(e.J);
        };
        e.forEach((([e, r]) => this.setElement(e, r)));
    }
    begin() {
        return new OrderedMapIterator(this.S.U || this.S, this.S);
    }
    end() {
        return new OrderedMapIterator(this.S, this.S);
    }
    rBegin() {
        return new OrderedMapIterator(this.S.J || this.S, this.S, 1);
    }
    rEnd() {
        return new OrderedMapIterator(this.S, this.S, 1);
    }
    front() {
        if (!this.o) return undefined;
        const e = this.S.U;
        return [ e.T, e.L ];
    }
    back() {
        if (!this.o) return undefined;
        const e = this.S.J;
        return [ e.T, e.L ];
    }
    forEach(e) {
        let r = 0;
        for (const t of this) e(t, r++);
    }
    lowerBound(e) {
        const r = this.W(this.X, e);
        return new OrderedMapIterator(r, this.S);
    }
    upperBound(e) {
        const r = this.Y(this.X, e);
        return new OrderedMapIterator(r, this.S);
    }
    reverseLowerBound(e) {
        const r = this.Z(this.X, e);
        return new OrderedMapIterator(r, this.S);
    }
    reverseUpperBound(e) {
        const r = this.$(this.X, e);
        return new OrderedMapIterator(r, this.S);
    }
    setElement(e, r, t) {
        this.ee(e, r, t);
    }
    find(e) {
        const r = this.re(this.X, e);
        if (r !== undefined) {
            return new OrderedMapIterator(r, this.S);
        }
        return this.end();
    }
    getElementByKey(e) {
        const r = this.re(this.X, e);
        return r ? r.L : undefined;
    }
    getElementByPos(e) {
        if (e < 0 || e > this.o - 1) {
            throw new RangeError;
        }
        let r;
        let t = 0;
        for (const s of this) {
            if (t === e) {
                r = s;
                break;
            }
            t += 1;
        }
        return r;
    }
    union(e) {
        e.forEach((([e, r]) => this.setElement(e, r)));
    }
    [Symbol.iterator]() {
        return this.K(this.X);
    }
}

var _default = OrderedMap;

exports.default = _default;