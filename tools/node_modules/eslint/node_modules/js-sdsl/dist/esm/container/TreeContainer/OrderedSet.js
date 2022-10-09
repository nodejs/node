var __extends = this && this.t || function() {
    var extendStatics = function(e, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, r) {
            e.__proto__ = r;
        } || function(e, r) {
            for (var t in r) if (Object.prototype.hasOwnProperty.call(r, t)) e[t] = r[t];
        };
        return extendStatics(e, r);
    };
    return function(e, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(e, r);
        function __() {
            this.constructor = e;
        }
        e.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

var __generator = this && this.i || function(e, r) {
    var t = {
        label: 0,
        sent: function() {
            if (o[0] & 1) throw o[1];
            return o[1];
        },
        trys: [],
        ops: []
    }, n, i, o, u;
    return u = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (u[Symbol.iterator] = function() {
        return this;
    }), u;
    function verb(e) {
        return function(r) {
            return step([ e, r ]);
        };
    }
    function step(u) {
        if (n) throw new TypeError("Generator is already executing.");
        while (t) try {
            if (n = 1, i && (o = u[0] & 2 ? i["return"] : u[0] ? i["throw"] || ((o = i["return"]) && o.call(i), 
            0) : i.next) && !(o = o.call(i, u[1])).done) return o;
            if (i = 0, o) u = [ u[0] & 2, o.value ];
            switch (u[0]) {
              case 0:
              case 1:
                o = u;
                break;

              case 4:
                t.label++;
                return {
                    value: u[1],
                    done: false
                };

              case 5:
                t.label++;
                i = u[1];
                u = [ 0 ];
                continue;

              case 7:
                u = t.ops.pop();
                t.trys.pop();
                continue;

              default:
                if (!(o = t.trys, o = o.length > 0 && o[o.length - 1]) && (u[0] === 6 || u[0] === 2)) {
                    t = 0;
                    continue;
                }
                if (u[0] === 3 && (!o || u[1] > o[0] && u[1] < o[3])) {
                    t.label = u[1];
                    break;
                }
                if (u[0] === 6 && t.label < o[1]) {
                    t.label = o[1];
                    o = u;
                    break;
                }
                if (o && t.label < o[2]) {
                    t.label = o[2];
                    t.ops.push(u);
                    break;
                }
                if (o[2]) t.ops.pop();
                t.trys.pop();
                continue;
            }
            u = r.call(e, t);
        } catch (e) {
            u = [ 6, e ];
            i = 0;
        } finally {
            n = o = 0;
        }
        if (u[0] & 5) throw u[1];
        return {
            value: u[0] ? u[1] : void 0,
            done: true
        };
    }
};

var __values = this && this.u || function(e) {
    var r = typeof Symbol === "function" && Symbol.iterator, t = r && e[r], n = 0;
    if (t) return t.call(e);
    if (e && typeof e.length === "number") return {
        next: function() {
            if (e && n >= e.length) e = void 0;
            return {
                value: e && e[n++],
                done: !e
            };
        }
    };
    throw new TypeError(r ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import TreeContainer from "./Base";

import TreeIterator from "./Base/TreeIterator";

var OrderedSetIterator = function(e) {
    __extends(OrderedSetIterator, e);
    function OrderedSetIterator() {
        return e !== null && e.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedSetIterator.prototype, "pointer", {
        get: function() {
            if (this.D === this.J) {
                throw new RangeError("OrderedSet iterator access denied!");
            }
            return this.D.W;
        },
        enumerable: false,
        configurable: true
    });
    OrderedSetIterator.prototype.copy = function() {
        return new OrderedSetIterator(this.D, this.J, this.iteratorType);
    };
    return OrderedSetIterator;
}(TreeIterator);

export { OrderedSetIterator };

var OrderedSet = function(e) {
    __extends(OrderedSet, e);
    function OrderedSet(r, t, n) {
        if (r === void 0) {
            r = [];
        }
        var i = e.call(this, t, n) || this;
        i.X = function(e) {
            return __generator(this, (function(r) {
                switch (r.label) {
                  case 0:
                    if (e === undefined) return [ 2 ];
                    return [ 5, __values(this.X(e.Y)) ];

                  case 1:
                    r.sent();
                    return [ 4, e.W ];

                  case 2:
                    r.sent();
                    return [ 5, __values(this.X(e.Z)) ];

                  case 3:
                    r.sent();
                    return [ 2 ];
                }
            }));
        };
        r.forEach((function(e) {
            return i.insert(e);
        }));
        return i;
    }
    OrderedSet.prototype.begin = function() {
        return new OrderedSetIterator(this.J.Y || this.J, this.J);
    };
    OrderedSet.prototype.end = function() {
        return new OrderedSetIterator(this.J, this.J);
    };
    OrderedSet.prototype.rBegin = function() {
        return new OrderedSetIterator(this.J.Z || this.J, this.J, 1);
    };
    OrderedSet.prototype.rEnd = function() {
        return new OrderedSetIterator(this.J, this.J, 1);
    };
    OrderedSet.prototype.front = function() {
        return this.J.Y ? this.J.Y.W : undefined;
    };
    OrderedSet.prototype.back = function() {
        return this.J.Z ? this.J.Z.W : undefined;
    };
    OrderedSet.prototype.forEach = function(e) {
        var r, t;
        var n = 0;
        try {
            for (var i = __values(this), o = i.next(); !o.done; o = i.next()) {
                var u = o.value;
                e(u, n++);
            }
        } catch (e) {
            r = {
                error: e
            };
        } finally {
            try {
                if (o && !o.done && (t = i.return)) t.call(i);
            } finally {
                if (r) throw r.error;
            }
        }
    };
    OrderedSet.prototype.getElementByPos = function(e) {
        var r, t;
        if (e < 0 || e > this.o - 1) {
            throw new RangeError;
        }
        var n;
        var i = 0;
        try {
            for (var o = __values(this), u = o.next(); !u.done; u = o.next()) {
                var d = u.value;
                if (i === e) {
                    n = d;
                    break;
                }
                i += 1;
            }
        } catch (e) {
            r = {
                error: e
            };
        } finally {
            try {
                if (u && !u.done && (t = o.return)) t.call(o);
            } finally {
                if (r) throw r.error;
            }
        }
        return n;
    };
    OrderedSet.prototype.insert = function(e, r) {
        this.ir(e, undefined, r);
    };
    OrderedSet.prototype.find = function(e) {
        var r = this.ar(this.rr, e);
        if (r !== undefined) {
            return new OrderedSetIterator(r, this.J);
        }
        return this.end();
    };
    OrderedSet.prototype.lowerBound = function(e) {
        var r = this.$(this.rr, e);
        return new OrderedSetIterator(r, this.J);
    };
    OrderedSet.prototype.upperBound = function(e) {
        var r = this.er(this.rr, e);
        return new OrderedSetIterator(r, this.J);
    };
    OrderedSet.prototype.reverseLowerBound = function(e) {
        var r = this.tr(this.rr, e);
        return new OrderedSetIterator(r, this.J);
    };
    OrderedSet.prototype.reverseUpperBound = function(e) {
        var r = this.nr(this.rr, e);
        return new OrderedSetIterator(r, this.J);
    };
    OrderedSet.prototype.union = function(e) {
        var r = this;
        e.forEach((function(e) {
            return r.insert(e);
        }));
    };
    OrderedSet.prototype[Symbol.iterator] = function() {
        return this.X(this.rr);
    };
    return OrderedSet;
}(TreeContainer);

export default OrderedSet;