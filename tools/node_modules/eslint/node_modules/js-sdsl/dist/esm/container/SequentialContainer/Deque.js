var __extends = this && this.t || function() {
    var extendStatics = function(t, i) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, i) {
            t.__proto__ = i;
        } || function(t, i) {
            for (var e in i) if (Object.prototype.hasOwnProperty.call(i, e)) t[e] = i[e];
        };
        return extendStatics(t, i);
    };
    return function(t, i) {
        if (typeof i !== "function" && i !== null) throw new TypeError("Class extends value " + String(i) + " is not a constructor or null");
        extendStatics(t, i);
        function __() {
            this.constructor = t;
        }
        t.prototype = i === null ? Object.create(i) : (__.prototype = i.prototype, new __);
    };
}();

var __generator = this && this.i || function(t, i) {
    var e = {
        label: 0,
        sent: function() {
            if (h[0] & 1) throw h[1];
            return h[1];
        },
        trys: [],
        ops: []
    }, r, s, h, n;
    return n = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (n[Symbol.iterator] = function() {
        return this;
    }), n;
    function verb(t) {
        return function(i) {
            return step([ t, i ]);
        };
    }
    function step(n) {
        if (r) throw new TypeError("Generator is already executing.");
        while (e) try {
            if (r = 1, s && (h = n[0] & 2 ? s["return"] : n[0] ? s["throw"] || ((h = s["return"]) && h.call(s), 
            0) : s.next) && !(h = h.call(s, n[1])).done) return h;
            if (s = 0, h) n = [ n[0] & 2, h.value ];
            switch (n[0]) {
              case 0:
              case 1:
                h = n;
                break;

              case 4:
                e.label++;
                return {
                    value: n[1],
                    done: false
                };

              case 5:
                e.label++;
                s = n[1];
                n = [ 0 ];
                continue;

              case 7:
                n = e.ops.pop();
                e.trys.pop();
                continue;

              default:
                if (!(h = e.trys, h = h.length > 0 && h[h.length - 1]) && (n[0] === 6 || n[0] === 2)) {
                    e = 0;
                    continue;
                }
                if (n[0] === 3 && (!h || n[1] > h[0] && n[1] < h[3])) {
                    e.label = n[1];
                    break;
                }
                if (n[0] === 6 && e.label < h[1]) {
                    e.label = h[1];
                    h = n;
                    break;
                }
                if (h && e.label < h[2]) {
                    e.label = h[2];
                    e.ops.push(n);
                    break;
                }
                if (h[2]) e.ops.pop();
                e.trys.pop();
                continue;
            }
            n = i.call(t, e);
        } catch (t) {
            n = [ 6, t ];
            s = 0;
        } finally {
            r = h = 0;
        }
        if (n[0] & 5) throw n[1];
        return {
            value: n[0] ? n[1] : void 0,
            done: true
        };
    }
};

var __read = this && this._ || function(t, i) {
    var e = typeof Symbol === "function" && t[Symbol.iterator];
    if (!e) return t;
    var r = e.call(t), s, h = [], n;
    try {
        while ((i === void 0 || i-- > 0) && !(s = r.next()).done) h.push(s.value);
    } catch (t) {
        n = {
            error: t
        };
    } finally {
        try {
            if (s && !s.done && (e = r["return"])) e.call(r);
        } finally {
            if (n) throw n.error;
        }
    }
    return h;
};

var __spreadArray = this && this.P || function(t, i, e) {
    if (e || arguments.length === 2) for (var r = 0, s = i.length, h; r < s; r++) {
        if (h || !(r in i)) {
            if (!h) h = Array.prototype.slice.call(i, 0, r);
            h[r] = i[r];
        }
    }
    return t.concat(h || Array.prototype.slice.call(i));
};

import SequentialContainer from "./Base";

import { RandomIterator } from "./Base/RandomIterator";

var DequeIterator = function(t) {
    __extends(DequeIterator, t);
    function DequeIterator() {
        return t !== null && t.apply(this, arguments) || this;
    }
    DequeIterator.prototype.copy = function() {
        return new DequeIterator(this.D, this.I, this.g, this.R, this.iteratorType);
    };
    return DequeIterator;
}(RandomIterator);

export { DequeIterator };

