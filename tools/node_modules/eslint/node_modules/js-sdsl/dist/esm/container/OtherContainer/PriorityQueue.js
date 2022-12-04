var __extends = this && this.t || function() {
    var extendStatics = function(i, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(i, t) {
            i.__proto__ = t;
        } || function(i, t) {
            for (var r in t) if (Object.prototype.hasOwnProperty.call(t, r)) i[r] = t[r];
        };
        return extendStatics(i, t);
    };
    return function(i, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(i, t);
        function __() {
            this.constructor = i;
        }
        i.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

var __read = this && this._ || function(i, t) {
    var r = typeof Symbol === "function" && i[Symbol.iterator];
    if (!r) return i;
    var e = r.call(i), n, u = [], s;
    try {
        while ((t === void 0 || t-- > 0) && !(n = e.next()).done) u.push(n.value);
    } catch (i) {
        s = {
            error: i
        };
    } finally {
        try {
            if (n && !n.done && (r = e["return"])) r.call(e);
        } finally {
            if (s) throw s.error;
        }
    }
    return u;
};

var __spreadArray = this && this.P || function(i, t, r) {
    if (r || arguments.length === 2) for (var e = 0, n = t.length, u; e < n; e++) {
        if (u || !(e in t)) {
            if (!u) u = Array.prototype.slice.call(t, 0, e);
            u[e] = t[e];
        }
    }
    return i.concat(u || Array.prototype.slice.call(t));
};

import { Base } from "../ContainerBase";

var PriorityQueue = function(i) {
    __extends(PriorityQueue, i);
    function PriorityQueue(t, r, e) {
        if (t === void 0) {
            t = [];
        }
        if (r === void 0) {
            r = function(i, t) {
                if (i > t) return -1;
                if (i < t) return 1;
                return 0;
            };
        }
        if (e === void 0) {
            e = true;
        }
        var n = i.call(this) || this;
        n.A = r;
        if (Array.isArray(t)) {
            n.m = e ? __spreadArray([], __read(t), false) : t;
        } else {
            n.m = [];
            t.forEach((function(i) {
                return n.m.push(i);
            }));
        }
        n.o = n.m.length;
        var u = n.o >> 1;
        for (var s = n.o - 1 >> 1; s >= 0; --s) {
            n.j(s, u);
        }
        return n;
    }
    PriorityQueue.prototype.B = function(i) {
        var t = this.m[i];
        while (i > 0) {
            var r = i - 1 >> 1;
            var e = this.m[r];
            if (this.A(e, t) <= 0) break;
            this.m[i] = e;
            i = r;
        }
        this.m[i] = t;
    };
    PriorityQueue.prototype.j = function(i, t) {
        var r = this.m[i];
        while (i < t) {
            var e = i << 1 | 1;
            var n = e + 1;
            var u = this.m[e];
            if (n < this.o && this.A(u, this.m[n]) > 0) {
                e = n;
                u = this.m[n];
            }
            if (this.A(u, r) >= 0) break;
            this.m[i] = u;
            i = e;
        }
        this.m[i] = r;
    };
    PriorityQueue.prototype.clear = function() {
        this.o = 0;
        this.m.length = 0;
    };
    PriorityQueue.prototype.push = function(i) {
        this.m.push(i);
        this.B(this.o);
        this.o += 1;
    };
    PriorityQueue.prototype.pop = function() {
        if (!this.o) return;
        var i = this.m.pop();
        this.o -= 1;
        if (this.o) {
            this.m[0] = i;
            this.j(0, this.o >> 1);
        }
    };
    PriorityQueue.prototype.top = function() {
        return this.m[0];
    };
    PriorityQueue.prototype.find = function(i) {
        return this.m.indexOf(i) >= 0;
    };
    PriorityQueue.prototype.remove = function(i) {
        var t = this.m.indexOf(i);
        if (t < 0) return false;
        if (t === 0) {
            this.pop();
        } else if (t === this.o - 1) {
            this.m.pop();
            this.o -= 1;
        } else {
            this.m.splice(t, 1, this.m.pop());
            this.o -= 1;
            this.B(t);
            this.j(t, this.o >> 1);
        }
        return true;
    };
    PriorityQueue.prototype.updateItem = function(i) {
        var t = this.m.indexOf(i);
        if (t < 0) return false;
        this.B(t);
        this.j(t, this.o >> 1);
        return true;
    };
    PriorityQueue.prototype.toArray = function() {
        return __spreadArray([], __read(this.m), false);
    };
    return PriorityQueue;
}(Base);

export default PriorityQueue;