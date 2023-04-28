var __extends = this && this.t || function() {
    var extendStatics = function(i, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(i, r) {
            i.__proto__ = r;
        } || function(i, r) {
            for (var t in r) if (Object.prototype.hasOwnProperty.call(r, t)) i[t] = r[t];
        };
        return extendStatics(i, r);
    };
    return function(i, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(i, r);
        function __() {
            this.constructor = i;
        }
        i.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

var __read = this && this.P || function(i, r) {
    var t = typeof Symbol === "function" && i[Symbol.iterator];
    if (!t) return i;
    var e = t.call(i), n, u = [], s;
    try {
        while ((r === void 0 || r-- > 0) && !(n = e.next()).done) u.push(n.value);
    } catch (i) {
        s = {
            error: i
        };
    } finally {
        try {
            if (n && !n.done && (t = e["return"])) t.call(e);
        } finally {
            if (s) throw s.error;
        }
    }
    return u;
};

var __spreadArray = this && this.A || function(i, r, t) {
    if (t || arguments.length === 2) for (var e = 0, n = r.length, u; e < n; e++) {
        if (u || !(e in r)) {
            if (!u) u = Array.prototype.slice.call(r, 0, e);
            u[e] = r[e];
        }
    }
    return i.concat(u || Array.prototype.slice.call(r));
};

import { Base } from "../ContainerBase";

var PriorityQueue = function(i) {
    __extends(PriorityQueue, i);
    function PriorityQueue(r, t, e) {
        if (r === void 0) {
            r = [];
        }
        if (t === void 0) {
            t = function(i, r) {
                if (i > r) return -1;
                if (i < r) return 1;
                return 0;
            };
        }
        if (e === void 0) {
            e = true;
        }
        var n = i.call(this) || this;
        n.j = t;
        if (Array.isArray(r)) {
            n.B = e ? __spreadArray([], __read(r), false) : r;
        } else {
            n.B = [];
            var u = n;
            r.forEach((function(i) {
                u.B.push(i);
            }));
        }
        n.i = n.B.length;
        var s = n.i >> 1;
        for (var o = n.i - 1 >> 1; o >= 0; --o) {
            n.O(o, s);
        }
        return n;
    }
    PriorityQueue.prototype.S = function(i) {
        var r = this.B[i];
        while (i > 0) {
            var t = i - 1 >> 1;
            var e = this.B[t];
            if (this.j(e, r) <= 0) break;
            this.B[i] = e;
            i = t;
        }
        this.B[i] = r;
    };
    PriorityQueue.prototype.O = function(i, r) {
        var t = this.B[i];
        while (i < r) {
            var e = i << 1 | 1;
            var n = e + 1;
            var u = this.B[e];
            if (n < this.i && this.j(u, this.B[n]) > 0) {
                e = n;
                u = this.B[n];
            }
            if (this.j(u, t) >= 0) break;
            this.B[i] = u;
            i = e;
        }
        this.B[i] = t;
    };
    PriorityQueue.prototype.clear = function() {
        this.i = 0;
        this.B.length = 0;
    };
    PriorityQueue.prototype.push = function(i) {
        this.B.push(i);
        this.S(this.i);
        this.i += 1;
    };
    PriorityQueue.prototype.pop = function() {
        if (this.i === 0) return;
        var i = this.B[0];
        var r = this.B.pop();
        this.i -= 1;
        if (this.i) {
            this.B[0] = r;
            this.O(0, this.i >> 1);
        }
        return i;
    };
    PriorityQueue.prototype.top = function() {
        return this.B[0];
    };
    PriorityQueue.prototype.find = function(i) {
        return this.B.indexOf(i) >= 0;
    };
    PriorityQueue.prototype.remove = function(i) {
        var r = this.B.indexOf(i);
        if (r < 0) return false;
        if (r === 0) {
            this.pop();
        } else if (r === this.i - 1) {
            this.B.pop();
            this.i -= 1;
        } else {
            this.B.splice(r, 1, this.B.pop());
            this.i -= 1;
            this.S(r);
            this.O(r, this.i >> 1);
        }
        return true;
    };
    PriorityQueue.prototype.updateItem = function(i) {
        var r = this.B.indexOf(i);
        if (r < 0) return false;
        this.S(r);
        this.O(r, this.i >> 1);
        return true;
    };
    PriorityQueue.prototype.toArray = function() {
        return __spreadArray([], __read(this.B), false);
    };
    return PriorityQueue;
}(Base);

export default PriorityQueue;
//# sourceMappingURL=PriorityQueue.js.map
