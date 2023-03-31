var __extends = this && this.t || function() {
    var extendStatics = function(r, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(r, t) {
            r.__proto__ = t;
        } || function(r, t) {
            for (var e in t) if (Object.prototype.hasOwnProperty.call(t, e)) r[e] = t[e];
        };
        return extendStatics(r, t);
    };
    return function(r, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(r, t);
        function __() {
            this.constructor = r;
        }
        r.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

var __generator = this && this.h || function(r, t) {
    var e = {
        label: 0,
        sent: function() {
            if (a[0] & 1) throw a[1];
            return a[1];
        },
        trys: [],
        ops: []
    }, n, i, a, o;
    return o = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (o[Symbol.iterator] = function() {
        return this;
    }), o;
    function verb(r) {
        return function(t) {
            return step([ r, t ]);
        };
    }
    function step(o) {
        if (n) throw new TypeError("Generator is already executing.");
        while (e) try {
            if (n = 1, i && (a = o[0] & 2 ? i["return"] : o[0] ? i["throw"] || ((a = i["return"]) && a.call(i), 
            0) : i.next) && !(a = a.call(i, o[1])).done) return a;
            if (i = 0, a) o = [ o[0] & 2, a.value ];
            switch (o[0]) {
              case 0:
              case 1:
                a = o;
                break;

              case 4:
                e.label++;
                return {
                    value: o[1],
                    done: false
                };

              case 5:
                e.label++;
                i = o[1];
                o = [ 0 ];
                continue;

              case 7:
                o = e.ops.pop();
                e.trys.pop();
                continue;

              default:
                if (!(a = e.trys, a = a.length > 0 && a[a.length - 1]) && (o[0] === 6 || o[0] === 2)) {
                    e = 0;
                    continue;
                }
                if (o[0] === 3 && (!a || o[1] > a[0] && o[1] < a[3])) {
                    e.label = o[1];
                    break;
                }
                if (o[0] === 6 && e.label < a[1]) {
                    e.label = a[1];
                    a = o;
                    break;
                }
                if (a && e.label < a[2]) {
                    e.label = a[2];
                    e.ops.push(o);
                    break;
                }
                if (a[2]) e.ops.pop();
                e.trys.pop();
                continue;
            }
            o = t.call(r, e);
        } catch (r) {
            o = [ 6, r ];
            i = 0;
        } finally {
            n = a = 0;
        }
        if (o[0] & 5) throw o[1];
        return {
            value: o[0] ? o[1] : void 0,
            done: true
        };
    }
};

import TreeContainer from "./Base";

import TreeIterator from "./Base/TreeIterator";

import { throwIteratorAccessError } from "../../utils/throwError";

var OrderedMapIterator = function(r) {
    __extends(OrderedMapIterator, r);
    function OrderedMapIterator(t, e, n, i) {
        var a = r.call(this, t, e, i) || this;
        a.container = n;
        return a;
    }
    Object.defineProperty(OrderedMapIterator.prototype, "pointer", {
        get: function() {
            if (this.o === this.u) {
                throwIteratorAccessError();
            }
            var r = this;
            return new Proxy([], {
                get: function(t, e) {
                    if (e === "0") return r.o.p; else if (e === "1") return r.o.H;
                },
                set: function(t, e, n) {
                    if (e !== "1") {
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
        return new OrderedMapIterator(this.o, this.u, this.container, this.iteratorType);
    };
    return OrderedMapIterator;
}(TreeIterator);

var OrderedMap = function(r) {
    __extends(OrderedMap, r);
    function OrderedMap(t, e, n) {
        if (t === void 0) {
            t = [];
        }
        var i = r.call(this, e, n) || this;
        var a = i;
        t.forEach((function(r) {
            a.setElement(r[0], r[1]);
        }));
        return i;
    }
    OrderedMap.prototype.begin = function() {
        return new OrderedMapIterator(this.u.Y || this.u, this.u, this);
    };
    OrderedMap.prototype.end = function() {
        return new OrderedMapIterator(this.u, this.u, this);
    };
    OrderedMap.prototype.rBegin = function() {
        return new OrderedMapIterator(this.u.Z || this.u, this.u, this, 1);
    };
    OrderedMap.prototype.rEnd = function() {
        return new OrderedMapIterator(this.u, this.u, this, 1);
    };
    OrderedMap.prototype.front = function() {
        if (this.i === 0) return;
        var r = this.u.Y;
        return [ r.p, r.H ];
    };
    OrderedMap.prototype.back = function() {
        if (this.i === 0) return;
        var r = this.u.Z;
        return [ r.p, r.H ];
    };
    OrderedMap.prototype.lowerBound = function(r) {
        var t = this.$(this.rr, r);
        return new OrderedMapIterator(t, this.u, this);
    };
    OrderedMap.prototype.upperBound = function(r) {
        var t = this.tr(this.rr, r);
        return new OrderedMapIterator(t, this.u, this);
    };
    OrderedMap.prototype.reverseLowerBound = function(r) {
        var t = this.er(this.rr, r);
        return new OrderedMapIterator(t, this.u, this);
    };
    OrderedMap.prototype.reverseUpperBound = function(r) {
        var t = this.nr(this.rr, r);
        return new OrderedMapIterator(t, this.u, this);
    };
    OrderedMap.prototype.forEach = function(r) {
        this.ir((function(t, e, n) {
            r([ t.p, t.H ], e, n);
        }));
    };
    OrderedMap.prototype.setElement = function(r, t, e) {
        return this.v(r, t, e);
    };
    OrderedMap.prototype.getElementByPos = function(r) {
        if (r < 0 || r > this.i - 1) {
            throw new RangeError;
        }
        var t = this.ir(r);
        return [ t.p, t.H ];
    };
    OrderedMap.prototype.find = function(r) {
        var t = this.ar(this.rr, r);
        return new OrderedMapIterator(t, this.u, this);
    };
    OrderedMap.prototype.getElementByKey = function(r) {
        var t = this.ar(this.rr, r);
        return t.H;
    };
    OrderedMap.prototype.union = function(r) {
        var t = this;
        r.forEach((function(r) {
            t.setElement(r[0], r[1]);
        }));
        return this.i;
    };
    OrderedMap.prototype[Symbol.iterator] = function() {
        var r, t, e, n;
        return __generator(this, (function(i) {
            switch (i.label) {
              case 0:
                r = this.i;
                t = this.ir();
                e = 0;
                i.label = 1;

              case 1:
                if (!(e < r)) return [ 3, 4 ];
                n = t[e];
                return [ 4, [ n.p, n.H ] ];

              case 2:
                i.sent();
                i.label = 3;

              case 3:
                ++e;
                return [ 3, 1 ];

              case 4:
                return [ 2 ];
            }
        }));
    };
    return OrderedMap;
}(TreeContainer);

export default OrderedMap;
//# sourceMappingURL=OrderedMap.js.map
