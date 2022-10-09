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

var __generator = this && this.i || function(r, e) {
    var t = {
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
        return function(e) {
            return step([ r, e ]);
        };
    }
    function step(o) {
        if (n) throw new TypeError("Generator is already executing.");
        while (t) try {
            if (n = 1, i && (a = o[0] & 2 ? i["return"] : o[0] ? i["throw"] || ((a = i["return"]) && a.call(i), 
            0) : i.next) && !(a = a.call(i, o[1])).done) return a;
            if (i = 0, a) o = [ o[0] & 2, a.value ];
            switch (o[0]) {
              case 0:
              case 1:
                a = o;
                break;

              case 4:
                t.label++;
                return {
                    value: o[1],
                    done: false
                };

              case 5:
                t.label++;
                i = o[1];
                o = [ 0 ];
                continue;

              case 7:
                o = t.ops.pop();
                t.trys.pop();
                continue;

              default:
                if (!(a = t.trys, a = a.length > 0 && a[a.length - 1]) && (o[0] === 6 || o[0] === 2)) {
                    t = 0;
                    continue;
                }
                if (o[0] === 3 && (!a || o[1] > a[0] && o[1] < a[3])) {
                    t.label = o[1];
                    break;
                }
                if (o[0] === 6 && t.label < a[1]) {
                    t.label = a[1];
                    a = o;
                    break;
                }
                if (a && t.label < a[2]) {
                    t.label = a[2];
                    t.ops.push(o);
                    break;
                }
                if (a[2]) t.ops.pop();
                t.trys.pop();
                continue;
            }
            o = e.call(r, t);
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

var __read = this && this._ || function(r, e) {
    var t = typeof Symbol === "function" && r[Symbol.iterator];
    if (!t) return r;
    var n = t.call(r), i, a = [], o;
    try {
        while ((e === void 0 || e-- > 0) && !(i = n.next()).done) a.push(i.value);
    } catch (r) {
        o = {
            error: r
        };
    } finally {
        try {
            if (i && !i.done && (t = n["return"])) t.call(n);
        } finally {
            if (o) throw o.error;
        }
    }
    return a;
};

var __values = this && this.u || function(r) {
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

var OrderedMapIterator = function(r) {
    __extends(OrderedMapIterator, r);
    function OrderedMapIterator() {
        return r !== null && r.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedMapIterator.prototype, "pointer", {
        get: function() {
            var r = this;
            if (this.D === this.J) {
                throw new RangeError("OrderedMap iterator access denied");
            }
            return new Proxy([], {
                get: function(e, t) {
                    if (t === "0") return r.D.W; else if (t === "1") return r.D.L;
                },
                set: function(e, t, n) {
                    if (t !== "1") {
                        throw new TypeError("props must be 1");
                    }
                    r.D.L = n;
                    return true;
                }
            });
        },
        enumerable: false,
        configurable: true
    });
    OrderedMapIterator.prototype.copy = function() {
        return new OrderedMapIterator(this.D, this.J, this.iteratorType);
    };
    return OrderedMapIterator;
}(TreeIterator);

export { OrderedMapIterator };

var OrderedMap = function(r) {
    __extends(OrderedMap, r);
    function OrderedMap(e, t, n) {
        if (e === void 0) {
            e = [];
        }
        var i = r.call(this, t, n) || this;
        i.X = function(r) {
            return __generator(this, (function(e) {
                switch (e.label) {
                  case 0:
                    if (r === undefined) return [ 2 ];
                    return [ 5, __values(this.X(r.Y)) ];

                  case 1:
                    e.sent();
                    return [ 4, [ r.W, r.L ] ];

                  case 2:
                    e.sent();
                    return [ 5, __values(this.X(r.Z)) ];

                  case 3:
                    e.sent();
                    return [ 2 ];
                }
            }));
        };
        e.forEach((function(r) {
            var e = __read(r, 2), t = e[0], n = e[1];
            return i.setElement(t, n);
        }));
        return i;
    }
    OrderedMap.prototype.begin = function() {
        return new OrderedMapIterator(this.J.Y || this.J, this.J);
    };
    OrderedMap.prototype.end = function() {
        return new OrderedMapIterator(this.J, this.J);
    };
    OrderedMap.prototype.rBegin = function() {
        return new OrderedMapIterator(this.J.Z || this.J, this.J, 1);
    };
    OrderedMap.prototype.rEnd = function() {
        return new OrderedMapIterator(this.J, this.J, 1);
    };
    OrderedMap.prototype.front = function() {
        if (!this.o) return undefined;
        var r = this.J.Y;
        return [ r.W, r.L ];
    };
    OrderedMap.prototype.back = function() {
        if (!this.o) return undefined;
        var r = this.J.Z;
        return [ r.W, r.L ];
    };
    OrderedMap.prototype.forEach = function(r) {
        var e, t;
        var n = 0;
        try {
            for (var i = __values(this), a = i.next(); !a.done; a = i.next()) {
                var o = a.value;
                r(o, n++);
            }
        } catch (r) {
            e = {
                error: r
            };
        } finally {
            try {
                if (a && !a.done && (t = i.return)) t.call(i);
            } finally {
                if (e) throw e.error;
            }
        }
    };
    OrderedMap.prototype.lowerBound = function(r) {
        var e = this.$(this.rr, r);
        return new OrderedMapIterator(e, this.J);
    };
    OrderedMap.prototype.upperBound = function(r) {
        var e = this.er(this.rr, r);
        return new OrderedMapIterator(e, this.J);
    };
    OrderedMap.prototype.reverseLowerBound = function(r) {
        var e = this.tr(this.rr, r);
        return new OrderedMapIterator(e, this.J);
    };
    OrderedMap.prototype.reverseUpperBound = function(r) {
        var e = this.nr(this.rr, r);
        return new OrderedMapIterator(e, this.J);
    };
    OrderedMap.prototype.setElement = function(r, e, t) {
        this.ir(r, e, t);
    };
    OrderedMap.prototype.find = function(r) {
        var e = this.ar(this.rr, r);
        if (e !== undefined) {
            return new OrderedMapIterator(e, this.J);
        }
        return this.end();
    };
    OrderedMap.prototype.getElementByKey = function(r) {
        var e = this.ar(this.rr, r);
        return e ? e.L : undefined;
    };
    OrderedMap.prototype.getElementByPos = function(r) {
        var e, t;
        if (r < 0 || r > this.o - 1) {
            throw new RangeError;
        }
        var n;
        var i = 0;
        try {
            for (var a = __values(this), o = a.next(); !o.done; o = a.next()) {
                var u = o.value;
                if (i === r) {
                    n = u;
                    break;
                }
                i += 1;
            }
        } catch (r) {
            e = {
                error: r
            };
        } finally {
            try {
                if (o && !o.done && (t = a.return)) t.call(a);
            } finally {
                if (e) throw e.error;
            }
        }
        return n;
    };
    OrderedMap.prototype.union = function(r) {
        var e = this;
        r.forEach((function(r) {
            var t = __read(r, 2), n = t[0], i = t[1];
            return e.setElement(n, i);
        }));
    };
    OrderedMap.prototype[Symbol.iterator] = function() {
        return this.X(this.rr);
    };
    return OrderedMap;
}(TreeContainer);

export default OrderedMap;