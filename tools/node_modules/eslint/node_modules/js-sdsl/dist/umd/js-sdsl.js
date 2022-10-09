(function(t, i) {
    typeof exports === "object" && typeof module !== "undefined" ? i(exports) : typeof define === "function" && define.amd ? define("sdsl", [ "exports" ], i) : (t = typeof globalThis !== "undefined" ? globalThis : t || self, 
    i(t.sdsl = {}));
})(this, (function(t) {
    "use strict";
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
    function __extends(t, i) {
        if (typeof i !== "function" && i !== null) throw new TypeError("Class extends value " + String(i) + " is not a constructor or null");
        extendStatics(t, i);
        function __() {
            this.constructor = t;
        }
        t.prototype = i === null ? Object.create(i) : (__.prototype = i.prototype, new __);
    }
    function __generator(t, i) {
        var e = {
            label: 0,
            sent: function() {
                if (s[0] & 1) throw s[1];
                return s[1];
            },
            trys: [],
            ops: []
        }, r, n, s, h;
        return h = {
            next: verb(0),
            throw: verb(1),
            return: verb(2)
        }, typeof Symbol === "function" && (h[Symbol.iterator] = function() {
            return this;
        }), h;
        function verb(t) {
            return function(i) {
                return step([ t, i ]);
            };
        }
        function step(h) {
            if (r) throw new TypeError("Generator is already executing.");
            while (e) try {
                if (r = 1, n && (s = h[0] & 2 ? n["return"] : h[0] ? n["throw"] || ((s = n["return"]) && s.call(n), 
                0) : n.next) && !(s = s.call(n, h[1])).done) return s;
                if (n = 0, s) h = [ h[0] & 2, s.value ];
                switch (h[0]) {
                  case 0:
                  case 1:
                    s = h;
                    break;

                  case 4:
                    e.label++;
                    return {
                        value: h[1],
                        done: false
                    };

                  case 5:
                    e.label++;
                    n = h[1];
                    h = [ 0 ];
                    continue;

                  case 7:
                    h = e.ops.pop();
                    e.trys.pop();
                    continue;

                  default:
                    if (!(s = e.trys, s = s.length > 0 && s[s.length - 1]) && (h[0] === 6 || h[0] === 2)) {
                        e = 0;
                        continue;
                    }
                    if (h[0] === 3 && (!s || h[1] > s[0] && h[1] < s[3])) {
                        e.label = h[1];
                        break;
                    }
                    if (h[0] === 6 && e.label < s[1]) {
                        e.label = s[1];
                        s = h;
                        break;
                    }
                    if (s && e.label < s[2]) {
                        e.label = s[2];
                        e.ops.push(h);
                        break;
                    }
                    if (s[2]) e.ops.pop();
                    e.trys.pop();
                    continue;
                }
                h = i.call(t, e);
            } catch (t) {
                h = [ 6, t ];
                n = 0;
            } finally {
                r = s = 0;
            }
            if (h[0] & 5) throw h[1];
            return {
                value: h[0] ? h[1] : void 0,
                done: true
            };
        }
    }
    function __values(t) {
        var i = typeof Symbol === "function" && Symbol.iterator, e = i && t[i], r = 0;
        if (e) return e.call(t);
        if (t && typeof t.length === "number") return {
            next: function() {
                if (t && r >= t.length) t = void 0;
                return {
                    value: t && t[r++],
                    done: !t
                };
            }
        };
        throw new TypeError(i ? "Object is not iterable." : "Symbol.iterator is not defined.");
    }
    function __read(t, i) {
        var e = typeof Symbol === "function" && t[Symbol.iterator];
        if (!e) return t;
        var r = e.call(t), n, s = [], h;
        try {
            while ((i === void 0 || i-- > 0) && !(n = r.next()).done) s.push(n.value);
        } catch (t) {
            h = {
                error: t
            };
        } finally {
            try {
                if (n && !n.done && (e = r["return"])) e.call(r);
            } finally {
                if (h) throw h.error;
            }
        }
        return s;
    }
    function __spreadArray(t, i, e) {
        if (e || arguments.length === 2) for (var r = 0, n = i.length, s; r < n; r++) {
            if (s || !(r in i)) {
                if (!s) s = Array.prototype.slice.call(i, 0, r);
                s[r] = i[r];
            }
        }
        return t.concat(s || Array.prototype.slice.call(i));
    }
    var i = function() {
        function ContainerIterator(t) {
            if (t === void 0) {
                t = 0;
            }
            this.iteratorType = t;
        }
        return ContainerIterator;
    }();
    var e = function() {
        function Base() {
            this.t = 0;
        }
        Base.prototype.size = function() {
            return this.t;
        };
        Base.prototype.empty = function() {
            return this.t === 0;
        };
        return Base;
    }();
    var r = function(t) {
        __extends(Container, t);
        function Container() {
            return t !== null && t.apply(this, arguments) || this;
        }
        return Container;
    }(e);
    var n = function(t) {
        __extends(Stack, t);
        function Stack(i) {
            if (i === void 0) {
                i = [];
            }
            var e = t.call(this) || this;
            e.i = [];
            i.forEach((function(t) {
                return e.push(t);
            }));
            return e;
        }
        Stack.prototype.clear = function() {
            this.t = 0;
            this.i.length = 0;
        };
        Stack.prototype.push = function(t) {
            this.i.push(t);
            this.t += 1;
        };
        Stack.prototype.pop = function() {
            this.i.pop();
            if (this.t > 0) this.t -= 1;
        };
        Stack.prototype.top = function() {
            return this.i[this.t - 1];
        };
        return Stack;
    }(e);
    var s = function(t) {
        __extends(SequentialContainer, t);
        function SequentialContainer() {
            return t !== null && t.apply(this, arguments) || this;
        }
        return SequentialContainer;
    }(r);
    var h = function(t) {
        __extends(RandomIterator, t);
        function RandomIterator(i, e, r, n, s) {
            var h = t.call(this, s) || this;
            h.h = i;
            h.u = e;
            h.o = r;
            h.v = n;
            if (h.iteratorType === 0) {
                h.pre = function() {
                    if (this.h === 0) {
                        throw new RangeError("Random iterator access denied!");
                    }
                    this.h -= 1;
                    return this;
                };
                h.next = function() {
                    if (this.h === this.u()) {
                        throw new RangeError("Random Iterator access denied!");
                    }
                    this.h += 1;
                    return this;
                };
            } else {
                h.pre = function() {
                    if (this.h === this.u() - 1) {
                        throw new RangeError("Random iterator access denied!");
                    }
                    this.h += 1;
                    return this;
                };
                h.next = function() {
                    if (this.h === -1) {
                        throw new RangeError("Random iterator access denied!");
                    }
                    this.h -= 1;
                    return this;
                };
            }
            return h;
        }
        Object.defineProperty(RandomIterator.prototype, "pointer", {
            get: function() {
                return this.o(this.h);
            },
            set: function(t) {
                this.v(this.h, t);
            },
            enumerable: false,
            configurable: true
        });
        RandomIterator.prototype.equals = function(t) {
            return this.h === t.h;
        };
        return RandomIterator;
    }(i);
    var u = function(t) {
        __extends(DequeIterator, t);
        function DequeIterator() {
            return t !== null && t.apply(this, arguments) || this;
        }
        DequeIterator.prototype.copy = function() {
            return new DequeIterator(this.h, this.u, this.o, this.v, this.iteratorType);
        };
        return DequeIterator;
    }(h);
    var f = function(t) {
        __extends(Deque, t);
        function Deque(i, e) {
            if (i === void 0) {
                i = [];
            }
            if (e === void 0) {
                e = 1 << 12;
            }
            var r = t.call(this) || this;
            r.l = 0;
            r._ = 0;
            r.L = 0;
            r.p = 0;
            r.O = 0;
            r.k = [];
            var n;
            if ("size" in i) {
                if (typeof i.size === "number") {
                    n = i.size;
                } else {
                    n = i.size();
                }
            } else if ("length" in i) {
                n = i.length;
            } else {
                throw new RangeError("Can't get container's size!");
            }
            r.S = e;
            r.O = Math.max(Math.ceil(n / r.S), 1);
            for (var s = 0; s < r.O; ++s) {
                r.k.push(new Array(r.S));
            }
            var h = Math.ceil(n / r.S);
            r.l = r.L = (r.O >> 1) - (h >> 1);
            r._ = r.p = r.S - n % r.S >> 1;
            i.forEach((function(t) {
                return r.pushBack(t);
            }));
            r.size = r.size.bind(r);
            r.getElementByPos = r.getElementByPos.bind(r);
            r.setElementByPos = r.setElementByPos.bind(r);
            return r;
        }
        Deque.prototype.I = function() {
            var t = [];
            var i = Math.max(this.O >> 1, 1);
            for (var e = 0; e < i; ++e) {
                t[e] = new Array(this.S);
            }
            for (var e = this.l; e < this.O; ++e) {
                t[t.length] = this.k[e];
            }
            for (var e = 0; e < this.L; ++e) {
                t[t.length] = this.k[e];
            }
            t[t.length] = __spreadArray([], __read(this.k[this.L]), false);
            this.l = i;
            this.L = t.length - 1;
            for (var e = 0; e < i; ++e) {
                t[t.length] = new Array(this.S);
            }
            this.k = t;
            this.O = t.length;
        };
        Deque.prototype.g = function(t) {
            var i = this._ + t + 1;
            var e = i % this.S;
            var r = e - 1;
            var n = this.l + (i - e) / this.S;
            if (e === 0) n -= 1;
            n %= this.O;
            if (r < 0) r += this.S;
            return {
                curNodeBucketIndex: n,
                curNodePointerIndex: r
            };
        };
        Deque.prototype.clear = function() {
            this.k = [ [] ];
            this.O = 1;
            this.l = this.L = this.t = 0;
            this._ = this.p = this.S >> 1;
        };
        Deque.prototype.front = function() {
            return this.k[this.l][this._];
        };
        Deque.prototype.back = function() {
            return this.k[this.L][this.p];
        };
        Deque.prototype.begin = function() {
            return new u(0, this.size, this.getElementByPos, this.setElementByPos);
        };
        Deque.prototype.end = function() {
            return new u(this.t, this.size, this.getElementByPos, this.setElementByPos);
        };
        Deque.prototype.rBegin = function() {
            return new u(this.t - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
        };
        Deque.prototype.rEnd = function() {
            return new u(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
        };
        Deque.prototype.pushBack = function(t) {
            if (this.t) {
                if (this.p < this.S - 1) {
                    this.p += 1;
                } else if (this.L < this.O - 1) {
                    this.L += 1;
                    this.p = 0;
                } else {
                    this.L = 0;
                    this.p = 0;
                }
                if (this.L === this.l && this.p === this._) this.I();
            }
            this.t += 1;
            this.k[this.L][this.p] = t;
        };
        Deque.prototype.popBack = function() {
            if (!this.t) return;
            this.k[this.L][this.p] = undefined;
            if (this.t !== 1) {
                if (this.p > 0) {
                    this.p -= 1;
                } else if (this.L > 0) {
                    this.L -= 1;
                    this.p = this.S - 1;
                } else {
                    this.L = this.O - 1;
                    this.p = this.S - 1;
                }
            }
            this.t -= 1;
        };
        Deque.prototype.pushFront = function(t) {
            if (this.t) {
                if (this._ > 0) {
                    this._ -= 1;
                } else if (this.l > 0) {
                    this.l -= 1;
                    this._ = this.S - 1;
                } else {
                    this.l = this.O - 1;
                    this._ = this.S - 1;
                }
                if (this.l === this.L && this._ === this.p) this.I();
            }
            this.t += 1;
            this.k[this.l][this._] = t;
        };
        Deque.prototype.popFront = function() {
            if (!this.t) return;
            this.k[this.l][this._] = undefined;
            if (this.t !== 1) {
                if (this._ < this.S - 1) {
                    this._ += 1;
                } else if (this.l < this.O - 1) {
                    this.l += 1;
                    this._ = 0;
                } else {
                    this.l = 0;
                    this._ = 0;
                }
            }
            this.t -= 1;
        };
        Deque.prototype.forEach = function(t) {
            for (var i = 0; i < this.t; ++i) {
                t(this.getElementByPos(i), i);
            }
        };
        Deque.prototype.getElementByPos = function(t) {
            var i = this.g(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
            return this.k[e][r];
        };
        Deque.prototype.setElementByPos = function(t, i) {
            var e = this.g(t), r = e.curNodeBucketIndex, n = e.curNodePointerIndex;
            this.k[r][n] = i;
        };
        Deque.prototype.insert = function(t, i, e) {
            if (e === void 0) {
                e = 1;
            }
            if (t === 0) {
                while (e--) this.pushFront(i);
            } else if (t === this.t) {
                while (e--) this.pushBack(i);
            } else {
                var r = [];
                for (var n = t; n < this.t; ++n) {
                    r.push(this.getElementByPos(n));
                }
                this.cut(t - 1);
                for (var n = 0; n < e; ++n) this.pushBack(i);
                for (var n = 0; n < r.length; ++n) this.pushBack(r[n]);
            }
        };
        Deque.prototype.cut = function(t) {
            if (t < 0) {
                this.clear();
                return;
            }
            var i = this.g(t), e = i.curNodeBucketIndex, r = i.curNodePointerIndex;
            this.L = e;
            this.p = r;
            this.t = t + 1;
        };
        Deque.prototype.eraseElementByPos = function(t) {
            var i = this;
            if (t === 0) this.popFront(); else if (t === this.t - 1) this.popBack(); else {
                var e = [];
                for (var r = t + 1; r < this.t; ++r) {
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
            if (!this.t) return;
            var i = [];
            for (var e = 0; e < this.t; ++e) {
                var r = this.getElementByPos(e);
                if (r !== t) i.push(r);
            }
            var n = i.length;
            for (var e = 0; e < n; ++e) this.setElementByPos(e, i[e]);
            this.cut(n - 1);
        };
        Deque.prototype.eraseElementByIterator = function(t) {
            var i = t.h;
            this.eraseElementByPos(i);
            t = t.next();
            return t;
        };
        Deque.prototype.find = function(t) {
            for (var i = 0; i < this.t; ++i) {
                if (this.getElementByPos(i) === t) {
                    return new u(i, this.size, this.getElementByPos, this.setElementByPos);
                }
            }
            return this.end();
        };
        Deque.prototype.reverse = function() {
            var t = 0;
            var i = this.t - 1;
            while (t < i) {
                var e = this.getElementByPos(t);
                this.setElementByPos(t, this.getElementByPos(i));
                this.setElementByPos(i, e);
                t += 1;
                i -= 1;
            }
        };
        Deque.prototype.unique = function() {
            if (this.t <= 1) return;
            var t = 1;
            var i = this.getElementByPos(0);
            for (var e = 1; e < this.t; ++e) {
                var r = this.getElementByPos(e);
                if (r !== i) {
                    i = r;
                    this.setElementByPos(t++, r);
                }
            }
            while (this.t > t) this.popBack();
        };
        Deque.prototype.sort = function(t) {
            var i = [];
            for (var e = 0; e < this.t; ++e) {
                i.push(this.getElementByPos(e));
            }
            i.sort(t);
            for (var e = 0; e < this.t; ++e) this.setElementByPos(e, i[e]);
        };
        Deque.prototype.shrinkToFit = function() {
            if (!this.t) return;
            var t = [];
            this.forEach((function(i) {
                return t.push(i);
            }));
            this.O = Math.max(Math.ceil(this.t / this.S), 1);
            this.t = this.l = this.L = this._ = this.p = 0;
            this.k = [];
            for (var i = 0; i < this.O; ++i) {
                this.k.push(new Array(this.S));
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
                        if (!(t < this.t)) return [ 3, 4 ];
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
    }(s);
    var a = function(t) {
        __extends(Queue, t);
        function Queue(i) {
            if (i === void 0) {
                i = [];
            }
            var e = t.call(this) || this;
            e.T = new f(i);
            e.t = e.T.size();
            return e;
        }
        Queue.prototype.clear = function() {
            this.T.clear();
            this.t = 0;
        };
        Queue.prototype.push = function(t) {
            this.T.pushBack(t);
            this.t += 1;
        };
        Queue.prototype.pop = function() {
            this.T.popFront();
            if (this.t) this.t -= 1;
        };
        Queue.prototype.front = function() {
            return this.T.front();
        };
        return Queue;
    }(e);
    var o = function(t) {
        __extends(PriorityQueue, t);
        function PriorityQueue(i, e, r) {
            if (i === void 0) {
                i = [];
            }
            if (e === void 0) {
                e = function(t, i) {
                    if (t > i) return -1;
                    if (t < i) return 1;
                    return 0;
                };
            }
            if (r === void 0) {
                r = true;
            }
            var n = t.call(this) || this;
            n.M = e;
            if (Array.isArray(i)) {
                n.q = r ? __spreadArray([], __read(i), false) : i;
            } else {
                n.q = [];
                i.forEach((function(t) {
                    return n.q.push(t);
                }));
            }
            n.t = n.q.length;
            var s = n.t >> 1;
            for (var h = n.t - 1 >> 1; h >= 0; --h) {
                n.D(h, s);
            }
            return n;
        }
        PriorityQueue.prototype.m = function(t) {
            var i = this.q[t];
            while (t > 0) {
                var e = t - 1 >> 1;
                var r = this.q[e];
                if (this.M(r, i) <= 0) break;
                this.q[t] = r;
                t = e;
            }
            this.q[t] = i;
        };
        PriorityQueue.prototype.D = function(t, i) {
            var e = this.q[t];
            while (t < i) {
                var r = t << 1 | 1;
                var n = r + 1;
                var s = this.q[r];
                if (n < this.t && this.M(s, this.q[n]) > 0) {
                    r = n;
                    s = this.q[n];
                }
                if (this.M(s, e) >= 0) break;
                this.q[t] = s;
                t = r;
            }
            this.q[t] = e;
        };
        PriorityQueue.prototype.clear = function() {
            this.t = 0;
            this.q.length = 0;
        };
        PriorityQueue.prototype.push = function(t) {
            this.q.push(t);
            this.m(this.t);
            this.t += 1;
        };
        PriorityQueue.prototype.pop = function() {
            if (!this.t) return;
            var t = this.q.pop();
            this.t -= 1;
            if (this.t) {
                this.q[0] = t;
                this.D(0, this.t >> 1);
            }
        };
        PriorityQueue.prototype.top = function() {
            return this.q[0];
        };
        PriorityQueue.prototype.find = function(t) {
            return this.q.indexOf(t) >= 0;
        };
        PriorityQueue.prototype.remove = function(t) {
            var i = this.q.indexOf(t);
            if (i < 0) return false;
            if (i === 0) {
                this.pop();
            } else if (i === this.t - 1) {
                this.q.pop();
                this.t -= 1;
            } else {
                this.q.splice(i, 1, this.q.pop());
                this.t -= 1;
                this.m(i);
                this.D(i, this.t >> 1);
            }
            return true;
        };
        PriorityQueue.prototype.updateItem = function(t) {
            var i = this.q.indexOf(t);
            if (i < 0) return false;
            this.m(i);
            this.D(i, this.t >> 1);
            return true;
        };
        PriorityQueue.prototype.toArray = function() {
            return __spreadArray([], __read(this.q), false);
        };
        return PriorityQueue;
    }(e);
    var c = function(t) {
        __extends(VectorIterator, t);
        function VectorIterator() {
            return t !== null && t.apply(this, arguments) || this;
        }
        VectorIterator.prototype.copy = function() {
            return new VectorIterator(this.h, this.u, this.o, this.v, this.iteratorType);
        };
        return VectorIterator;
    }(h);
    var v = function(t) {
        __extends(Vector, t);
        function Vector(i, e) {
            if (i === void 0) {
                i = [];
            }
            if (e === void 0) {
                e = true;
            }
            var r = t.call(this) || this;
            if (Array.isArray(i)) {
                r.C = e ? __spreadArray([], __read(i), false) : i;
                r.t = i.length;
            } else {
                r.C = [];
                i.forEach((function(t) {
                    return r.pushBack(t);
                }));
            }
            r.size = r.size.bind(r);
            r.getElementByPos = r.getElementByPos.bind(r);
            r.setElementByPos = r.setElementByPos.bind(r);
            return r;
        }
        Vector.prototype.clear = function() {
            this.t = 0;
            this.C.length = 0;
        };
        Vector.prototype.begin = function() {
            return new c(0, this.size, this.getElementByPos, this.setElementByPos);
        };
        Vector.prototype.end = function() {
            return new c(this.t, this.size, this.getElementByPos, this.setElementByPos);
        };
        Vector.prototype.rBegin = function() {
            return new c(this.t - 1, this.size, this.getElementByPos, this.setElementByPos, 1);
        };
        Vector.prototype.rEnd = function() {
            return new c(-1, this.size, this.getElementByPos, this.setElementByPos, 1);
        };
        Vector.prototype.front = function() {
            return this.C[0];
        };
        Vector.prototype.back = function() {
            return this.C[this.t - 1];
        };
        Vector.prototype.forEach = function(t) {
            for (var i = 0; i < this.t; ++i) {
                t(this.C[i], i);
            }
        };
        Vector.prototype.getElementByPos = function(t) {
            return this.C[t];
        };
        Vector.prototype.eraseElementByPos = function(t) {
            this.C.splice(t, 1);
            this.t -= 1;
        };
        Vector.prototype.eraseElementByValue = function(t) {
            var i = 0;
            for (var e = 0; e < this.t; ++e) {
                if (this.C[e] !== t) {
                    this.C[i++] = this.C[e];
                }
            }
            this.t = this.C.length = i;
        };
        Vector.prototype.eraseElementByIterator = function(t) {
            var i = t.h;
            t = t.next();
            this.eraseElementByPos(i);
            return t;
        };
        Vector.prototype.pushBack = function(t) {
            this.C.push(t);
            this.t += 1;
        };
        Vector.prototype.popBack = function() {
            if (!this.t) return;
            this.C.pop();
            this.t -= 1;
        };
        Vector.prototype.setElementByPos = function(t, i) {
            this.C[t] = i;
        };
        Vector.prototype.insert = function(t, i, e) {
            var r;
            if (e === void 0) {
                e = 1;
            }
            (r = this.C).splice.apply(r, __spreadArray([ t, 0 ], __read(new Array(e).fill(i)), false));
            this.t += e;
        };
        Vector.prototype.find = function(t) {
            for (var i = 0; i < this.t; ++i) {
                if (this.C[i] === t) {
                    return new c(i, this.size, this.getElementByPos, this.getElementByPos);
                }
            }
            return this.end();
        };
        Vector.prototype.reverse = function() {
            this.C.reverse();
        };
        Vector.prototype.unique = function() {
            var t = 1;
            for (var i = 1; i < this.t; ++i) {
                if (this.C[i] !== this.C[i - 1]) {
                    this.C[t++] = this.C[i];
                }
            }
            this.t = this.C.length = t;
        };
        Vector.prototype.sort = function(t) {
            this.C.sort(t);
        };
        Vector.prototype[Symbol.iterator] = function() {
            return function() {
                return __generator(this, (function(t) {
                    switch (t.label) {
                      case 0:
                        return [ 5, __values(this.C) ];

                      case 1:
                        return [ 2, t.sent() ];
                    }
                }));
            }.bind(this)();
        };
        return Vector;
    }(s);
    var d = function() {
        function LinkNode(t) {
            this.R = undefined;
            this.V = undefined;
            this.H = undefined;
            this.R = t;
        }
        return LinkNode;
    }();
    var l = function(t) {
        __extends(LinkListIterator, t);
        function LinkListIterator(i, e, r) {
            var n = t.call(this, r) || this;
            n.h = i;
            n.N = e;
            if (n.iteratorType === 0) {
                n.pre = function() {
                    if (this.h.V === this.N) {
                        throw new RangeError("LinkList iterator access denied!");
                    }
                    this.h = this.h.V;
                    return this;
                };
                n.next = function() {
                    if (this.h === this.N) {
                        throw new RangeError("LinkList iterator access denied!");
                    }
                    this.h = this.h.H;
                    return this;
                };
            } else {
                n.pre = function() {
                    if (this.h.H === this.N) {
                        throw new RangeError("LinkList iterator access denied!");
                    }
                    this.h = this.h.H;
                    return this;
                };
                n.next = function() {
                    if (this.h === this.N) {
                        throw new RangeError("LinkList iterator access denied!");
                    }
                    this.h = this.h.V;
                    return this;
                };
            }
            return n;
        }
        Object.defineProperty(LinkListIterator.prototype, "pointer", {
            get: function() {
                if (this.h === this.N) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                return this.h.R;
            },
            set: function(t) {
                if (this.h === this.N) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.h.R = t;
            },
            enumerable: false,
            configurable: true
        });
        LinkListIterator.prototype.equals = function(t) {
            return this.h === t.h;
        };
        LinkListIterator.prototype.copy = function() {
            return new LinkListIterator(this.h, this.N, this.iteratorType);
        };
        return LinkListIterator;
    }(i);
    var w = function(t) {
        __extends(LinkList, t);
        function LinkList(i) {
            if (i === void 0) {
                i = [];
            }
            var e = t.call(this) || this;
            e.N = new d;
            e.j = undefined;
            e.P = undefined;
            i.forEach((function(t) {
                return e.pushBack(t);
            }));
            return e;
        }
        LinkList.prototype.clear = function() {
            this.t = 0;
            this.j = this.P = undefined;
            this.N.V = this.N.H = undefined;
        };
        LinkList.prototype.begin = function() {
            return new l(this.j || this.N, this.N);
        };
        LinkList.prototype.end = function() {
            return new l(this.N, this.N);
        };
        LinkList.prototype.rBegin = function() {
            return new l(this.P || this.N, this.N, 1);
        };
        LinkList.prototype.rEnd = function() {
            return new l(this.N, this.N, 1);
        };
        LinkList.prototype.front = function() {
            return this.j ? this.j.R : undefined;
        };
        LinkList.prototype.back = function() {
            return this.P ? this.P.R : undefined;
        };
        LinkList.prototype.forEach = function(t) {
            if (!this.t) return;
            var i = this.j;
            var e = 0;
            while (i !== this.N) {
                t(i.R, e++);
                i = i.H;
            }
        };
        LinkList.prototype.getElementByPos = function(t) {
            var i = this.j;
            while (t--) {
                i = i.H;
            }
            return i.R;
        };
        LinkList.prototype.eraseElementByPos = function(t) {
            if (t === 0) this.popFront(); else if (t === this.t - 1) this.popBack(); else {
                var i = this.j;
                while (t--) {
                    i = i.H;
                }
                i = i;
                var e = i.V;
                var r = i.H;
                r.V = e;
                e.H = r;
                this.t -= 1;
            }
        };
        LinkList.prototype.eraseElementByValue = function(t) {
            while (this.j && this.j.R === t) this.popFront();
            while (this.P && this.P.R === t) this.popBack();
            if (!this.j) return;
            var i = this.j;
            while (i !== this.N) {
                if (i.R === t) {
                    var e = i.V;
                    var r = i.H;
                    r.V = e;
                    e.H = r;
                    this.t -= 1;
                }
                i = i.H;
            }
        };
        LinkList.prototype.eraseElementByIterator = function(t) {
            var i = t.h;
            if (i === this.N) {
                throw new RangeError("Invalid iterator");
            }
            t = t.next();
            if (this.j === i) this.popFront(); else if (this.P === i) this.popBack(); else {
                var e = i.V;
                var r = i.H;
                r.V = e;
                e.H = r;
                this.t -= 1;
            }
            return t;
        };
        LinkList.prototype.pushBack = function(t) {
            this.t += 1;
            var i = new d(t);
            if (!this.P) {
                this.j = this.P = i;
                this.N.H = this.j;
                this.j.V = this.N;
            } else {
                this.P.H = i;
                i.V = this.P;
                this.P = i;
            }
            this.P.H = this.N;
            this.N.V = this.P;
        };
        LinkList.prototype.popBack = function() {
            if (!this.P) return;
            this.t -= 1;
            if (this.j === this.P) {
                this.j = this.P = undefined;
                this.N.H = undefined;
            } else {
                this.P = this.P.V;
                this.P.H = this.N;
            }
            this.N.V = this.P;
        };
        LinkList.prototype.setElementByPos = function(t, i) {
            var e = this.j;
            while (t--) {
                e = e.H;
            }
            e.R = i;
        };
        LinkList.prototype.insert = function(t, i, e) {
            if (e === void 0) {
                e = 1;
            }
            if (e <= 0) return;
            if (t === 0) {
                while (e--) this.pushFront(i);
            } else if (t === this.t) {
                while (e--) this.pushBack(i);
            } else {
                var r = this.j;
                for (var n = 1; n < t; ++n) {
                    r = r.H;
                }
                var s = r.H;
                this.t += e;
                while (e--) {
                    r.H = new d(i);
                    r.H.V = r;
                    r = r.H;
                }
                r.H = s;
                s.V = r;
            }
        };
        LinkList.prototype.find = function(t) {
            if (!this.j) return this.end();
            var i = this.j;
            while (i !== this.N) {
                if (i.R === t) {
                    return new l(i, this.N);
                }
                i = i.H;
            }
            return this.end();
        };
        LinkList.prototype.reverse = function() {
            if (this.t <= 1) return;
            var t = this.j;
            var i = this.P;
            var e = 0;
            while (e << 1 < this.t) {
                var r = t.R;
                t.R = i.R;
                i.R = r;
                t = t.H;
                i = i.V;
                e += 1;
            }
        };
        LinkList.prototype.unique = function() {
            if (this.t <= 1) return;
            var t = this.j;
            while (t !== this.N) {
                var i = t;
                while (i.H && i.R === i.H.R) {
                    i = i.H;
                    this.t -= 1;
                }
                t.H = i.H;
                t.H.V = t;
                t = t.H;
            }
        };
        LinkList.prototype.sort = function(t) {
            if (this.t <= 1) return;
            var i = [];
            this.forEach((function(t) {
                return i.push(t);
            }));
            i.sort(t);
            var e = this.j;
            i.forEach((function(t) {
                e.R = t;
                e = e.H;
            }));
        };
        LinkList.prototype.pushFront = function(t) {
            this.t += 1;
            var i = new d(t);
            if (!this.j) {
                this.j = this.P = i;
                this.P.H = this.N;
                this.N.V = this.P;
            } else {
                i.H = this.j;
                this.j.V = i;
                this.j = i;
            }
            this.N.H = this.j;
            this.j.V = this.N;
        };
        LinkList.prototype.popFront = function() {
            if (!this.j) return;
            this.t -= 1;
            if (this.j === this.P) {
                this.j = this.P = undefined;
                this.N.V = this.P;
            } else {
                this.j = this.j.H;
                this.j.V = this.N;
            }
            this.N.H = this.j;
        };
        LinkList.prototype.merge = function(t) {
            var i = this;
            if (!this.j) {
                t.forEach((function(t) {
                    return i.pushBack(t);
                }));
                return;
            }
            var e = this.j;
            t.forEach((function(t) {
                while (e && e !== i.N && e.R <= t) {
                    e = e.H;
                }
                if (e === i.N) {
                    i.pushBack(t);
                    e = i.P;
                } else if (e === i.j) {
                    i.pushFront(t);
                    e = i.j;
                } else {
                    i.t += 1;
                    var r = e.V;
                    r.H = new d(t);
                    r.H.V = r;
                    r.H.H = e;
                    e.V = r.H;
                }
            }));
        };
        LinkList.prototype[Symbol.iterator] = function() {
            return function() {
                var t;
                return __generator(this, (function(i) {
                    switch (i.label) {
                      case 0:
                        if (!this.j) return [ 2 ];
                        t = this.j;
                        i.label = 1;

                      case 1:
                        if (!(t !== this.N)) return [ 3, 3 ];
                        return [ 4, t.R ];

                      case 2:
                        i.sent();
                        t = t.H;
                        return [ 3, 1 ];

                      case 3:
                        return [ 2 ];
                    }
                }));
            }.bind(this)();
        };
        return LinkList;
    }(s);
    var _ = function() {
        function TreeNode(t, i) {
            this.A = 1;
            this.B = undefined;
            this.R = undefined;
            this.G = undefined;
            this.J = undefined;
            this.F = undefined;
            this.B = t;
            this.R = i;
        }
        TreeNode.prototype.pre = function() {
            var t = this;
            if (t.A === 1 && t.F.F === t) {
                t = t.J;
            } else if (t.G) {
                t = t.G;
                while (t.J) {
                    t = t.J;
                }
            } else {
                var i = t.F;
                while (i.G === t) {
                    t = i;
                    i = t.F;
                }
                t = i;
            }
            return t;
        };
        TreeNode.prototype.next = function() {
            var t = this;
            if (t.J) {
                t = t.J;
                while (t.G) {
                    t = t.G;
                }
                return t;
            } else {
                var i = t.F;
                while (i.J === t) {
                    t = i;
                    i = t.F;
                }
                if (t.J !== i) {
                    return i;
                } else return t;
            }
        };
        TreeNode.prototype.rotateLeft = function() {
            var t = this.F;
            var i = this.J;
            var e = i.G;
            if (t.F === this) t.F = i; else if (t.G === this) t.G = i; else t.J = i;
            i.F = t;
            i.G = this;
            this.F = i;
            this.J = e;
            if (e) e.F = this;
            return i;
        };
        TreeNode.prototype.rotateRight = function() {
            var t = this.F;
            var i = this.G;
            var e = i.J;
            if (t.F === this) t.F = i; else if (t.G === this) t.G = i; else t.J = i;
            i.F = t;
            i.J = this;
            this.F = i;
            this.G = e;
            if (e) e.F = this;
            return i;
        };
        return TreeNode;
    }();
    var y = function(t) {
        __extends(TreeNodeEnableIndex, t);
        function TreeNodeEnableIndex() {
            var i = t !== null && t.apply(this, arguments) || this;
            i.G = undefined;
            i.J = undefined;
            i.F = undefined;
            i.K = 1;
            return i;
        }
        TreeNodeEnableIndex.prototype.rotateLeft = function() {
            var i = t.prototype.rotateLeft.call(this);
            this.recount();
            i.recount();
            return i;
        };
        TreeNodeEnableIndex.prototype.rotateRight = function() {
            var i = t.prototype.rotateRight.call(this);
            this.recount();
            i.recount();
            return i;
        };
        TreeNodeEnableIndex.prototype.recount = function() {
            this.K = 1;
            if (this.G) this.K += this.G.K;
            if (this.J) this.K += this.J.K;
        };
        return TreeNodeEnableIndex;
    }(_);
    var L = function(t) {
        __extends(TreeContainer, t);
        function TreeContainer(i, e) {
            if (i === void 0) {
                i = function(t, i) {
                    if (t < i) return -1;
                    if (t > i) return 1;
                    return 0;
                };
            }
            if (e === void 0) {
                e = false;
            }
            var r = t.call(this) || this;
            r.U = undefined;
            r.W = function(t, i) {
                if (t === undefined) return false;
                var e = r.W(t.G, i);
                if (e) return true;
                if (i(t)) return true;
                return r.W(t.J, i);
            };
            r.M = i;
            if (e) {
                r.X = y;
                r.Y = function(t, i, e) {
                    var r = this.Z(t, i, e);
                    if (r) {
                        var n = r.F;
                        while (n !== this.N) {
                            n.K += 1;
                            n = n.F;
                        }
                        var s = this.$(r);
                        if (s) {
                            var h = s, u = h.parentNode, f = h.grandParent, a = h.curNode;
                            u.recount();
                            f.recount();
                            a.recount();
                        }
                    }
                };
                r.tt = function(t) {
                    var i = this.it(t);
                    while (i !== this.N) {
                        i.K -= 1;
                        i = i.F;
                    }
                };
            } else {
                r.X = _;
                r.Y = function(t, i, e) {
                    var r = this.Z(t, i, e);
                    if (r) this.$(r);
                };
                r.tt = r.it;
            }
            r.N = new r.X;
            return r;
        }
        TreeContainer.prototype.et = function(t, i) {
            var e;
            while (t) {
                var r = this.M(t.B, i);
                if (r < 0) {
                    t = t.J;
                } else if (r > 0) {
                    e = t;
                    t = t.G;
                } else return t;
            }
            return e === undefined ? this.N : e;
        };
        TreeContainer.prototype.rt = function(t, i) {
            var e;
            while (t) {
                var r = this.M(t.B, i);
                if (r <= 0) {
                    t = t.J;
                } else {
                    e = t;
                    t = t.G;
                }
            }
            return e === undefined ? this.N : e;
        };
        TreeContainer.prototype.nt = function(t, i) {
            var e;
            while (t) {
                var r = this.M(t.B, i);
                if (r < 0) {
                    e = t;
                    t = t.J;
                } else if (r > 0) {
                    t = t.G;
                } else return t;
            }
            return e === undefined ? this.N : e;
        };
        TreeContainer.prototype.st = function(t, i) {
            var e;
            while (t) {
                var r = this.M(t.B, i);
                if (r < 0) {
                    e = t;
                    t = t.J;
                } else {
                    t = t.G;
                }
            }
            return e === undefined ? this.N : e;
        };
        TreeContainer.prototype.ht = function(t) {
            while (true) {
                var i = t.F;
                if (i === this.N) return;
                if (t.A === 1) {
                    t.A = 0;
                    return;
                }
                if (t === i.G) {
                    var e = i.J;
                    if (e.A === 1) {
                        e.A = 0;
                        i.A = 1;
                        if (i === this.U) {
                            this.U = i.rotateLeft();
                        } else i.rotateLeft();
                    } else {
                        if (e.J && e.J.A === 1) {
                            e.A = i.A;
                            i.A = 0;
                            e.J.A = 0;
                            if (i === this.U) {
                                this.U = i.rotateLeft();
                            } else i.rotateLeft();
                            return;
                        } else if (e.G && e.G.A === 1) {
                            e.A = 1;
                            e.G.A = 0;
                            e.rotateRight();
                        } else {
                            e.A = 1;
                            t = i;
                        }
                    }
                } else {
                    var e = i.G;
                    if (e.A === 1) {
                        e.A = 0;
                        i.A = 1;
                        if (i === this.U) {
                            this.U = i.rotateRight();
                        } else i.rotateRight();
                    } else {
                        if (e.G && e.G.A === 1) {
                            e.A = i.A;
                            i.A = 0;
                            e.G.A = 0;
                            if (i === this.U) {
                                this.U = i.rotateRight();
                            } else i.rotateRight();
                            return;
                        } else if (e.J && e.J.A === 1) {
                            e.A = 1;
                            e.J.A = 0;
                            e.rotateLeft();
                        } else {
                            e.A = 1;
                            t = i;
                        }
                    }
                }
            }
        };
        TreeContainer.prototype.it = function(t) {
            var i, e;
            if (this.t === 1) {
                this.clear();
                return this.N;
            }
            var r = t;
            while (r.G || r.J) {
                if (r.J) {
                    r = r.J;
                    while (r.G) r = r.G;
                } else {
                    r = r.G;
                }
                i = __read([ r.B, t.B ], 2), t.B = i[0], r.B = i[1];
                e = __read([ r.R, t.R ], 2), t.R = e[0], r.R = e[1];
                t = r;
            }
            if (this.N.G === r) {
                this.N.G = r.F;
            } else if (this.N.J === r) {
                this.N.J = r.F;
            }
            this.ht(r);
            var n = r.F;
            if (r === n.G) {
                n.G = undefined;
            } else n.J = undefined;
            this.t -= 1;
            this.U.A = 0;
            return n;
        };
        TreeContainer.prototype.$ = function(t) {
            while (true) {
                var i = t.F;
                if (i.A === 0) return;
                var e = i.F;
                if (i === e.G) {
                    var r = e.J;
                    if (r && r.A === 1) {
                        r.A = i.A = 0;
                        if (e === this.U) return;
                        e.A = 1;
                        t = e;
                        continue;
                    } else if (t === i.J) {
                        t.A = 0;
                        if (t.G) t.G.F = i;
                        if (t.J) t.J.F = e;
                        i.J = t.G;
                        e.G = t.J;
                        t.G = i;
                        t.J = e;
                        if (e === this.U) {
                            this.U = t;
                            this.N.F = t;
                        } else {
                            var n = e.F;
                            if (n.G === e) {
                                n.G = t;
                            } else n.J = t;
                        }
                        t.F = e.F;
                        i.F = t;
                        e.F = t;
                        e.A = 1;
                        return {
                            parentNode: i,
                            grandParent: e,
                            curNode: t
                        };
                    } else {
                        i.A = 0;
                        if (e === this.U) {
                            this.U = e.rotateRight();
                        } else e.rotateRight();
                        e.A = 1;
                    }
                } else {
                    var r = e.G;
                    if (r && r.A === 1) {
                        r.A = i.A = 0;
                        if (e === this.U) return;
                        e.A = 1;
                        t = e;
                        continue;
                    } else if (t === i.G) {
                        t.A = 0;
                        if (t.G) t.G.F = e;
                        if (t.J) t.J.F = i;
                        e.J = t.G;
                        i.G = t.J;
                        t.G = e;
                        t.J = i;
                        if (e === this.U) {
                            this.U = t;
                            this.N.F = t;
                        } else {
                            var n = e.F;
                            if (n.G === e) {
                                n.G = t;
                            } else n.J = t;
                        }
                        t.F = e.F;
                        i.F = t;
                        e.F = t;
                        e.A = 1;
                        return {
                            parentNode: i,
                            grandParent: e,
                            curNode: t
                        };
                    } else {
                        i.A = 0;
                        if (e === this.U) {
                            this.U = e.rotateLeft();
                        } else e.rotateLeft();
                        e.A = 1;
                    }
                }
                return;
            }
        };
        TreeContainer.prototype.Z = function(t, i, e) {
            if (this.U === undefined) {
                this.t += 1;
                this.U = new this.X(t, i);
                this.U.A = 0;
                this.U.F = this.N;
                this.N.F = this.U;
                this.N.G = this.U;
                this.N.J = this.U;
                return;
            }
            var r;
            var n = this.N.G;
            var s = this.M(n.B, t);
            if (s === 0) {
                n.R = i;
                return;
            } else if (s > 0) {
                n.G = new this.X(t, i);
                n.G.F = n;
                r = n.G;
                this.N.G = r;
            } else {
                var h = this.N.J;
                var u = this.M(h.B, t);
                if (u === 0) {
                    h.R = i;
                    return;
                } else if (u < 0) {
                    h.J = new this.X(t, i);
                    h.J.F = h;
                    r = h.J;
                    this.N.J = r;
                } else {
                    if (e !== undefined) {
                        var f = e.h;
                        if (f !== this.N) {
                            var a = this.M(f.B, t);
                            if (a === 0) {
                                f.R = i;
                                return;
                            } else if (a > 0) {
                                var o = f.pre();
                                var c = this.M(o.B, t);
                                if (c === 0) {
                                    o.R = i;
                                    return;
                                } else if (c < 0) {
                                    r = new this.X(t, i);
                                    if (o.J === undefined) {
                                        o.J = r;
                                        r.F = o;
                                    } else {
                                        f.G = r;
                                        r.F = f;
                                    }
                                }
                            }
                        }
                    }
                    if (r === undefined) {
                        r = this.U;
                        while (true) {
                            var v = this.M(r.B, t);
                            if (v > 0) {
                                if (r.G === undefined) {
                                    r.G = new this.X(t, i);
                                    r.G.F = r;
                                    r = r.G;
                                    break;
                                }
                                r = r.G;
                            } else if (v < 0) {
                                if (r.J === undefined) {
                                    r.J = new this.X(t, i);
                                    r.J.F = r;
                                    r = r.J;
                                    break;
                                }
                                r = r.J;
                            } else {
                                r.R = i;
                                return;
                            }
                        }
                    }
                }
            }
            this.t += 1;
            return r;
        };
        TreeContainer.prototype.clear = function() {
            this.t = 0;
            this.U = undefined;
            this.N.F = undefined;
            this.N.G = this.N.J = undefined;
        };
        TreeContainer.prototype.updateKeyByIterator = function(t, i) {
            var e = t.h;
            if (e === this.N) {
                throw new TypeError("Invalid iterator!");
            }
            if (this.t === 1) {
                e.B = i;
                return true;
            }
            if (e === this.N.G) {
                if (this.M(e.next().B, i) > 0) {
                    e.B = i;
                    return true;
                }
                return false;
            }
            if (e === this.N.J) {
                if (this.M(e.pre().B, i) < 0) {
                    e.B = i;
                    return true;
                }
                return false;
            }
            var r = e.pre().B;
            if (this.M(r, i) >= 0) return false;
            var n = e.next().B;
            if (this.M(n, i) <= 0) return false;
            e.B = i;
            return true;
        };
        TreeContainer.prototype.eraseElementByPos = function(t) {
            var i = this;
            var e = 0;
            this.W(this.U, (function(r) {
                if (t === e) {
                    i.tt(r);
                    return true;
                }
                e += 1;
                return false;
            }));
        };
        TreeContainer.prototype.ut = function(t, i) {
            while (t) {
                var e = this.M(t.B, i);
                if (e < 0) {
                    t = t.J;
                } else if (e > 0) {
                    t = t.G;
                } else return t;
            }
            return t;
        };
        TreeContainer.prototype.eraseElementByKey = function(t) {
            if (!this.t) return;
            var i = this.ut(this.U, t);
            if (i === undefined) return;
            this.tt(i);
        };
        TreeContainer.prototype.eraseElementByIterator = function(t) {
            var i = t.h;
            if (i === this.N) {
                throw new RangeError("Invalid iterator");
            }
            if (i.J === undefined) {
                t = t.next();
            }
            this.tt(i);
            return t;
        };
        TreeContainer.prototype.getHeight = function() {
            if (!this.t) return 0;
            var traversal = function(t) {
                if (!t) return 0;
                return Math.max(traversal(t.G), traversal(t.J)) + 1;
            };
            return traversal(this.U);
        };
        return TreeContainer;
    }(r);
    var p = function(t) {
        __extends(TreeIterator, t);
        function TreeIterator(i, e, r) {
            var n = t.call(this, r) || this;
            n.h = i;
            n.N = e;
            if (n.iteratorType === 0) {
                n.pre = function() {
                    if (this.h === this.N.G) {
                        throw new RangeError("Tree iterator access denied!");
                    }
                    this.h = this.h.pre();
                    return this;
                };
                n.next = function() {
                    if (this.h === this.N) {
                        throw new RangeError("Tree iterator access denied!");
                    }
                    this.h = this.h.next();
                    return this;
                };
            } else {
                n.pre = function() {
                    if (this.h === this.N.J) {
                        throw new RangeError("Tree iterator access denied!");
                    }
                    this.h = this.h.next();
                    return this;
                };
                n.next = function() {
                    if (this.h === this.N) {
                        throw new RangeError("Tree iterator access denied!");
                    }
                    this.h = this.h.pre();
                    return this;
                };
            }
            return n;
        }
        Object.defineProperty(TreeIterator.prototype, "index", {
            get: function() {
                var t = this.h;
                var i = this.N.F;
                if (t === this.N) {
                    if (i) {
                        return i.K - 1;
                    }
                    return 0;
                }
                var e = 0;
                if (t.G) {
                    e += t.G.K;
                }
                while (t !== i) {
                    var r = t.F;
                    if (t === r.J) {
                        e += 1;
                        if (r.G) {
                            e += r.G.K;
                        }
                    }
                    t = r;
                }
                return e;
            },
            enumerable: false,
            configurable: true
        });
        TreeIterator.prototype.equals = function(t) {
            return this.h === t.h;
        };
        return TreeIterator;
    }(i);
    var O = function(t) {
        __extends(OrderedSetIterator, t);
        function OrderedSetIterator() {
            return t !== null && t.apply(this, arguments) || this;
        }
        Object.defineProperty(OrderedSetIterator.prototype, "pointer", {
            get: function() {
                if (this.h === this.N) {
                    throw new RangeError("OrderedSet iterator access denied!");
                }
                return this.h.B;
            },
            enumerable: false,
            configurable: true
        });
        OrderedSetIterator.prototype.copy = function() {
            return new OrderedSetIterator(this.h, this.N, this.iteratorType);
        };
        return OrderedSetIterator;
    }(p);
    var b = function(t) {
        __extends(OrderedSet, t);
        function OrderedSet(i, e, r) {
            if (i === void 0) {
                i = [];
            }
            var n = t.call(this, e, r) || this;
            n.ft = function(t) {
                return __generator(this, (function(i) {
                    switch (i.label) {
                      case 0:
                        if (t === undefined) return [ 2 ];
                        return [ 5, __values(this.ft(t.G)) ];

                      case 1:
                        i.sent();
                        return [ 4, t.B ];

                      case 2:
                        i.sent();
                        return [ 5, __values(this.ft(t.J)) ];

                      case 3:
                        i.sent();
                        return [ 2 ];
                    }
                }));
            };
            i.forEach((function(t) {
                return n.insert(t);
            }));
            return n;
        }
        OrderedSet.prototype.begin = function() {
            return new O(this.N.G || this.N, this.N);
        };
        OrderedSet.prototype.end = function() {
            return new O(this.N, this.N);
        };
        OrderedSet.prototype.rBegin = function() {
            return new O(this.N.J || this.N, this.N, 1);
        };
        OrderedSet.prototype.rEnd = function() {
            return new O(this.N, this.N, 1);
        };
        OrderedSet.prototype.front = function() {
            return this.N.G ? this.N.G.B : undefined;
        };
        OrderedSet.prototype.back = function() {
            return this.N.J ? this.N.J.B : undefined;
        };
        OrderedSet.prototype.forEach = function(t) {
            var i, e;
            var r = 0;
            try {
                for (var n = __values(this), s = n.next(); !s.done; s = n.next()) {
                    var h = s.value;
                    t(h, r++);
                }
            } catch (t) {
                i = {
                    error: t
                };
            } finally {
                try {
                    if (s && !s.done && (e = n.return)) e.call(n);
                } finally {
                    if (i) throw i.error;
                }
            }
        };
        OrderedSet.prototype.getElementByPos = function(t) {
            var i, e;
            var r;
            var n = 0;
            try {
                for (var s = __values(this), h = s.next(); !h.done; h = s.next()) {
                    var u = h.value;
                    if (n === t) {
                        r = u;
                        break;
                    }
                    n += 1;
                }
            } catch (t) {
                i = {
                    error: t
                };
            } finally {
                try {
                    if (h && !h.done && (e = s.return)) e.call(s);
                } finally {
                    if (i) throw i.error;
                }
            }
            return r;
        };
        OrderedSet.prototype.insert = function(t, i) {
            this.Y(t, undefined, i);
        };
        OrderedSet.prototype.find = function(t) {
            var i = this.ut(this.U, t);
            if (i !== undefined) {
                return new O(i, this.N);
            }
            return this.end();
        };
        OrderedSet.prototype.lowerBound = function(t) {
            var i = this.et(this.U, t);
            return new O(i, this.N);
        };
        OrderedSet.prototype.upperBound = function(t) {
            var i = this.rt(this.U, t);
            return new O(i, this.N);
        };
        OrderedSet.prototype.reverseLowerBound = function(t) {
            var i = this.nt(this.U, t);
            return new O(i, this.N);
        };
        OrderedSet.prototype.reverseUpperBound = function(t) {
            var i = this.st(this.U, t);
            return new O(i, this.N);
        };
        OrderedSet.prototype.union = function(t) {
            var i = this;
            t.forEach((function(t) {
                return i.insert(t);
            }));
        };
        OrderedSet.prototype[Symbol.iterator] = function() {
            return this.ft(this.U);
        };
        return OrderedSet;
    }(L);
    var k = function(t) {
        __extends(OrderedMapIterator, t);
        function OrderedMapIterator() {
            return t !== null && t.apply(this, arguments) || this;
        }
        Object.defineProperty(OrderedMapIterator.prototype, "pointer", {
            get: function() {
                var t = this;
                if (this.h === this.N) {
                    throw new RangeError("OrderedMap iterator access denied");
                }
                return new Proxy([], {
                    get: function(i, e) {
                        if (e === "0") return t.h.B; else if (e === "1") return t.h.R;
                    },
                    set: function(i, e, r) {
                        if (e !== "1") {
                            throw new TypeError("props must be 1");
                        }
                        t.h.R = r;
                        return true;
                    }
                });
            },
            enumerable: false,
            configurable: true
        });
        OrderedMapIterator.prototype.copy = function() {
            return new OrderedMapIterator(this.h, this.N, this.iteratorType);
        };
        return OrderedMapIterator;
    }(p);
    var S = function(t) {
        __extends(OrderedMap, t);
        function OrderedMap(i, e, r) {
            if (i === void 0) {
                i = [];
            }
            var n = t.call(this, e, r) || this;
            n.ft = function(t) {
                return __generator(this, (function(i) {
                    switch (i.label) {
                      case 0:
                        if (t === undefined) return [ 2 ];
                        return [ 5, __values(this.ft(t.G)) ];

                      case 1:
                        i.sent();
                        return [ 4, [ t.B, t.R ] ];

                      case 2:
                        i.sent();
                        return [ 5, __values(this.ft(t.J)) ];

                      case 3:
                        i.sent();
                        return [ 2 ];
                    }
                }));
            };
            i.forEach((function(t) {
                var i = __read(t, 2), e = i[0], r = i[1];
                return n.setElement(e, r);
            }));
            return n;
        }
        OrderedMap.prototype.begin = function() {
            return new k(this.N.G || this.N, this.N);
        };
        OrderedMap.prototype.end = function() {
            return new k(this.N, this.N);
        };
        OrderedMap.prototype.rBegin = function() {
            return new k(this.N.J || this.N, this.N, 1);
        };
        OrderedMap.prototype.rEnd = function() {
            return new k(this.N, this.N, 1);
        };
        OrderedMap.prototype.front = function() {
            if (!this.t) return undefined;
            var t = this.N.G;
            return [ t.B, t.R ];
        };
        OrderedMap.prototype.back = function() {
            if (!this.t) return undefined;
            var t = this.N.J;
            return [ t.B, t.R ];
        };
        OrderedMap.prototype.forEach = function(t) {
            var i, e;
            var r = 0;
            try {
                for (var n = __values(this), s = n.next(); !s.done; s = n.next()) {
                    var h = s.value;
                    t(h, r++);
                }
            } catch (t) {
                i = {
                    error: t
                };
            } finally {
                try {
                    if (s && !s.done && (e = n.return)) e.call(n);
                } finally {
                    if (i) throw i.error;
                }
            }
        };
        OrderedMap.prototype.lowerBound = function(t) {
            var i = this.et(this.U, t);
            return new k(i, this.N);
        };
        OrderedMap.prototype.upperBound = function(t) {
            var i = this.rt(this.U, t);
            return new k(i, this.N);
        };
        OrderedMap.prototype.reverseLowerBound = function(t) {
            var i = this.nt(this.U, t);
            return new k(i, this.N);
        };
        OrderedMap.prototype.reverseUpperBound = function(t) {
            var i = this.st(this.U, t);
            return new k(i, this.N);
        };
        OrderedMap.prototype.setElement = function(t, i, e) {
            this.Y(t, i, e);
        };
        OrderedMap.prototype.find = function(t) {
            var i = this.ut(this.U, t);
            if (i !== undefined) {
                return new k(i, this.N);
            }
            return this.end();
        };
        OrderedMap.prototype.getElementByKey = function(t) {
            var i = this.ut(this.U, t);
            return i ? i.R : undefined;
        };
        OrderedMap.prototype.getElementByPos = function(t) {
            var i, e;
            var r;
            var n = 0;
            try {
                for (var s = __values(this), h = s.next(); !h.done; h = s.next()) {
                    var u = h.value;
                    if (n === t) {
                        r = u;
                        break;
                    }
                    n += 1;
                }
            } catch (t) {
                i = {
                    error: t
                };
            } finally {
                try {
                    if (h && !h.done && (e = s.return)) e.call(s);
                } finally {
                    if (i) throw i.error;
                }
            }
            return r;
        };
        OrderedMap.prototype.union = function(t) {
            var i = this;
            t.forEach((function(t) {
                var e = __read(t, 2), r = e[0], n = e[1];
                return i.setElement(r, n);
            }));
        };
        OrderedMap.prototype[Symbol.iterator] = function() {
            return this.ft(this.U);
        };
        return OrderedMap;
    }(L);
    var I = function(t) {
        __extends(HashContainer, t);
        function HashContainer(i, e) {
            if (i === void 0) {
                i = 16;
            }
            if (e === void 0) {
                e = function(t) {
                    var i;
                    if (typeof t !== "string") {
                        i = JSON.stringify(t);
                    } else i = t;
                    var e = 0;
                    var r = i.length;
                    for (var n = 0; n < r; n++) {
                        var s = i.charCodeAt(n);
                        e = (e << 5) - e + s;
                        e |= 0;
                    }
                    return e >>> 0;
                };
            }
            var r = t.call(this) || this;
            if (i < 16 || (i & i - 1) !== 0) {
                throw new RangeError("InitBucketNum range error");
            }
            r.O = r.ot = i;
            r.ct = e;
            return r;
        }
        HashContainer.prototype.clear = function() {
            this.t = 0;
            this.O = this.ot;
            this.vt = [];
        };
        return HashContainer;
    }(e);
    var g = function(t) {
        __extends(HashSet, t);
        function HashSet(i, e, r) {
            if (i === void 0) {
                i = [];
            }
            var n = t.call(this, e, r) || this;
            n.vt = [];
            i.forEach((function(t) {
                return n.insert(t);
            }));
            return n;
        }
        HashSet.prototype.I = function() {
            var t = this;
            if (this.O >= 1073741824) return;
            var i = [];
            var e = this.O;
            this.O <<= 1;
            var r = Object.keys(this.vt);
            var n = r.length;
            var _loop_1 = function(n) {
                var h = parseInt(r[n]);
                var u = s.vt[h];
                var f = u.size();
                if (f === 0) return "continue";
                if (f === 1) {
                    var a = u.front();
                    i[s.ct(a) & s.O - 1] = new v([ a ], false);
                    return "continue";
                }
                var o = [];
                var c = [];
                u.forEach((function(i) {
                    var r = t.ct(i);
                    if ((r & e) === 0) {
                        o.push(i);
                    } else c.push(i);
                }));
                if (u instanceof b) {
                    if (o.length > 6) {
                        i[h] = new b(o);
                    } else {
                        i[h] = new v(o, false);
                    }
                    if (c.length > 6) {
                        i[h + e] = new b(c);
                    } else {
                        i[h + e] = new v(c, false);
                    }
                } else {
                    i[h] = new v(o, false);
                    i[h + e] = new v(c, false);
                }
            };
            var s = this;
            for (var h = 0; h < n; ++h) {
                _loop_1(h);
            }
            this.vt = i;
        };
        HashSet.prototype.forEach = function(t) {
            var i = Object.values(this.vt);
            var e = i.length;
            var r = 0;
            for (var n = 0; n < e; ++n) {
                i[n].forEach((function(i) {
                    return t(i, r++);
                }));
            }
        };
        HashSet.prototype.insert = function(t) {
            var i = this.ct(t) & this.O - 1;
            var e = this.vt[i];
            if (!e) {
                this.vt[i] = new v([ t ], false);
                this.t += 1;
            } else {
                var r = e.size();
                if (e instanceof v) {
                    if (!e.find(t).equals(e.end())) return;
                    e.pushBack(t);
                    if (r + 1 >= 8) {
                        if (this.O <= 64) {
                            this.t += 1;
                            this.I();
                            return;
                        }
                        this.vt[i] = new b(e);
                    }
                    this.t += 1;
                } else {
                    e.insert(t);
                    var n = e.size();
                    this.t += n - r;
                }
            }
            if (this.t > this.O * .75) {
                this.I();
            }
        };
        HashSet.prototype.eraseElementByKey = function(t) {
            var i = this.ct(t) & this.O - 1;
            var e = this.vt[i];
            if (!e) return;
            var r = e.size();
            if (r === 0) return;
            if (e instanceof v) {
                e.eraseElementByValue(t);
                var n = e.size();
                this.t += n - r;
            } else {
                e.eraseElementByKey(t);
                var n = e.size();
                this.t += n - r;
                if (n <= 6) {
                    this.vt[i] = new v(e);
                }
            }
        };
        HashSet.prototype.find = function(t) {
            var i = this.ct(t) & this.O - 1;
            var e = this.vt[i];
            if (!e) return false;
            return !e.find(t).equals(e.end());
        };
        HashSet.prototype[Symbol.iterator] = function() {
            return function() {
                var t, i, e, r, n, s, h, u;
                var f, a;
                return __generator(this, (function(o) {
                    switch (o.label) {
                      case 0:
                        t = Object.values(this.vt);
                        i = t.length;
                        e = 0;
                        o.label = 1;

                      case 1:
                        if (!(e < i)) return [ 3, 10 ];
                        r = t[e];
                        o.label = 2;

                      case 2:
                        o.trys.push([ 2, 7, 8, 9 ]);
                        n = (f = void 0, __values(r)), s = n.next();
                        o.label = 3;

                      case 3:
                        if (!!s.done) return [ 3, 6 ];
                        h = s.value;
                        return [ 4, h ];

                      case 4:
                        o.sent();
                        o.label = 5;

                      case 5:
                        s = n.next();
                        return [ 3, 3 ];

                      case 6:
                        return [ 3, 9 ];

                      case 7:
                        u = o.sent();
                        f = {
                            error: u
                        };
                        return [ 3, 9 ];

                      case 8:
                        try {
                            if (s && !s.done && (a = n.return)) a.call(n);
                        } finally {
                            if (f) throw f.error;
                        }
                        return [ 7 ];

                      case 9:
                        ++e;
                        return [ 3, 1 ];

                      case 10:
                        return [ 2 ];
                    }
                }));
            }.bind(this)();
        };
        return HashSet;
    }(I);
    var T = function(t) {
        __extends(HashMap, t);
        function HashMap(i, e, r) {
            if (i === void 0) {
                i = [];
            }
            var n = t.call(this, e, r) || this;
            n.vt = [];
            i.forEach((function(t) {
                return n.setElement(t[0], t[1]);
            }));
            return n;
        }
        HashMap.prototype.I = function() {
            var t = this;
            if (this.O >= 1073741824) return;
            var i = [];
            var e = this.O;
            this.O <<= 1;
            var r = Object.keys(this.vt);
            var n = r.length;
            var _loop_1 = function(n) {
                var h = parseInt(r[n]);
                var u = s.vt[h];
                var f = u.size();
                if (f === 0) return "continue";
                if (f === 1) {
                    var a = u.front();
                    i[s.ct(a[0]) & s.O - 1] = new v([ a ], false);
                    return "continue";
                }
                var o = [];
                var c = [];
                u.forEach((function(i) {
                    var r = t.ct(i[0]);
                    if ((r & e) === 0) {
                        o.push(i);
                    } else c.push(i);
                }));
                if (u instanceof S) {
                    if (o.length > 6) {
                        i[h] = new S(o);
                    } else {
                        i[h] = new v(o, false);
                    }
                    if (c.length > 6) {
                        i[h + e] = new S(c);
                    } else {
                        i[h + e] = new v(c, false);
                    }
                } else {
                    i[h] = new v(o, false);
                    i[h + e] = new v(c, false);
                }
            };
            var s = this;
            for (var h = 0; h < n; ++h) {
                _loop_1(h);
            }
            this.vt = i;
        };
        HashMap.prototype.forEach = function(t) {
            var i = Object.values(this.vt);
            var e = i.length;
            var r = 0;
            for (var n = 0; n < e; ++n) {
                i[n].forEach((function(i) {
                    return t(i, r++);
                }));
            }
        };
        HashMap.prototype.setElement = function(t, i) {
            var e, r;
            var n = this.ct(t) & this.O - 1;
            var s = this.vt[n];
            if (!s) {
                this.t += 1;
                this.vt[n] = new v([ [ t, i ] ], false);
            } else {
                var h = s.size();
                if (s instanceof v) {
                    try {
                        for (var u = __values(s), f = u.next(); !f.done; f = u.next()) {
                            var a = f.value;
                            if (a[0] === t) {
                                a[1] = i;
                                return;
                            }
                        }
                    } catch (t) {
                        e = {
                            error: t
                        };
                    } finally {
                        try {
                            if (f && !f.done && (r = u.return)) r.call(u);
                        } finally {
                            if (e) throw e.error;
                        }
                    }
                    s.pushBack([ t, i ]);
                    if (h + 1 >= 8) {
                        if (this.O <= 64) {
                            this.t += 1;
                            this.I();
                            return;
                        }
                        this.vt[n] = new S(this.vt[n]);
                    }
                    this.t += 1;
                } else {
                    s.setElement(t, i);
                    var o = s.size();
                    this.t += o - h;
                }
            }
            if (this.t > this.O * .75) {
                this.I();
            }
        };
        HashMap.prototype.getElementByKey = function(t) {
            var i, e;
            var r = this.ct(t) & this.O - 1;
            var n = this.vt[r];
            if (!n) return undefined;
            if (n instanceof S) {
                return n.getElementByKey(t);
            } else {
                try {
                    for (var s = __values(n), h = s.next(); !h.done; h = s.next()) {
                        var u = h.value;
                        if (u[0] === t) return u[1];
                    }
                } catch (t) {
                    i = {
                        error: t
                    };
                } finally {
                    try {
                        if (h && !h.done && (e = s.return)) e.call(s);
                    } finally {
                        if (i) throw i.error;
                    }
                }
                return undefined;
            }
        };
        HashMap.prototype.eraseElementByKey = function(t) {
            var i, e;
            var r = this.ct(t) & this.O - 1;
            var n = this.vt[r];
            if (!n) return;
            if (n instanceof v) {
                var s = 0;
                try {
                    for (var h = __values(n), u = h.next(); !u.done; u = h.next()) {
                        var f = u.value;
                        if (f[0] === t) {
                            n.eraseElementByPos(s);
                            this.t -= 1;
                            return;
                        }
                        s += 1;
                    }
                } catch (t) {
                    i = {
                        error: t
                    };
                } finally {
                    try {
                        if (u && !u.done && (e = h.return)) e.call(h);
                    } finally {
                        if (i) throw i.error;
                    }
                }
            } else {
                var a = n.size();
                n.eraseElementByKey(t);
                var o = n.size();
                this.t += o - a;
                if (o <= 6) {
                    this.vt[r] = new v(n);
                }
            }
        };
        HashMap.prototype.find = function(t) {
            var i, e;
            var r = this.ct(t) & this.O - 1;
            var n = this.vt[r];
            if (!n) return false;
            if (n instanceof S) {
                return !n.find(t).equals(n.end());
            }
            try {
                for (var s = __values(n), h = s.next(); !h.done; h = s.next()) {
                    var u = h.value;
                    if (u[0] === t) return true;
                }
            } catch (t) {
                i = {
                    error: t
                };
            } finally {
                try {
                    if (h && !h.done && (e = s.return)) e.call(s);
                } finally {
                    if (i) throw i.error;
                }
            }
            return false;
        };
        HashMap.prototype[Symbol.iterator] = function() {
            return function() {
                var t, i, e, r, n, s, h, u;
                var f, a;
                return __generator(this, (function(o) {
                    switch (o.label) {
                      case 0:
                        t = Object.values(this.vt);
                        i = t.length;
                        e = 0;
                        o.label = 1;

                      case 1:
                        if (!(e < i)) return [ 3, 10 ];
                        r = t[e];
                        o.label = 2;

                      case 2:
                        o.trys.push([ 2, 7, 8, 9 ]);
                        n = (f = void 0, __values(r)), s = n.next();
                        o.label = 3;

                      case 3:
                        if (!!s.done) return [ 3, 6 ];
                        h = s.value;
                        return [ 4, h ];

                      case 4:
                        o.sent();
                        o.label = 5;

                      case 5:
                        s = n.next();
                        return [ 3, 3 ];

                      case 6:
                        return [ 3, 9 ];

                      case 7:
                        u = o.sent();
                        f = {
                            error: u
                        };
                        return [ 3, 9 ];

                      case 8:
                        try {
                            if (s && !s.done && (a = n.return)) a.call(n);
                        } finally {
                            if (f) throw f.error;
                        }
                        return [ 7 ];

                      case 9:
                        ++e;
                        return [ 3, 1 ];

                      case 10:
                        return [ 2 ];
                    }
                }));
            }.bind(this)();
        };
        return HashMap;
    }(I);
    t.Deque = f;
    t.HashMap = T;
    t.HashSet = g;
    t.LinkList = w;
    t.OrderedMap = S;
    t.OrderedSet = b;
    t.PriorityQueue = o;
    t.Queue = a;
    t.Stack = n;
    t.Vector = v;
    Object.defineProperty(t, "dt", {
        value: true
    });
}));