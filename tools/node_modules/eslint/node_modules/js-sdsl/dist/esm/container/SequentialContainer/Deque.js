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

var __generator = this && this.u || function(t, i) {
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

var __read = this && this.P || function(t, i) {
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

var __spreadArray = this && this.A || function(t, i, e) {
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
        return new DequeIterator(this.o, this.D, this.R, this.C, this.iteratorType);
    };
    return DequeIterator;
}(RandomIterator);

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
        r.N = 0;
        r.T = 0;
        r.G = 0;
        r.F = 0;
        r.J = 0;
        r.K = [];
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
        r.L = e;
        r.J = Math.max(Math.ceil(s / r.L), 1);
        for (var h = 0; h < r.J; ++h) {
            r.K.push(new Array(r.L));
        }
        var n = Math.ceil(s / r.L);
        r.N = r.G = (r.J >> 1) - (n >> 1);
        r.T = r.F = r.L - s % r.L >> 1;
        var u = r;
        i.forEach((function(t) {
            u.pushBack(t);
        }));
        r.size = r.size.bind(r);
        r.getElementByPos = r.getElementByPos.bind(r);
        r.setElementByPos = r.setElementByPos.bind(r);
        return r;
    }
    Deque.prototype.U = function() {
        var t = [];
        var i = Math.max(this.J >> 1, 1);
        for (var e = 0; e < i; ++e) {
            t[e] = new Array(this.L);
        }
        for (var e = this.N; e < this.J; ++e) {
            t[t.length] = this.K[e];
        }
        for (var e = 0; e < this.G; ++e) {
            t[t.length] = this.K[e];
        }
        t[t.length] = __spreadArray([], __read(this.K[this.G]), false);
        this.N = i;
        this.G = t.length - 1;
        for (var e = 0; e < i; ++e) {
            t[t.length] = new Array(this.L);
        }
        this.K = t;
        this.J = t.length;
    };
    Deque.prototype.V = function(t) {
        var i = this.T + t + 1;
        var e = i % this.L;
        var r = e - 1;
        var s = this.N + (i - e) / this.L;
        if (e === 0) s -= 1;
        s %= this.J;
        if (r < 0) r += this.L;
        return {
            curNodeBucketIndex: s,
            curNodePointerIndex: r
        };
    };
    Deque.prototype.clear = function() {
        this.K = [ [] ];
        this.J = 1;
        this.N = this.G = this.i = 0;
        this.T = this.F = this.L >> 1;
    };
    Deque.prototype.begin = function() {
        return new DequeIterator(0, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.end = function() {
        return new DequeIterator(this.i, this.size, this.getElementByPos, this.setElementByPos);
    };
    Deque.prototype.rBegin = function() {
        return new DequeIterator(this.i - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Deque.prototype.rEnd = function() {
        return new DequeIterator(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
    };
    Deque.prototype.front = function() {
        return this.K[this.N][this.T];
    };
    Deque.prototype.back = function() {
        return this.K[this.G][this.F];
    };
    Deque.prototype.pushBack = function(t) {
        if (this.i) {
            if (this.F < this.L - 1) {
                this.F += 1;
            } else if (this.G < this.J - 1) {
                this.G += 1;
                this.F = 0;
            } else {
                this.G = 0;
                this.F = 0;
            }
            if (this.G === this.N && this.F === this.T) this.U();
        }
        this.i += 1;
        this.K[this.G][this.F] = t;
        return this.i;
    };
    Deque.prototype.popBack = function() {
        if (this.i === 0) return;
        var t = this.K[this.G][this.F];
        delete this.K[this.G][this.F];
        if (this.i !== 1) {
            if (this.F > 0) {
                this.F -= 1;
            } else if (this.G > 0) {
                this.G -= 1;
                this.F = this.L - 1;
            } else {
                this.G = this.J - 1;
                this.F = this.L - 1;
            }
        }
        this.i -= 1;
        return t;
    };
    Deque.prototype.pushFront = function(t) {
        if (this.i) {
            if (this.T > 0) {
                this.T -= 1;
            } else if (this.N > 0) {
                this.N -= 1;
                this.T = this.L - 1;
            } else {
                this.N = this.J - 1;
                this.T = this.L - 1;
            }
            if (this.N === this.G && this.T === this.F) this.U();
        }
        this.i += 1;
        this.K[this.N][this.T] = t;
        return this.i;
    };
    Deque.prototype.popFront = function() {
        if (this.i === 0) return;
        var t = this.K[this.N][this.T];
        delete this.K[this.N][this.T];
        if (this.i !== 1) {
            if (this.T < this.L - 1) {
                this.T += 1;
            } else if (this.N < this.J - 1) {
                this.N += 1;
                this.T = 0;
            } else {
                this.N = 0;
                this.T = 0;
            }
        }
        this.i -= 1;
        return t;
    };
    Deque.prototype.getElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var i = this.V(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
        return this.K[e][r];
    };
    Deque.prototype.setElementByPos = function(t, i) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var e = this.V(t), r = e.curNodeBucketIndex, s = e.curNodePointerIndex;
        this.K[r][s] = i;
    };
    Deque.prototype.insert = function(t, i, e) {
        if (e === void 0) {
            e = 1;
        }
        if (t < 0 || t > this.i) {
            throw new RangeError;
        }
        if (t === 0) {
            while (e--) this.pushFront(i);
        } else if (t === this.i) {
            while (e--) this.pushBack(i);
        } else {
            var r = [];
            for (var s = t; s < this.i; ++s) {
                r.push(this.getElementByPos(s));
            }
            this.cut(t - 1);
            for (var s = 0; s < e; ++s) this.pushBack(i);
            for (var s = 0; s < r.length; ++s) this.pushBack(r[s]);
        }
        return this.i;
    };
    Deque.prototype.cut = function(t) {
        if (t < 0) {
            this.clear();
            return 0;
        }
        var i = this.V(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
        this.G = e;
        this.F = r;
        this.i = t + 1;
        return this.i;
    };
    Deque.prototype.eraseElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.i - 1) this.popBack(); else {
            var i = [];
            for (var e = t + 1; e < this.i; ++e) {
                i.push(this.getElementByPos(e));
            }
            this.cut(t);
            this.popBack();
            var r = this;
            i.forEach((function(t) {
                r.pushBack(t);
            }));
        }
        return this.i;
    };
    Deque.prototype.eraseElementByValue = function(t) {
        if (this.i === 0) return 0;
        var i = [];
        for (var e = 0; e < this.i; ++e) {
            var r = this.getElementByPos(e);
            if (r !== t) i.push(r);
        }
        var s = i.length;
        for (var e = 0; e < s; ++e) this.setElementByPos(e, i[e]);
        return this.cut(s - 1);
    };
    Deque.prototype.eraseElementByIterator = function(t) {
        var i = t.o;
        this.eraseElementByPos(i);
        t = t.next();
        return t;
    };
    Deque.prototype.find = function(t) {
        for (var i = 0; i < this.i; ++i) {
            if (this.getElementByPos(i) === t) {
                return new DequeIterator(i, this.size, this.getElementByPos, this.setElementByPos);
            }
        }
        return this.end();
    };
    Deque.prototype.reverse = function() {
        var t = 0;
        var i = this.i - 1;
        while (t < i) {
            var e = this.getElementByPos(t);
            this.setElementByPos(t, this.getElementByPos(i));
            this.setElementByPos(i, e);
            t += 1;
            i -= 1;
        }
    };
    Deque.prototype.unique = function() {
        if (this.i <= 1) {
            return this.i;
        }
        var t = 1;
        var i = this.getElementByPos(0);
        for (var e = 1; e < this.i; ++e) {
            var r = this.getElementByPos(e);
            if (r !== i) {
                i = r;
                this.setElementByPos(t++, r);
            }
        }
        while (this.i > t) this.popBack();
        return this.i;
    };
    Deque.prototype.sort = function(t) {
        var i = [];
        for (var e = 0; e < this.i; ++e) {
            i.push(this.getElementByPos(e));
        }
        i.sort(t);
        for (var e = 0; e < this.i; ++e) this.setElementByPos(e, i[e]);
    };
    Deque.prototype.shrinkToFit = function() {
        if (this.i === 0) return;
        var t = [];
        this.forEach((function(i) {
            t.push(i);
        }));
        this.J = Math.max(Math.ceil(this.i / this.L), 1);
        this.i = this.N = this.G = this.T = this.F = 0;
        this.K = [];
        for (var i = 0; i < this.J; ++i) {
            this.K.push(new Array(this.L));
        }
        for (var i = 0; i < t.length; ++i) this.pushBack(t[i]);
    };
    Deque.prototype.forEach = function(t) {
        for (var i = 0; i < this.i; ++i) {
            t(this.getElementByPos(i), i, this);
        }
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
                    if (!(t < this.i)) return [ 3, 4 ];
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
//# sourceMappingURL=Deque.js.map
