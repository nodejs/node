"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _TreeIterator = _interopRequireDefault(require("./Base/TreeIterator"));

var _throwError = require("../../utils/throwError");

function _interopRequireDefault(e) {
    return e && e.t ? e : {
        default: e
    };
}

class OrderedSetIterator extends _TreeIterator.default {
    get pointer() {
        if (this.o === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        return this.o.u;
    }
    copy() {
        return new OrderedSetIterator(this.o, this.h, this.iteratorType);
    }
}

class OrderedSet extends _Base.default {
    constructor(e = [], r, t) {
        super(r, t);
        const i = this;
        e.forEach((function(e) {
            i.insert(e);
        }));
    }
    * X(e) {
        if (e === undefined) return;
        yield* this.X(e.Y);
        yield e.u;
        yield* this.X(e.Z);
    }
    begin() {
        return new OrderedSetIterator(this.h.Y || this.h, this.h);
    }
    end() {
        return new OrderedSetIterator(this.h, this.h);
    }
    rBegin() {
        return new OrderedSetIterator(this.h.Z || this.h, this.h, 1);
    }
    rEnd() {
        return new OrderedSetIterator(this.h, this.h, 1);
    }
    front() {
        return this.h.Y ? this.h.Y.u : undefined;
    }
    back() {
        return this.h.Z ? this.h.Z.u : undefined;
    }
    insert(e, r) {
        return this.M(e, undefined, r);
    }
    find(e) {
        const r = this.I(this.rr, e);
        return new OrderedSetIterator(r, this.h);
    }
    lowerBound(e) {
        const r = this.$(this.rr, e);
        return new OrderedSetIterator(r, this.h);
    }
    upperBound(e) {
        const r = this.er(this.rr, e);
        return new OrderedSetIterator(r, this.h);
    }
    reverseLowerBound(e) {
        const r = this.tr(this.rr, e);
        return new OrderedSetIterator(r, this.h);
    }
    reverseUpperBound(e) {
        const r = this.sr(this.rr, e);
        return new OrderedSetIterator(r, this.h);
    }
    union(e) {
        const r = this;
        e.forEach((function(e) {
            r.insert(e);
        }));
        return this.i;
    }
    [Symbol.iterator]() {
        return this.X(this.rr);
    }
}

var _default = OrderedSet;

exports.default = _default;
//# sourceMappingURL=OrderedSet.js.map
