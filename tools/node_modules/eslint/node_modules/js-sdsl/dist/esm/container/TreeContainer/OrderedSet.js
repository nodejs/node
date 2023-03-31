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

var __generator = this && this.h || function(e, r) {
    var t = {
        label: 0,
        sent: function() {
            if (o[0] & 1) throw o[1];
            return o[1];
        },
        trys: [],
        ops: []
    }, n, i, o, s;
    return s = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (s[Symbol.iterator] = function() {
        return this;
    }), s;
    function verb(e) {
        return function(r) {
            return step([ e, r ]);
        };
    }
    function step(s) {
        if (n) throw new TypeError("Generator is already executing.");
        while (t) try {
            if (n = 1, i && (o = s[0] & 2 ? i["return"] : s[0] ? i["throw"] || ((o = i["return"]) && o.call(i), 
            0) : i.next) && !(o = o.call(i, s[1])).done) return o;
            if (i = 0, o) s = [ s[0] & 2, o.value ];
            switch (s[0]) {
              case 0:
              case 1:
                o = s;
                break;

              case 4:
                t.label++;
                return {
                    value: s[1],
                    done: false
                };

              case 5:
                t.label++;
                i = s[1];
                s = [ 0 ];
                continue;

              case 7:
                s = t.ops.pop();
                t.trys.pop();
                continue;

              default:
                if (!(o = t.trys, o = o.length > 0 && o[o.length - 1]) && (s[0] === 6 || s[0] === 2)) {
                    t = 0;
                    continue;
                }
                if (s[0] === 3 && (!o || s[1] > o[0] && s[1] < o[3])) {
                    t.label = s[1];
                    break;
                }
                if (s[0] === 6 && t.label < o[1]) {
                    t.label = o[1];
                    o = s;
                    break;
                }
                if (o && t.label < o[2]) {
                    t.label = o[2];
                    t.ops.push(s);
                    break;
                }
                if (o[2]) t.ops.pop();
                t.trys.pop();
                continue;
            }
            s = r.call(e, t);
        } catch (e) {
            s = [ 6, e ];
            i = 0;
        } finally {
            n = o = 0;
        }
        if (s[0] & 5) throw s[1];
        return {
            value: s[0] ? s[1] : void 0,
            done: true
        };
    }
};

import TreeContainer from "./Base";

import TreeIterator from "./Base/TreeIterator";

import { throwIteratorAccessError } from "../../utils/throwError";

var OrderedSetIterator = function(e) {
    __extends(OrderedSetIterator, e);
    function OrderedSetIterator(r, t, n, i) {
        var o = e.call(this, r, t, i) || this;
        o.container = n;
        return o;
    }
    Object.defineProperty(OrderedSetIterator.prototype, "pointer", {
        get: function() {
            if (this.o === this.u) {
                throwIteratorAccessError();
            }
            return this.o.p;
        },
        enumerable: false,
        configurable: true
    });
    OrderedSetIterator.prototype.copy = function() {
        return new OrderedSetIterator(this.o, this.u, this.container, this.iteratorType);
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
    OrderedSet.prototype.begin = function() {
        return new OrderedSetIterator(this.u.Y || this.u, this.u, this);
    };
    OrderedSet.prototype.end = function() {
        return new OrderedSetIterator(this.u, this.u, this);
    };
    OrderedSet.prototype.rBegin = function() {
        return new OrderedSetIterator(this.u.Z || this.u, this.u, this, 1);
    };
    OrderedSet.prototype.rEnd = function() {
        return new OrderedSetIterator(this.u, this.u, this, 1);
    };
    OrderedSet.prototype.front = function() {
        return this.u.Y ? this.u.Y.p : undefined;
    };
    OrderedSet.prototype.back = function() {
        return this.u.Z ? this.u.Z.p : undefined;
    };
    OrderedSet.prototype.lowerBound = function(e) {
        var r = this.$(this.rr, e);
        return new OrderedSetIterator(r, this.u, this);
    };
    OrderedSet.prototype.upperBound = function(e) {
        var r = this.tr(this.rr, e);
        return new OrderedSetIterator(r, this.u, this);
    };
    OrderedSet.prototype.reverseLowerBound = function(e) {
        var r = this.er(this.rr, e);
        return new OrderedSetIterator(r, this.u, this);
    };
    OrderedSet.prototype.reverseUpperBound = function(e) {
        var r = this.nr(this.rr, e);
        return new OrderedSetIterator(r, this.u, this);
    };
    OrderedSet.prototype.forEach = function(e) {
        this.ir((function(r, t, n) {
            e(r.p, t, n);
        }));
    };
    OrderedSet.prototype.insert = function(e, r) {
        return this.v(e, undefined, r);
    };
    OrderedSet.prototype.getElementByPos = function(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        var r = this.ir(e);
        return r.p;
    };
    OrderedSet.prototype.find = function(e) {
        var r = this.ar(this.rr, e);
        return new OrderedSetIterator(r, this.u, this);
    };
    OrderedSet.prototype.union = function(e) {
        var r = this;
        e.forEach((function(e) {
            r.insert(e);
        }));
        return this.i;
    };
    OrderedSet.prototype[Symbol.iterator] = function() {
        var e, r, t;
        return __generator(this, (function(n) {
            switch (n.label) {
              case 0:
                e = this.i;
                r = this.ir();
                t = 0;
                n.label = 1;

              case 1:
                if (!(t < e)) return [ 3, 4 ];
                return [ 4, r[t].p ];

              case 2:
                n.sent();
                n.label = 3;

              case 3:
                ++t;
                return [ 3, 1 ];

              case 4:
                return [ 2 ];
            }
        }));
    };
    return OrderedSet;
}(TreeContainer);

export default OrderedSet;
//# sourceMappingURL=OrderedSet.js.map
