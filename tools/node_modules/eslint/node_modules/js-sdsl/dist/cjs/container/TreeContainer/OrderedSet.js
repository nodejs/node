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

class OrderedSetIterator extends _TreeIterator.default {
    constructor(t, e, r, s) {
        super(t, e, s);
        this.container = r;
    }
    get pointer() {
        if (this.o === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        return this.o.u;
    }
    copy() {
        return new OrderedSetIterator(this.o, this.h, this.container, this.iteratorType);
    }
}

class OrderedSet extends _Base.default {
    constructor(t = [], e, r) {
        super(e, r);
        const s = this;
        t.forEach((function(t) {
            s.insert(t);
        }));
    }
    begin() {
        return new OrderedSetIterator(this.h.T || this.h, this.h, this);
    }
    end() {
        return new OrderedSetIterator(this.h, this.h, this);
    }
    rBegin() {
        return new OrderedSetIterator(this.h.K || this.h, this.h, this, 1);
    }
    rEnd() {
        return new OrderedSetIterator(this.h, this.h, this, 1);
    }
    front() {
        return this.h.T ? this.h.T.u : undefined;
    }
    back() {
        return this.h.K ? this.h.K.u : undefined;
    }
    lowerBound(t) {
        const e = this.U(this.X, t);
        return new OrderedSetIterator(e, this.h, this);
    }
    upperBound(t) {
        const e = this.Y(this.X, t);
        return new OrderedSetIterator(e, this.h, this);
    }
    reverseLowerBound(t) {
        const e = this.Z(this.X, t);
        return new OrderedSetIterator(e, this.h, this);
    }
    reverseUpperBound(t) {
        const e = this.$(this.X, t);
        return new OrderedSetIterator(e, this.h, this);
    }
    forEach(t) {
        this.tt((function(e, r, s) {
            t(e.u, r, s);
        }));
    }
    insert(t, e) {
        return this.M(t, undefined, e);
    }
    getElementByPos(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        const e = this.tt(t);
        return e.u;
    }
    find(t) {
        const e = this.rt(this.X, t);
        return new OrderedSetIterator(e, this.h, this);
    }
    union(t) {
        const e = this;
        t.forEach((function(t) {
            e.insert(t);
        }));
        return this.i;
    }
    * [Symbol.iterator]() {
        const t = this.i;
        const e = this.tt();
        for (let r = 0; r < t; ++r) {
            yield e[r].u;
        }
    }
}

var _default = OrderedSet;

exports.default = _default;
//# sourceMappingURL=OrderedSet.js.map
