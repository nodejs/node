var __extends = this && this.t || function() {
    var extendStatics = function(e, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, t) {
            e.__proto__ = t;
        } || function(e, t) {
            for (var r in t) if (Object.prototype.hasOwnProperty.call(t, r)) e[r] = t[r];
        };
        return extendStatics(e, t);
    };
    return function(e, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(e, t);
        function __() {
            this.constructor = e;
        }
        e.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

var __generator = this && this.i || function(e, t) {
    var r = {
        label: 0,
        sent: function() {
            if (a[0] & 1) throw a[1];
            return a[1];
        },
        trys: [],
        ops: []
    }, n, i, a, s;
    return s = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (s[Symbol.iterator] = function() {
        return this;
    }), s;
    function verb(e) {
        return function(t) {
            return step([ e, t ]);
        };
    }
    function step(s) {
        if (n) throw new TypeError("Generator is already executing.");
        while (r) try {
            if (n = 1, i && (a = s[0] & 2 ? i["return"] : s[0] ? i["throw"] || ((a = i["return"]) && a.call(i), 
            0) : i.next) && !(a = a.call(i, s[1])).done) return a;
            if (i = 0, a) s = [ s[0] & 2, a.value ];
            switch (s[0]) {
              case 0:
              case 1:
                a = s;
                break;

              case 4:
                r.label++;
                return {
                    value: s[1],
                    done: false
                };

              case 5:
                r.label++;
                i = s[1];
                s = [ 0 ];
                continue;

              case 7:
                s = r.ops.pop();
                r.trys.pop();
                continue;

              default:
                if (!(a = r.trys, a = a.length > 0 && a[a.length - 1]) && (s[0] === 6 || s[0] === 2)) {
                    r = 0;
                    continue;
                }
                if (s[0] === 3 && (!a || s[1] > a[0] && s[1] < a[3])) {
                    r.label = s[1];
                    break;
                }
                if (s[0] === 6 && r.label < a[1]) {
                    r.label = a[1];
                    a = s;
                    break;
                }
                if (a && r.label < a[2]) {
                    r.label = a[2];
                    r.ops.push(s);
                    break;
                }
                if (a[2]) r.ops.pop();
                r.trys.pop();
                continue;
            }
            s = t.call(e, r);
        } catch (e) {
            s = [ 6, e ];
            i = 0;
        } finally {
            n = a = 0;
        }
        if (s[0] & 5) throw s[1];
        return {
            value: s[0] ? s[1] : void 0,
            done: true
        };
    }
};

var __values = this && this.u || function(e) {
    var t = typeof Symbol === "function" && Symbol.iterator, r = t && e[t], n = 0;
    if (r) return r.call(e);
    if (e && typeof e.length === "number") return {
        next: function() {
            if (e && n >= e.length) e = void 0;
            return {
                value: e && e[n++],
                done: !e
            };
        }
    };
    throw new TypeError(t ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import HashContainer from "./Base";

import Vector from "../SequentialContainer/Vector";

import OrderedSet from "../TreeContainer/OrderedSet";

var HashSet = function(e) {
    __extends(HashSet, e);
    function HashSet(t, r, n) {
        if (t === void 0) {
            t = [];
        }
        var i = e.call(this, r, n) || this;
        i.h = [];
        t.forEach((function(e) {
            return i.insert(e);
        }));
        return i;
    }
    HashSet.prototype.v = function() {
        var e = this;
        if (this.l >= 1073741824) return;
        var t = [];
        var r = this.l;
        this.l <<= 1;
        var n = Object.keys(this.h);
        var i = n.length;
        var _loop_1 = function(i) {
            var s = parseInt(n[i]);
            var o = a.h[s];
            var f = o.size();
            if (f === 0) return "continue";
            if (f === 1) {
                var u = o.front();
                t[a.p(u) & a.l - 1] = new Vector([ u ], false);
                return "continue";
            }
            var c = [];
            var h = [];
            o.forEach((function(t) {
                var n = e.p(t);
                if ((n & r) === 0) {
                    c.push(t);
                } else h.push(t);
            }));
            if (o instanceof OrderedSet) {
                if (c.length > 6) {
                    t[s] = new OrderedSet(c);
                } else {
                    t[s] = new Vector(c, false);
                }
                if (h.length > 6) {
                    t[s + r] = new OrderedSet(h);
                } else {
                    t[s + r] = new Vector(h, false);
                }
            } else {
                t[s] = new Vector(c, false);
                t[s + r] = new Vector(h, false);
            }
        };
        var a = this;
        for (var s = 0; s < i; ++s) {
            _loop_1(s);
        }
        this.h = t;
    };
    HashSet.prototype.forEach = function(e) {
        var t = Object.values(this.h);
        var r = t.length;
        var n = 0;
        for (var i = 0; i < r; ++i) {
            t[i].forEach((function(t) {
                return e(t, n++);
            }));
        }
    };
    HashSet.prototype.insert = function(e) {
        var t = this.p(e) & this.l - 1;
        var r = this.h[t];
        if (!r) {
            this.h[t] = new Vector([ e ], false);
            this.o += 1;
        } else {
            var n = r.size();
            if (r instanceof Vector) {
                if (!r.find(e).equals(r.end())) return;
                r.pushBack(e);
                if (n + 1 >= 8) {
                    if (this.l <= 64) {
                        this.o += 1;
                        this.v();
                        return;
                    }
                    this.h[t] = new OrderedSet(r);
                }
                this.o += 1;
            } else {
                r.insert(e);
                var i = r.size();
                this.o += i - n;
            }
        }
        if (this.o > this.l * .75) {
            this.v();
        }
    };
    HashSet.prototype.eraseElementByKey = function(e) {
        var t = this.p(e) & this.l - 1;
        var r = this.h[t];
        if (!r) return;
        var n = r.size();
        if (n === 0) return;
        if (r instanceof Vector) {
            r.eraseElementByValue(e);
            var i = r.size();
            this.o += i - n;
        } else {
            r.eraseElementByKey(e);
            var i = r.size();
            this.o += i - n;
            if (i <= 6) {
                this.h[t] = new Vector(r);
            }
        }
    };
    HashSet.prototype.find = function(e) {
        var t = this.p(e) & this.l - 1;
        var r = this.h[t];
        if (!r) return false;
        return !r.find(e).equals(r.end());
    };
    HashSet.prototype[Symbol.iterator] = function() {
        return function() {
            var e, t, r, n, i, a, s, o;
            var f, u;
            return __generator(this, (function(c) {
                switch (c.label) {
                  case 0:
                    e = Object.values(this.h);
                    t = e.length;
                    r = 0;
                    c.label = 1;

                  case 1:
                    if (!(r < t)) return [ 3, 10 ];
                    n = e[r];
                    c.label = 2;

                  case 2:
                    c.trys.push([ 2, 7, 8, 9 ]);
                    i = (f = void 0, __values(n)), a = i.next();
                    c.label = 3;

                  case 3:
                    if (!!a.done) return [ 3, 6 ];
                    s = a.value;
                    return [ 4, s ];

                  case 4:
                    c.sent();
                    c.label = 5;

                  case 5:
                    a = i.next();
                    return [ 3, 3 ];

                  case 6:
                    return [ 3, 9 ];

                  case 7:
                    o = c.sent();
                    f = {
                        error: o
                    };
                    return [ 3, 9 ];

                  case 8:
                    try {
                        if (a && !a.done && (u = i.return)) u.call(i);
                    } finally {
                        if (f) throw f.error;
                    }
                    return [ 7 ];

                  case 9:
                    ++r;
                    return [ 3, 1 ];

                  case 10:
                    return [ 2 ];
                }
            }));
        }.bind(this)();
    };
    return HashSet;
}(HashContainer);

export default HashSet;