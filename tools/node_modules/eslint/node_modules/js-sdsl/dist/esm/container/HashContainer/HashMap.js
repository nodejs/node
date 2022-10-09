var __extends = this && this.t || function() {
    var extendStatics = function(r, e) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(r, e) {
            r.__proto__ = e;
        } || function(r, e) {
            for (var t in e) if (Object.prototype.hasOwnProperty.call(e, t)) r[t] = e[t];
        };
        return extendStatics(r, e);
    };
    return function(r, e) {
        if (typeof e !== "function" && e !== null) throw new TypeError("Class extends value " + String(e) + " is not a constructor or null");
        extendStatics(r, e);
        function __() {
            this.constructor = r;
        }
        r.prototype = e === null ? Object.create(e) : (__.prototype = e.prototype, new __);
    };
}();

var __generator = this && this.i || function(r, e) {
    var t = {
        label: 0,
        sent: function() {
            if (a[0] & 1) throw a[1];
            return a[1];
        },
        trys: [],
        ops: []
    }, n, i, a, f;
    return f = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (f[Symbol.iterator] = function() {
        return this;
    }), f;
    function verb(r) {
        return function(e) {
            return step([ r, e ]);
        };
    }
    function step(f) {
        if (n) throw new TypeError("Generator is already executing.");
        while (t) try {
            if (n = 1, i && (a = f[0] & 2 ? i["return"] : f[0] ? i["throw"] || ((a = i["return"]) && a.call(i), 
            0) : i.next) && !(a = a.call(i, f[1])).done) return a;
            if (i = 0, a) f = [ f[0] & 2, a.value ];
            switch (f[0]) {
              case 0:
              case 1:
                a = f;
                break;

              case 4:
                t.label++;
                return {
                    value: f[1],
                    done: false
                };

              case 5:
                t.label++;
                i = f[1];
                f = [ 0 ];
                continue;

              case 7:
                f = t.ops.pop();
                t.trys.pop();
                continue;

              default:
                if (!(a = t.trys, a = a.length > 0 && a[a.length - 1]) && (f[0] === 6 || f[0] === 2)) {
                    t = 0;
                    continue;
                }
                if (f[0] === 3 && (!a || f[1] > a[0] && f[1] < a[3])) {
                    t.label = f[1];
                    break;
                }
                if (f[0] === 6 && t.label < a[1]) {
                    t.label = a[1];
                    a = f;
                    break;
                }
                if (a && t.label < a[2]) {
                    t.label = a[2];
                    t.ops.push(f);
                    break;
                }
                if (a[2]) t.ops.pop();
                t.trys.pop();
                continue;
            }
            f = e.call(r, t);
        } catch (r) {
            f = [ 6, r ];
            i = 0;
        } finally {
            n = a = 0;
        }
        if (f[0] & 5) throw f[1];
        return {
            value: f[0] ? f[1] : void 0,
            done: true
        };
    }
};

