"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = exports.OrderedSetIterator = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _TreeIterator = _interopRequireDefault(require("./Base/TreeIterator"));

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class OrderedSetIterator extends _TreeIterator.default {
    get pointer() {
        if (this.I === this.S) {
            throw new RangeError("OrderedSet iterator access denied!");
        }
        return this.I.T;
    }
    copy() {
        return new OrderedSetIterator(this.I, this.S, this.iteratorType);
    }
}

exports.OrderedSetIterator = OrderedSetIterator;

class OrderedSet extends _Base.default {
    constructor(e = [], t, r) {
        super(t, r);
        this.K = function*(e) {
            if (e === undefined) return;
            yield* this.K(e.U);
            yield e.T;
            yield* this.K(e.J);
        };
        e.forEach((e => this.insert(e)));
    }
    begin() {
        return new OrderedSetIterator(this.S.U || this.S, this.S);
    }
    end() {
        return new OrderedSetIterator(this.S, this.S);
    }
    rBegin() {
        return new OrderedSetIterator(this.S.J || this.S, this.S, 1);
    }
    rEnd() {
        return new OrderedSetIterator(this.S, this.S, 1);
    }
    front() {
        return this.S.U ? this.S.U.T : undefined;
    }
    back() {
        return this.S.J ? this.S.J.T : undefined;
    }
    forEach(e) {
        let t = 0;
        for (const r of this) e(r, t++);
    }
    getElementByPos(e) {
        if (e < 0 || e > this.o - 1) {
            throw new RangeError;
        }
        let t;
        let r = 0;
        for (const i of this) {
            if (r === e) {
                t = i;
                break;
            }
            r += 1;
        }
        return t;
    }
    insert(e, t) {
        this.ee(e, undefined, t);
    }
    find(e) {
        const t = this.re(this.X, e);
        if (t !== undefined) {
            return new OrderedSetIterator(t, this.S);
        }
        return this.end();
    }
    lowerBound(e) {
        const t = this.W(this.X, e);
        return new OrderedSetIterator(t, this.S);
    }
    upperBound(e) {
        const t = this.Y(this.X, e);
        return new OrderedSetIterator(t, this.S);
    }
    reverseLowerBound(e) {
        const t = this.Z(this.X, e);
        return new OrderedSetIterator(t, this.S);
    }
    reverseUpperBound(e) {
        const t = this.$(this.X, e);
        return new OrderedSetIterator(t, this.S);
    }
    union(e) {
        e.forEach((e => this.insert(e)));
    }
    [Symbol.iterator]() {
        return this.K(this.X);
    }
}

var _default = OrderedSet;

exports.default = _default;