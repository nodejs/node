var __extends = this && this.t || function() {
    var extendStatics = function(t, i) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, i) {
            t.__proto__ = i;
        } || function(t, i) {
            for (var r in i) if (Object.prototype.hasOwnProperty.call(i, r)) t[r] = i[r];
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

var __generator = this && this.h || function(t, i) {
    var r = {
        label: 0,
        sent: function() {
            if (h[0] & 1) throw h[1];
            return h[1];
        },
        trys: [],
        ops: []
    }, e, s, h, n;
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
        if (e) throw new TypeError("Generator is already executing.");
        while (r) try {
            if (e = 1, s && (h = n[0] & 2 ? s["return"] : n[0] ? s["throw"] || ((h = s["return"]) && h.call(s), 
            0) : s.next) && !(h = h.call(s, n[1])).done) return h;
            if (s = 0, h) n = [ n[0] & 2, h.value ];
            switch (n[0]) {
              case 0:
              case 1:
                h = n;
                break;

              case 4:
                r.label++;
                return {
                    value: n[1],
                    done: false
                };

              case 5:
                r.label++;
                s = n[1];
                n = [ 0 ];
                continue;

              case 7:
                n = r.ops.pop();
                r.trys.pop();
                continue;

              default:
                if (!(h = r.trys, h = h.length > 0 && h[h.length - 1]) && (n[0] === 6 || n[0] === 2)) {
                    r = 0;
                    continue;
                }
                if (n[0] === 3 && (!h || n[1] > h[0] && n[1] < h[3])) {
                    r.label = n[1];
                    break;
                }
                if (n[0] === 6 && r.label < h[1]) {
                    r.label = h[1];
                    h = n;
                    break;
                }
                if (h && r.label < h[2]) {
                    r.label = h[2];
                    r.ops.push(n);
                    break;
                }
                if (h[2]) r.ops.pop();
                r.trys.pop();
                continue;
            }
            n = i.call(t, r);
        } catch (t) {
            n = [ 6, t ];
            s = 0;
        } finally {
            e = h = 0;
        }
        if (n[0] & 5) throw n[1];
        return {
            value: n[0] ? n[1] : void 0,
            done: true
        };
    }
};

var __read = this && this.P || function(t, i) {
    var r = typeof Symbol === "function" && t[Symbol.iterator];
    if (!r) return t;
    var e = r.call(t), s, h = [], n;
    try {
        while ((i === void 0 || i-- > 0) && !(s = e.next()).done) h.push(s.value);
    } catch (t) {
        n = {
            error: t
        };
    } finally {
        try {
            if (s && !s.done && (r = e["return"])) r.call(e);
        } finally {
            if (n) throw n.error;
        }
    }
    return h;
};

var __spreadArray = this && this.A || function(t, i, r) {
    if (r || arguments.length === 2) for (var e = 0, s = i.length, h; e < s; e++) {
        if (h || !(e in i)) {
            if (!h) h = Array.prototype.slice.call(i, 0, e);
            h[e] = i[e];
        }
    }
    return t.concat(h || Array.prototype.slice.call(i));
};

import SequentialContainer from "./Base";

import { RandomIterator } from "./Base/RandomIterator";

import * as Math from "../../utils/math";

var DequeIterator = function(t) {
    __extends(DequeIterator, t);
    function DequeIterator(i, r, e) {
        var s = t.call(this, i, e) || this;
        s.container = r;
        return s;
    }
    DequeIterator.prototype.copy = function() {
        return new DequeIterator(this.o, this.container, this.iteratorType);
    };
    return DequeIterator;
}(RandomIterator);

