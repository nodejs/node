var __extends = this && this.t || function() {
    var extendStatics = function(r, e) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(r, e) {
            r.__proto__ = e;
        } || function(r, e) {
            for (var t in e) if (Object.prototype.hasOwnProperty.call(e, t)) r[t] = e[t];
        };
        return extendStatics(r, e);
    };
    return function(r, e) {
        if (typeof e !== "function" && e !== null) throw new TypeError("Class extends value " + String(e) + " is not a constructor or null");
        extendStatics(r, e);
        function __() {
            this.constructor = r;
        }
        r.prototype = e === null ? Object.create(e) : (__.prototype = e.prototype, new __);
    };
}();

var __generator = this && this.u || function(r, e) {
    var t = {
        label: 0,
        sent: function() {
            if (o[0] & 1) throw o[1];
            return o[1];
        },
        trys: [],
        ops: []
    }, n, i, o, a;
    return a = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (a[Symbol.iterator] = function() {
        return this;
    }), a;
    function verb(r) {
        return function(e) {
            return step([ r, e ]);
        };
    }
    function step(a) {
        if (n) throw new TypeError("Generator is already executing.");
        while (t) try {
            if (n = 1, i && (o = a[0] & 2 ? i["return"] : a[0] ? i["throw"] || ((o = i["return"]) && o.call(i), 
            0) : i.next) && !(o = o.call(i, a[1])).done) return o;
            if (i = 0, o) a = [ a[0] & 2, o.value ];
            switch (a[0]) {
              case 0:
              case 1:
                o = a;
                break;

              case 4:
                t.label++;
                return {
                    value: a[1],
                    done: false
                };

              case 5:
                t.label++;
                i = a[1];
                a = [ 0 ];
                continue;

              case 7:
                a = t.ops.pop();
                t.trys.pop();
                continue;

              default:
                if (!(o = t.trys, o = o.length > 0 && o[o.length - 1]) && (a[0] === 6 || a[0] === 2)) {
                    t = 0;
                    continue;
                }
                if (a[0] === 3 && (!o || a[1] > o[0] && a[1] < o[3])) {
                    t.label = a[1];
                    break;
                }
                if (a[0] === 6 && t.label < o[1]) {
                    t.label = o[1];
                    o = a;
                    break;
                }
                if (o && t.label < o[2]) {
                    t.label = o[2];
                    t.ops.push(a);
                    break;
                }
                if (o[2]) t.ops.pop();
                t.trys.pop();
                continue;
            }
            a = e.call(r, t);
        } catch (r) {
            a = [ 6, r ];
            i = 0;
        } finally {
            n = o = 0;
        }
        if (a[0] & 5) throw a[1];
        return {
            value: a[0] ? a[1] : void 0,
            done: true
        };
    }
};

var __values = this && this.Z || function(r) {
    var e = typeof Symbol === "function" && Symbol.iterator, t = e && r[e], n = 0;
    if (t) return t.call(r);
    if (r && typeof r.length === "number") return {
        next: function() {
            if (r && n >= r.length) r = void 0;
            return {
                value: r && r[n++],
                done: !r
            };
        }
    };
    throw new TypeError(e ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import TreeContainer from "./Base";

import TreeIterator from "./Base/TreeIterator";

import { throwIteratorAccessError } from "../../utils/throwError";

var OrderedMapIterator = function(r) {
    __extends(OrderedMapIterator, r);
    function OrderedMapIterator() {
        return r !== null && r.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedMapIterator.prototype, "pointer", {
        get: function() {
            if (this.o === this.h) {
                throwIteratorAccessError();
            }
            var r = this;
            return new Proxy([], {
                get: function(e, t) {
                    if (t === "0") return r.o.p; else if (t === "1") return r.o.H;
                },
                set: function(e, t, n) {
                    if (t !== "1") {
                        throw new TypeError("props must be 1");
                    }
                    r.o.H = n;
                    return true;
                }
            });
        },
        enumerable: false,
        configurable: true
    });
    OrderedMapIterator.prototype.copy = function() {
        return new OrderedMapIterator(this.o, this.h, this.iteratorType);
    };
    return OrderedMapIterator;
}(TreeIterator);

var OrderedMap = function(r) {
    __extends(OrderedMap, r);
    function OrderedMap(e, t, n) {
        if (e === void 0) {
            e = [];
        }
        var i = r.call(this, t, n) || this;
        var o = i;
        e.forEach((function(r) {
            o.setElement(r[0], r[1]);
        }));
        return i;
    }
    OrderedMap.prototype.rr = function(r) {
        return __generator(this, (function(e) {
            switch (e.label) {
              case 0:
                if (r === undefined) return [ 2 ];
                return [ 5, __values(this.rr(r.er)) ];

              case 1:
                e.sent();
                return [ 4, [ r.p, r.H ] ];

              case 2:
                e.sent();
                return [ 5, __values(this.rr(r.tr)) ];

              case 3:
                e.sent();
                return [ 2 ];
            }
        }));
    };
    OrderedMap.prototype.begin = function() {
        return new OrderedMapIterator(this.h.er || this.h, this.h);
    };
    OrderedMap.prototype.end = function() {
        return new OrderedMapIterator(this.h, this.h);
    };
    OrderedMap.prototype.rBegin = function() {
        return new OrderedMapIterator(this.h.tr || this.h, this.h, 1);
    };
    OrderedMap.prototype.rEnd = function() {
        return new OrderedMapIterator(this.h, this.h, 1);
    };
    OrderedMap.prototype.front = function() {
        if (this.i === 0) return;
        var r = this.h.er;
        return [ r.p, r.H ];
    };
    OrderedMap.prototype.back = function() {
        if (this.i === 0) return;
        var r = this.h.tr;
        return [ r.p, r.H ];
    };
    OrderedMap.prototype.lowerBound = function(r) {
        var e = this.nr(this.ir, r);
        return new OrderedMapIterator(e, this.h);
    };
    OrderedMap.prototype.upperBound = function(r) {
        var e = this.ar(this.ir, r);
        return new OrderedMapIterator(e, this.h);
    };
    OrderedMap.prototype.reverseLowerBound = function(r) {
        var e = this.ur(this.ir, r);
        return new OrderedMapIterator(e, this.h);
    };
    OrderedMap.prototype.reverseUpperBound = function(r) {
        var e = this.sr(this.ir, r);
        return new OrderedMapIterator(e, this.h);
    };
    OrderedMap.prototype.setElement = function(r, e, t) {
        return this.v(r, e, t);
    };
    OrderedMap.prototype.find = function(r) {
        var e = this.g(this.ir, r);
        return new OrderedMapIterator(e, this.h);
    };
    OrderedMap.prototype.getElementByKey = function(r) {
        var e = this.g(this.ir, r);
        return e.H;
    };
    OrderedMap.prototype.union = function(r) {
        var e = this;
        r.forEach((function(r) {
            e.setElement(r[0], r[1]);
        }));
        return this.i;
    };
    OrderedMap.prototype[Symbol.iterator] = function() {
        return this.rr(this.ir);
    };
    return OrderedMap;
}(TreeContainer);

export default OrderedMap;
//# sourceMappingURL=OrderedMap.js.map