var __values = this && this.u || function(r) {
    var e = typeof Symbol === "function" && Symbol.iterator, t = e && r[e], n = 0;
    if (t) return t.call(r);
    if (r && typeof r.length === "number") return {
        next: function() {
            if (r && n >= r.length) r = void 0;
            return {
                value: r && r[n++],
                done: !r
            };
        }
    };
    throw new TypeError(e ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import HashContainer from "./Base";

import Vector from "../SequentialContainer/Vector";

import OrderedMap from "../TreeContainer/OrderedMap";

var HashMap = function(r) {
    __extends(HashMap, r);
    function HashMap(e, t, n) {
        if (e === void 0) {
            e = [];
        }
        var i = r.call(this, t, n) || this;
        i.h = [];
        e.forEach((function(r) {
            return i.setElement(r[0], r[1]);
        }));
        return i;
    }
    HashMap.prototype.v = function() {
        var r = this;
        if (this.l >= 1073741824) return;
        var e = [];
        var t = this.l;
        this.l <<= 1;
        var n = Object.keys(this.h);
        var i = n.length;
        var _loop_1 = function(i) {
            var f = parseInt(n[i]);
            var s = a.h[f];
            var o = s.size();
            if (o === 0) return "continue";
            if (o === 1) {
                var u = s.front();
                e[a.p(u[0]) & a.l - 1] = new Vector([ u ], false);
                return "continue";
            }
            var c = [];
            var h = [];
            s.forEach((function(e) {
                var n = r.p(e[0]);
                if ((n & t) === 0) {
                    c.push(e);
                } else h.push(e);
            }));
            if (s instanceof OrderedMap) {
                if (c.length > 6) {
                    e[f] = new OrderedMap(c);
                } else {
                    e[f] = new Vector(c, false);
                }
                if (h.length > 6) {
                    e[f + t] = new OrderedMap(h);
                } else {
                    e[f + t] = new Vector(h, false);
                }
            } else {
                e[f] = new Vector(c, false);
                e[f + t] = new Vector(h, false);
            }
        };
        var a = this;
        for (var f = 0; f < i; ++f) {
            _loop_1(f);
        }
        this.h = e;
    };
    HashMap.prototype.forEach = function(r) {
        var e = Object.values(this.h);
        var t = e.length;
        var n = 0;
        for (var i = 0; i < t; ++i) {
            e[i].forEach((function(e) {
                return r(e, n++);
            }));
        }
    };
    HashMap.prototype.setElement = function(r, e) {
        var t, n;
        var i = this.p(r) & this.l - 1;
        var a = this.h[i];
        if (!a) {
            this.o += 1;
            this.h[i] = new Vector([ [ r, e ] ], false);
        } else {
            var f = a.size();
            if (a instanceof Vector) {
                try {
                    for (var s = __values(a), o = s.next(); !o.done; o = s.next()) {
                        var u = o.value;
                        if (u[0] === r) {
                            u[1] = e;
                            return;
                        }
                    }
                } catch (r) {
                    t = {
                        error: r
                    };
                } finally {
                    try {
                        if (o && !o.done && (n = s.return)) n.call(s);
                    } finally {
                        if (t) throw t.error;
                    }
                }
                a.pushBack([ r, e ]);
                if (f + 1 >= 8) {
                    if (this.l <= 64) {
                        this.o += 1;
                        this.v();
                        return;
                    }
                    this.h[i] = new OrderedMap(this.h[i]);
                }
                this.o += 1;
            } else {
                a.setElement(r, e);
                var c = a.size();
                this.o += c - f;
            }
        }
        if (this.o > this.l * .75) {
            this.v();
        }
    };
    HashMap.prototype.getElementByKey = function(r) {
        var e, t;
        var n = this.p(r) & this.l - 1;
        var i = this.h[n];
        if (!i) return undefined;
        if (i instanceof OrderedMap) {
            return i.getElementByKey(r);
        } else {
            try {
                for (var a = __values(i), f = a.next(); !f.done; f = a.next()) {
                    var s = f.value;
                    if (s[0] === r) return s[1];
                }
            } catch (r) {
                e = {
                    error: r
                };
            } finally {
                try {
                    if (f && !f.done && (t = a.return)) t.call(a);
                } finally {
                    if (e) throw e.error;
                }
            }
            return undefined;
        }
    };
    HashMap.prototype.eraseElementByKey = function(r) {
        var e, t;
        var n = this.p(r) & this.l - 1;
        var i = this.h[n];
        if (!i) return;
        if (i instanceof Vector) {
            var a = 0;
            try {
                for (var f = __values(i), s = f.next(); !s.done; s = f.next()) {
                    var o = s.value;
                    if (o[0] === r) {
                        i.eraseElementByPos(a);
                        this.o -= 1;
                        return;
                    }
                    a += 1;
                }
            } catch (r) {
                e = {
                    error: r
                };
            } finally {
                try {
                    if (s && !s.done && (t = f.return)) t.call(f);
                } finally {
                    if (e) throw e.error;
                }
            }
        } else {
            var u = i.size();
            i.eraseElementByKey(r);
            var c = i.size();
            this.o += c - u;
            if (c <= 6) {
                this.h[n] = new Vector(i);
            }
        }
    };
    HashMap.prototype.find = function(r) {
        var e, t;
        var n = this.p(r) & this.l - 1;
        var i = this.h[n];
        if (!i) return false;
        if (i instanceof OrderedMap) {
            return !i.find(r).equals(i.end());
        }
        try {
            for (var a = __values(i), f = a.next(); !f.done; f = a.next()) {
                var s = f.value;
                if (s[0] === r) return true;
            }
        } catch (r) {
            e = {
                error: r
            };
        } finally {
            try {
                if (f && !f.done && (t = a.return)) t.call(a);
            } finally {
                if (e) throw e.error;
            }
        }
        return false;
    };
    HashMap.prototype[Symbol.iterator] = function() {
        return function() {
            var r, e, t, n, i, a, f, s;
            var o, u;
            return __generator(this, (function(c) {
                switch (c.label) {
                  case 0:
                    r = Object.values(this.h);
                    e = r.length;
                    t = 0;
                    c.label = 1;

                  case 1:
                    if (!(t < e)) return [ 3, 10 ];
                    n = r[t];
                    c.label = 2;

                  case 2:
                    c.trys.push([ 2, 7, 8, 9 ]);
                    i = (o = void 0, __values(n)), a = i.next();
                    c.label = 3;

                  case 3:
                    if (!!a.done) return [ 3, 6 ];
                    f = a.value;
                    return [ 4, f ];

                  case 4:
                    c.sent();
                    c.label = 5;

                  case 5:
                    a = i.next();
                    return [ 3, 3 ];

                  case 6:
                    return [ 3, 9 ];

                  case 7:
                    s = c.sent();
                    o = {
                        error: s
                    };
                    return [ 3, 9 ];

                  case 8:
                    try {
                        if (a && !a.done && (u = i.return)) u.call(i);
                    } finally {
                        if (o) throw o.error;
                    }
                    return [ 7 ];

                  case 9:
                    ++t;
                    return [ 3, 1 ];

                  case 10:
                    return [ 2 ];
                }
            }));
        }.bind(this)();
    };
    return HashMap;
}(HashContainer);

export default HashMap;