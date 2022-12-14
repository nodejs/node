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

var __generator = this && this.u || function(e, r) {
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

var __values = this && this.Z || function(e) {
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

import { throwIteratorAccessError } from "../../utils/throwError";

var OrderedSetIterator = function(e) {
    __extends(OrderedSetIterator, e);
    function OrderedSetIterator() {
        return e !== null && e.apply(this, arguments) || this;
    }
    Object.defineProperty(OrderedSetIterator.prototype, "pointer", {
        get: function() {
            if (this.o === this.h) {
                throwIteratorAccessError();
            }
            return this.o.p;
        },
        enumerable: false,
        configurable: true
    });
    OrderedSetIterator.prototype.copy = function() {
        return new OrderedSetIterator(this.o, this.h, this.iteratorType);
    };
    return OrderedSetIterator;
}(TreeIterator);

var OrderedSet = function(e) {
    __extends(OrderedSet, e);
    function OrderedSet(r, t, n) {
        if (r === void 0) {
            r = [];
        }
        var i = e.call(this, t, n) || this;
        var o = i;
        r.forEach((function(e) {
            o.insert(e);
        }));
        return i;
    }
    OrderedSet.prototype.rr = function(e) {
        return __generator(this, (function(r) {
            switch (r.label) {
              case 0:
                if (e === undefined) return [ 2 ];
                return [ 5, __values(this.rr(e.er)) ];

              case 1:
                r.sent();
                return [ 4, e.p ];

              case 2:
                r.sent();
                return [ 5, __values(this.rr(e.tr)) ];

              case 3:
                r.sent();
                return [ 2 ];
            }
        }));
    };
    OrderedSet.prototype.begin = function() {
        return new OrderedSetIterator(this.h.er || this.h, this.h);
    };
    OrderedSet.prototype.end = function() {
        return new OrderedSetIterator(this.h, this.h);
    };
    OrderedSet.prototype.rBegin = function() {
        return new OrderedSetIterator(this.h.tr || this.h, this.h, 1);
    };
    OrderedSet.prototype.rEnd = function() {
        return new OrderedSetIterator(this.h, this.h, 1);
    };
    OrderedSet.prototype.front = function() {
        return this.h.er ? this.h.er.p : undefined;
    };
    OrderedSet.prototype.back = function() {
        return this.h.tr ? this.h.tr.p : undefined;
    };
    OrderedSet.prototype.insert = function(e, r) {
        return this.v(e, undefined, r);
    };
    OrderedSet.prototype.find = function(e) {
        var r = this.g(this.ir, e);
        return new OrderedSetIterator(r, this.h);
    };
    OrderedSet.prototype.lowerBound = function(e) {
        var r = this.nr(this.ir, e);
        return new OrderedSetIterator(r, this.h);
    };
    OrderedSet.prototype.upperBound = function(e) {
        var r = this.ar(this.ir, e);
        return new OrderedSetIterator(r, this.h);
    };
    OrderedSet.prototype.reverseLowerBound = function(e) {
        var r = this.ur(this.ir, e);
        return new OrderedSetIterator(r, this.h);
    };
    OrderedSet.prototype.reverseUpperBound = function(e) {
        var r = this.sr(this.ir, e);
        return new OrderedSetIterator(r, this.h);
    };
    OrderedSet.prototype.union = function(e) {
        var r = this;
        e.forEach((function(e) {
            r.insert(e);
        }));
        return this.i;
    };
    OrderedSet.prototype[Symbol.iterator] = function() {
        return this.rr(this.ir);
    };
    return OrderedSet;
}(TreeContainer);

export default OrderedSet;
//# sourceMappingURL=OrderedSet.js.map
