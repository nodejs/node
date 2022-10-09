var __extends = this && this.t || function() {
    var extendStatics = function(t, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, r) {
            t.__proto__ = r;
        } || function(t, r) {
            for (var e in r) if (Object.prototype.hasOwnProperty.call(r, e)) t[e] = r[e];
        };
        return extendStatics(t, r);
    };
    return function(t, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(t, r);
        function __() {
            this.constructor = t;
        }
        t.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

var __generator = this && this.i || function(t, r) {
    var e = {
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
    function verb(t) {
        return function(r) {
            return step([ t, r ]);
        };
    }
    function step(s) {
        if (n) throw new TypeError("Generator is already executing.");
        while (e) try {
            if (n = 1, i && (o = s[0] & 2 ? i["return"] : s[0] ? i["throw"] || ((o = i["return"]) && o.call(i), 
            0) : i.next) && !(o = o.call(i, s[1])).done) return o;
            if (i = 0, o) s = [ s[0] & 2, o.value ];
            switch (s[0]) {
              case 0:
              case 1:
                o = s;
                break;

              case 4:
                e.label++;
                return {
                    value: s[1],
                    done: false
                };

              case 5:
                e.label++;
                i = s[1];
                s = [ 0 ];
                continue;

              case 7:
                s = e.ops.pop();
                e.trys.pop();
                continue;

              default:
                if (!(o = e.trys, o = o.length > 0 && o[o.length - 1]) && (s[0] === 6 || s[0] === 2)) {
                    e = 0;
                    continue;
                }
                if (s[0] === 3 && (!o || s[1] > o[0] && s[1] < o[3])) {
                    e.label = s[1];
                    break;
                }
                if (s[0] === 6 && e.label < o[1]) {
                    e.label = o[1];
                    o = s;
                    break;
                }
                if (o && e.label < o[2]) {
                    e.label = o[2];
                    e.ops.push(s);
                    break;
                }
                if (o[2]) e.ops.pop();
                e.trys.pop();
                continue;
            }
            s = r.call(t, e);
        } catch (t) {
            s = [ 6, t ];
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

var __read = this && this._ || function(t, r) {
    var e = typeof Symbol === "function" && t[Symbol.iterator];
    if (!e) return t;
    var n = e.call(t), i, o = [], s;
    try {
        while ((r === void 0 || r-- > 0) && !(i = n.next()).done) o.push(i.value);
    } catch (t) {
        s = {
            error: t
        };
    } finally {
        try {
            if (i && !i.done && (e = n["return"])) e.call(n);
        } finally {
            if (s) throw s.error;
        }
    }
    return o;
};

var __spreadArray = this && this.P || function(t, r, e) {
    if (e || arguments.length === 2) for (var n = 0, i = r.length, o; n < i; n++) {
        if (o || !(n in r)) {
            if (!o) o = Array.prototype.slice.call(r, 0, n);
            o[n] = r[n];
        }
    }
    return t.concat(o || Array.prototype.slice.call(r));
};

var __values = this && this.u || function(t) {
    var r = typeof Symbol === "function" && Symbol.iterator, e = r && t[r], n = 0;
    if (e) return e.call(t);
    if (t && typeof t.length === "number") return {
        next: function() {
            if (t && n >= t.length) t = void 0;
            return {
                value: t && t[n++],
                done: !t
            };
        }
    };
    throw new TypeError(r ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import SequentialContainer from "./Base";

import { RandomIterator } from "./Base/RandomIterator";

var VectorIterator = function(t) {
    __extends(VectorIterator, t);
    function VectorIterator() {
        return t !== null && t.apply(this, arguments) || this;
    }
    VectorIterator.prototype.copy = function() {
        return new VectorIterator(this.D, this.I, this.g, this.R, this.iteratorType);
    };
    return VectorIterator;
}(RandomIterator);

export { VectorIterator };

var Vector = function(t) {
    __extends(Vector, t);
    function Vector(r, e) {
        if (r === void 0) {
            r = [];
        }
        if (e === void 0) {
            e = true;
        }
        var n = t.call(this) || this;
        if (Array.isArray(r)) {
            n.V = e ? __spreadArray([], __read(r), false) : r;
            n.o = r.length;
        } else {
            n.V = [];
            r.forEach((function(t) {
                return n.pushBack(t);
            }));
        }
        n.size = n.size.bind(n);
        n.getElementByPos = n.getElementByPos.bind(n);
        n.setElementByPos = n.setElementByPos.bind(n);
        return n;
    }
    Vector.prototype.clear = function() {
        this.o = 0;
        this.V.length = 0;
    };
    Vector.prototype.begin = function() {
        return new VectorIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    };
    Vector.prototype.end = function() {
        return new VectorIterator(this.o, this.size, this.getElementByPos, this.setElementByPos);
    };
    Vector.prototype.rBegin = function() {
        return new VectorIterator(this.o - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Vector.prototype.rEnd = function() {
        return new VectorIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Vector.prototype.front = function() {
        return this.V[0];
    };
    Vector.prototype.back = function() {
        return this.V[this.o - 1];
    };
    Vector.prototype.forEach = function(t) {
        for (var r = 0; r < this.o; ++r) {
            t(this.V[r], r);
        }
    };
    Vector.prototype.getElementByPos = function(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        return this.V[t];
    };
    Vector.prototype.eraseElementByPos = function(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        this.V.splice(t, 1);
        this.o -= 1;
    };
    Vector.prototype.eraseElementByValue = function(t) {
        var r = 0;
        for (var e = 0; e < this.o; ++e) {
            if (this.V[e] !== t) {
                this.V[r++] = this.V[e];
            }
        }
        this.o = this.V.length = r;
    };
    Vector.prototype.eraseElementByIterator = function(t) {
        var r = t.D;
        t = t.next();
        this.eraseElementByPos(r);
        return t;
    };
    Vector.prototype.pushBack = function(t) {
        this.V.push(t);
        this.o += 1;
    };
    Vector.prototype.popBack = function() {
        if (!this.o) return;
        this.V.pop();
        this.o -= 1;
    };
    Vector.prototype.setElementByPos = function(t, r) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        this.V[t] = r;
    };
    Vector.prototype.insert = function(t, r, e) {
        var n;
        if (e === void 0) {
            e = 1;
        }
        if (t < 0 || t > this.o) {
            throw new RangeError;
        }
        (n = this.V).splice.apply(n, __spreadArray([ t, 0 ], __read(new Array(e).fill(r)), false));
        this.o += e;
    };
    Vector.prototype.find = function(t) {
        for (var r = 0; r < this.o; ++r) {
            if (this.V[r] === t) {
                return new VectorIterator(r, this.size, this.getElementByPos, this.getElementByPos);
            }
        }
        return this.end();
    };
    Vector.prototype.reverse = function() {
        this.V.reverse();
    };
    Vector.prototype.unique = function() {
        var t = 1;
        for (var r = 1; r < this.o; ++r) {
            if (this.V[r] !== this.V[r - 1]) {
                this.V[t++] = this.V[r];
            }
        }
        this.o = this.V.length = t;
    };
    Vector.prototype.sort = function(t) {
        this.V.sort(t);
    };
    Vector.prototype[Symbol.iterator] = function() {
        return function() {
            return __generator(this, (function(t) {
                switch (t.label) {
                  case 0:
                    return [ 5, __values(this.V) ];

                  case 1:
                    return [ 2, t.sent() ];
                }
            }));
        }.bind(this)();
    };
    return Vector;
}(SequentialContainer);

export default Vector;