var Deque = function(t) {
    __extends(Deque, t);
    function Deque(i, r) {
        if (i === void 0) {
            i = [];
        }
        if (r === void 0) {
            r = 1 << 12;
        }
        var e = t.call(this) || this;
        e.C = 0;
        e.q = 0;
        e.D = 0;
        e.R = 0;
        e.N = 0;
        e.G = [];
        var s = function() {
            if (typeof i.length === "number") return i.length;
            if (typeof i.size === "number") return i.size;
            if (typeof i.size === "function") return i.size();
            throw new TypeError("Cannot get the length or size of the container");
        }();
        e.F = r;
        e.N = Math.ceil(s, e.F) || 1;
        for (var h = 0; h < e.N; ++h) {
            e.G.push(new Array(e.F));
        }
        var n = Math.ceil(s, e.F);
        e.C = e.D = (e.N >> 1) - (n >> 1);
        e.q = e.R = e.F - s % e.F >> 1;
        var u = e;
        i.forEach((function(t) {
            u.pushBack(t);
        }));
        return e;
    }
    Deque.prototype.J = function(t) {
        var i = [];
        var r = t || this.N >> 1 || 1;
        for (var e = 0; e < r; ++e) {
            i[e] = new Array(this.F);
        }
        for (var e = this.C; e < this.N; ++e) {
            i[i.length] = this.G[e];
        }
        for (var e = 0; e < this.D; ++e) {
            i[i.length] = this.G[e];
        }
        i[i.length] = __spreadArray([], __read(this.G[this.D]), false);
        this.C = r;
        this.D = i.length - 1;
        for (var e = 0; e < r; ++e) {
            i[i.length] = new Array(this.F);
        }
        this.G = i;
        this.N = i.length;
    };
    Deque.prototype.K = function(t) {
        var i, r;
        var e = this.q + t;
        i = this.C + Math.floor(e / this.F);
        if (i >= this.N) {
            i -= this.N;
        }
        r = (e + 1) % this.F - 1;
        if (r < 0) {
            r = this.F - 1;
        }
        return {
            curNodeBucketIndex: i,
            curNodePointerIndex: r
        };
    };
    Deque.prototype.clear = function() {
        this.G = [ new Array(this.F) ];
        this.N = 1;
        this.C = this.D = this.i = 0;
        this.q = this.R = this.F >> 1;
    };
    Deque.prototype.begin = function() {
        return new DequeIterator(0, this);
    };
    Deque.prototype.end = function() {
        return new DequeIterator(this.i, this);
    };
    Deque.prototype.rBegin = function() {
        return new DequeIterator(this.i - 1, this, 1);
    };
    Deque.prototype.rEnd = function() {
        return new DequeIterator(-1, this, 1);
    };
    Deque.prototype.front = function() {
        if (this.i === 0) return;
        return this.G[this.C][this.q];
    };
    Deque.prototype.back = function() {
        if (this.i === 0) return;
        return this.G[this.D][this.R];
    };
    Deque.prototype.pushBack = function(t) {
        if (this.i) {
            if (this.R < this.F - 1) {
                this.R += 1;
            } else if (this.D < this.N - 1) {
                this.D += 1;
                this.R = 0;
            } else {
                this.D = 0;
                this.R = 0;
            }
            if (this.D === this.C && this.R === this.q) this.J();
        }
        this.i += 1;
        this.G[this.D][this.R] = t;
        return this.i;
    };
    Deque.prototype.popBack = function() {
        if (this.i === 0) return;
        var t = this.G[this.D][this.R];
        if (this.i !== 1) {
            if (this.R > 0) {
                this.R -= 1;
            } else if (this.D > 0) {
                this.D -= 1;
                this.R = this.F - 1;
            } else {
                this.D = this.N - 1;
                this.R = this.F - 1;
            }
        }
        this.i -= 1;
        return t;
    };
    Deque.prototype.pushFront = function(t) {
        if (this.i) {
            if (this.q > 0) {
                this.q -= 1;
            } else if (this.C > 0) {
                this.C -= 1;
                this.q = this.F - 1;
            } else {
                this.C = this.N - 1;
                this.q = this.F - 1;
            }
            if (this.C === this.D && this.q === this.R) this.J();
        }
        this.i += 1;
        this.G[this.C][this.q] = t;
        return this.i;
    };
    Deque.prototype.popFront = function() {
        if (this.i === 0) return;
        var t = this.G[this.C][this.q];
        if (this.i !== 1) {
            if (this.q < this.F - 1) {
                this.q += 1;
            } else if (this.C < this.N - 1) {
                this.C += 1;
                this.q = 0;
            } else {
                this.C = 0;
                this.q = 0;
            }
        }
        this.i -= 1;
        return t;
    };
    Deque.prototype.getElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var i = this.K(t), r = i.curNodeBucketIndex, e = i.curNodePointerIndex;
        return this.G[r][e];
    };
    Deque.prototype.setElementByPos = function(t, i) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var r = this.K(t), e = r.curNodeBucketIndex, s = r.curNodePointerIndex;
        this.G[e][s] = i;
    };
    Deque.prototype.insert = function(t, i, r) {
        if (r === void 0) {
            r = 1;
        }
        var e = this.i;
        if (t < 0 || t > e) {
            throw new RangeError;
        }
        if (t === 0) {
            while (r--) this.pushFront(i);
        } else if (t === this.i) {
            while (r--) this.pushBack(i);
        } else {
            var s = [];
            for (var h = t; h < this.i; ++h) {
                s.push(this.getElementByPos(h));
            }
            this.cut(t - 1);
            for (var h = 0; h < r; ++h) this.pushBack(i);
            for (var h = 0; h < s.length; ++h) this.pushBack(s[h]);
        }
        return this.i;
    };
    Deque.prototype.cut = function(t) {
        if (t < 0) {
            this.clear();
            return 0;
        }
        var i = this.K(t), r = i.curNodeBucketIndex, e = i.curNodePointerIndex;
        this.D = r;
        this.R = e;
        this.i = t + 1;
        return this.i;
    };
    Deque.prototype.eraseElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.i - 1) this.popBack(); else {
            var i = this.i - 1;
            var r = this.K(t), e = r.curNodeBucketIndex, s = r.curNodePointerIndex;
            for (var h = t; h < i; ++h) {
                var n = this.K(t + 1), u = n.curNodeBucketIndex, o = n.curNodePointerIndex;
                this.G[e][s] = this.G[u][o];
                e = u;
                s = o;
            }
            this.popBack();
        }
        return this.i;
    };
    Deque.prototype.eraseElementByValue = function(t) {
        var i = this.i;
        if (i === 0) return 0;
        var r = 0;
        var e = 0;
        while (r < i) {
            var s = this.getElementByPos(r);
            if (s !== t) {
                this.setElementByPos(e, s);
                e += 1;
            }
            r += 1;
        }
        this.cut(e - 1);
        return this.i;
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
                return new DequeIterator(i, this);
            }
        }
        return this.end();
    };
    Deque.prototype.reverse = function() {
        this.G.reverse().forEach((function(t) {
            t.reverse();
        }));
        var t = this, i = t.C, r = t.D, e = t.q, s = t.R;
        this.C = this.N - r - 1;
        this.D = this.N - i - 1;
        this.q = this.F - s - 1;
        this.R = this.F - e - 1;
        return this;
    };
    Deque.prototype.unique = function() {
        if (this.i <= 1) {
            return this.i;
        }
        var t = 1;
        var i = this.getElementByPos(0);
        for (var r = 1; r < this.i; ++r) {
            var e = this.getElementByPos(r);
            if (e !== i) {
                i = e;
                this.setElementByPos(t++, e);
            }
        }
        this.cut(t - 1);
        return this.i;
    };
    Deque.prototype.sort = function(t) {
        var i = [];
        for (var r = 0; r < this.i; ++r) {
            i.push(this.getElementByPos(r));
        }
        i.sort(t);
        for (var r = 0; r < this.i; ++r) {
            this.setElementByPos(r, i[r]);
        }
        return this;
    };
    Deque.prototype.shrinkToFit = function() {
        if (this.i === 0) return;
        var t = [];
        if (this.C === this.D) return; else if (this.C < this.D) {
            for (var i = this.C; i <= this.D; ++i) {
                t.push(this.G[i]);
            }
        } else {
            for (var i = this.C; i < this.N; ++i) {
                t.push(this.G[i]);
            }
            for (var i = 0; i <= this.D; ++i) {
                t.push(this.G[i]);
            }
        }
        this.C = 0;
        this.D = t.length - 1;
        this.G = t;
    };
    Deque.prototype.forEach = function(t) {
        for (var i = 0; i < this.i; ++i) {
            t(this.getElementByPos(i), i, this);
        }
    };
    Deque.prototype[Symbol.iterator] = function() {
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
    };
    return Deque;
}(SequentialContainer);

export default Deque;
//# sourceMappingURL=Deque.js.map
