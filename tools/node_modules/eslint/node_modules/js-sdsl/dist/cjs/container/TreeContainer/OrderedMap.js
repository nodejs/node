"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _TreeIterator = _interopRequireDefault(require("./Base/TreeIterator"));

var _throwError = require("../../utils/throwError");

function _interopRequireDefault(r) {
    return r && r.t ? r : {
        default: r
    };
}

class OrderedMapIterator extends _TreeIterator.default {
    get pointer() {
        if (this.o === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        const r = this;
        return new Proxy([], {
            get(e, t) {
                if (t === "0") return r.o.u; else if (t === "1") return r.o.l;
            },
            set(e, t, s) {
                if (t !== "1") {
                    throw new TypeError("props must be 1");
                }
                r.o.l = s;
                return true;
            }
        });
    }
    copy() {
        return new OrderedMapIterator(this.o, this.h, this.iteratorType);
    }
}

class OrderedMap extends _Base.default {
    constructor(r = [], e, t) {
        super(e, t);
        const s = this;
        r.forEach((function(r) {
            s.setElement(r[0], r[1]);
        }));
    }
    * X(r) {
        if (r === undefined) return;
        yield* this.X(r.Y);
        yield [ r.u, r.l ];
        yield* this.X(r.Z);
    }
    begin() {
        return new OrderedMapIterator(this.h.Y || this.h, this.h);
    }
    end() {
        return new OrderedMapIterator(this.h, this.h);
    }
    rBegin() {
        return new OrderedMapIterator(this.h.Z || this.h, this.h, 1);
    }
    rEnd() {
        return new OrderedMapIterator(this.h, this.h, 1);
    }
    front() {
        if (this.i === 0) return;
        const r = this.h.Y;
        return [ r.u, r.l ];
    }
    back() {
        if (this.i === 0) return;
        const r = this.h.Z;
        return [ r.u, r.l ];
    }
    lowerBound(r) {
        const e = this.$(this.rr, r);
        return new OrderedMapIterator(e, this.h);
    }
    upperBound(r) {
        const e = this.er(this.rr, r);
        return new OrderedMapIterator(e, this.h);
    }
    reverseLowerBound(r) {
        const e = this.tr(this.rr, r);
        return new OrderedMapIterator(e, this.h);
    }
    reverseUpperBound(r) {
        const e = this.sr(this.rr, r);
        return new OrderedMapIterator(e, this.h);
    }
    setElement(r, e, t) {
        return this.M(r, e, t);
    }
    find(r) {
        const e = this.I(this.rr, r);
        return new OrderedMapIterator(e, this.h);
    }
    getElementByKey(r) {
        const e = this.I(this.rr, r);
        return e.l;
    }
    union(r) {
        const e = this;
        r.forEach((function(r) {
            e.setElement(r[0], r[1]);
        }));
        return this.i;
    }
    [Symbol.iterator]() {
        return this.X(this.rr);
    }
}

var _default = OrderedMap;

exports.default = _default;
//# sourceMappingURL=OrderedMap.js.map