var Deque = function(t) {
    __extends(Deque, t);
    function Deque(i, e) {
        if (i === void 0) {
            i = [];
        }
        if (e === void 0) {
            e = 1 << 12;
        }
        var r = t.call(this) || this;
        r.M = 0;
        r.k = 0;
        r.C = 0;
        r.O = 0;
        r.l = 0;
        r.N = [];
        var s;
        if ("size" in i) {
            if (typeof i.size === "number") {
                s = i.size;
            } else {
                s = i.size();
            }
        } else if ("length" in i) {
            s = i.length;
        } else {
            throw new RangeError("Can't get container's size!");
        }
        r.T = e;
        r.l = Math.max(Math.ceil(s / r.T), 1);
        for (var h = 0; h < r.l; ++h) {
            r.N.push(new Array(r.T));
        }
        var n = Math.ceil(s / r.T);
        r.M = r.C = (r.l >> 1) - (n >> 1);
        r.k = r.O = r.T - s % r.T >> 1;
        i.forEach((function(t) {
            return r.pushBack(t);
        }));
        r.size = r.size.bind(r);
        r.getElementByPos = r.getElementByPos.bind(r);
        r.setElementByPos = r.setElementByPos.bind(r);
        return r;
    }
    Deque.prototype.v = function() {
        var t = [];
        var i = Math.max(this.l >> 1, 1);
        for (var e = 0; e < i; ++e) {
            t[e] = new Array(this.T);
        }
        for (var e = this.M; e < this.l; ++e) {
            t[t.length] = this.N[e];
        }
        for (var e = 0; e < this.C; ++e) {
            t[t.length] = this.N[e];
        }
        t[t.length] = __spreadArray([], __read(this.N[this.C]), false);
        this.M = i;
        this.C = t.length - 1;
        for (var e = 0; e < i; ++e) {
            t[t.length] = new Array(this.T);
        }
        this.N = t;
        this.l = t.length;
    };
    Deque.prototype.G = function(t) {
        var i = this.k + t + 1;
        var e = i % this.T;
        var r = e - 1;
        var s = this.M + (i - e) / this.T;
        if (e === 0) s -= 1;
        s %= this.l;
        if (r < 0) r += this.T;
        return {
            curNodeBucketIndex: s,
            curNodePointerIndex: r
        };
    };
    Deque.prototype.clear = function() {
        this.N = [ [] ];
        this.l = 1;
        this.M = this.C = this.o = 0;
        this.k = this.O = this.T >> 1;
    };
    Deque.prototype.front = function() {
        return this.N[this.M][this.k];
    };
    Deque.prototype.back = function() {
        return this.N[this.C][this.O];
    };
    Deque.prototype.begin = function() {
        return new DequeIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.end = function() {
        return new DequeIterator(this.o, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.rBegin = function() {
        return new DequeIterator(this.o - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Deque.prototype.rEnd = function() {
        return new DequeIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Deque.prototype.pushBack = function(t) {
        if (this.o) {
            if (this.O < this.T - 1) {
                this.O += 1;
            } else if (this.C < this.l - 1) {
                this.C += 1;
                this.O = 0;
            } else {
                this.C = 0;
                this.O = 0;
            }
            if (this.C === this.M && this.O === this.k) this.v();
        }
        this.o += 1;
        this.N[this.C][this.O] = t;
    };
    Deque.prototype.popBack = function() {
        if (!this.o) return;
        this.N[this.C][this.O] = undefined;
        if (this.o !== 1) {
            if (this.O > 0) {
                this.O -= 1;
            } else if (this.C > 0) {
                this.C -= 1;
                this.O = this.T - 1;
            } else {
                this.C = this.l - 1;
                this.O = this.T - 1;
            }
        }
        this.o -= 1;
    };
    Deque.prototype.pushFront = function(t) {
        if (this.o) {
            if (this.k > 0) {
                this.k -= 1;
            } else if (this.M > 0) {
                this.M -= 1;
                this.k = this.T - 1;
            } else {
                this.M = this.l - 1;
                this.k = this.T - 1;
            }
            if (this.M === this.C && this.k === this.O) this.v();
        }
        this.o += 1;
        this.N[this.M][this.k] = t;
    };
    Deque.prototype.popFront = function() {
        if (!this.o) return;
        this.N[this.M][this.k] = undefined;
        if (this.o !== 1) {
            if (this.k < this.T - 1) {
                this.k += 1;
            } else if (this.M < this.l - 1) {
                this.M += 1;
                this.k = 0;
            } else {
                this.M = 0;
                this.k = 0;
            }
        }
        this.o -= 1;
    };
    Deque.prototype.forEach = function(t) {
        for (var i = 0; i < this.o; ++i) {
            t(this.getElementByPos(i), i);
        }
    };
    Deque.prototype.getElementByPos = function(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        var i = this.G(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
        return this.N[e][r];
    };
    Deque.prototype.setElementByPos = function(t, i) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        var e = this.G(t), r = e.curNodeBucketIndex, s = e.curNodePointerIndex;
        this.N[r][s] = i;
    };
    Deque.prototype.insert = function(t, i, e) {
        if (e === void 0) {
            e = 1;
        }
        if (t < 0 || t > this.o) {
            throw new RangeError;
        }
        if (t === 0) {
            while (e--) this.pushFront(i);
        } else if (t === this.o) {
            while (e--) this.pushBack(i);
        } else {
            var r = [];
            for (var s = t; s < this.o; ++s) {
                r.push(this.getElementByPos(s));
            }
            this.cut(t - 1);
            for (var s = 0; s < e; ++s) this.pushBack(i);
            for (var s = 0; s < r.length; ++s) this.pushBack(r[s]);
        }
    };
    Deque.prototype.cut = function(t) {
        if (t < 0) {
            this.clear();
            return;
        }
        var i = this.G(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
        this.C = e;
        this.O = r;
        this.o = t + 1;
    };
    Deque.prototype.eraseElementByPos = function(t) {
        var i = this;
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.o - 1) this.popBack(); else {
            var e = [];
            for (var r = t + 1; r < this.o; ++r) {
                e.push(this.getElementByPos(r));
            }
            this.cut(t);
            this.popBack();
            e.forEach((function(t) {
                return i.pushBack(t);
            }));
        }
    };
    Deque.prototype.eraseElementByValue = function(t) {
        if (!this.o) return;
        var i = [];
        for (var e = 0; e < this.o; ++e) {
            var r = this.getElementByPos(e);
            if (r !== t) i.push(r);
        }
        var s = i.length;
        for (var e = 0; e < s; ++e) this.setElementByPos(e, i[e]);
        this.cut(s - 1);
    };
    Deque.prototype.eraseElementByIterator = function(t) {
        var i = t.D;
        this.eraseElementByPos(i);
        t = t.next();
        return t;
    };
    Deque.prototype.find = function(t) {
        for (var i = 0; i < this.o; ++i) {
            if (this.getElementByPos(i) === t) {
                return new DequeIterator(i, this.size, this.getElementByPos, this.setElementByPos);
            }
        }
        return this.end();
    };
    Deque.prototype.reverse = function() {
        var t = 0;
        var i = this.o - 1;
        while (t < i) {
            var e = this.getElementByPos(t);
            this.setElementByPos(t, this.getElementByPos(i));
            this.setElementByPos(i, e);
            t += 1;
            i -= 1;
        }
    };
    Deque.prototype.unique = function() {
        if (this.o <= 1) return;
        var t = 1;
        var i = this.getElementByPos(0);
        for (var e = 1; e < this.o; ++e) {
            var r = this.getElementByPos(e);
            if (r !== i) {
                i = r;
                this.setElementByPos(t++, r);
            }
        }
        while (this.o > t) this.popBack();
    };
    Deque.prototype.sort = function(t) {
        var i = [];
        for (var e = 0; e < this.o; ++e) {
            i.push(this.getElementByPos(e));
        }
        i.sort(t);
        for (var e = 0; e < this.o; ++e) this.setElementByPos(e, i[e]);
    };
    Deque.prototype.shrinkToFit = function() {
        if (!this.o) return;
        var t = [];
        this.forEach((function(i) {
            return t.push(i);
        }));
        this.l = Math.max(Math.ceil(this.o / this.T), 1);
        this.o = this.M = this.C = this.k = this.O = 0;
        this.N = [];
        for (var i = 0; i < this.l; ++i) {
            this.N.push(new Array(this.T));
        }
        for (var i = 0; i < t.length; ++i) this.pushBack(t[i]);
    };
    Deque.prototype[Symbol.iterator] = function() {
        return function() {
            var t;
            return __generator(this, (function(i) {
                switch (i.label) {
                  case 0:
                    t = 0;
                    i.label = 1;

                  case 1:
                    if (!(t < this.o)) return [ 3, 4 ];
                    return [ 4, this.getElementByPos(t) ];

                  case 2:
                    i.sent();
                    i.label = 3;

                  case 3:
                    ++t;
                    return [ 3, 1 ];

                  case 4:
                    return [ 2 ];
                }
            }));
        }.bind(this)();
    };
    return Deque;
}(SequentialContainer);

export default Deque;