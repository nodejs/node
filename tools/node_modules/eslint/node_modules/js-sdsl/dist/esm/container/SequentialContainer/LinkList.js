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
            if (e[0] & 1) throw e[1];
            return e[1];
        },
        trys: [],
        ops: []
    }, n, s, e, h;
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
        if (n) throw new TypeError("Generator is already executing.");
        while (r) try {
            if (n = 1, s && (e = h[0] & 2 ? s["return"] : h[0] ? s["throw"] || ((e = s["return"]) && e.call(s), 
            0) : s.next) && !(e = e.call(s, h[1])).done) return e;
            if (s = 0, e) h = [ h[0] & 2, e.value ];
            switch (h[0]) {
              case 0:
              case 1:
                e = h;
                break;

              case 4:
                r.label++;
                return {
                    value: h[1],
                    done: false
                };

              case 5:
                r.label++;
                s = h[1];
                h = [ 0 ];
                continue;

              case 7:
                h = r.ops.pop();
                r.trys.pop();
                continue;

              default:
                if (!(e = r.trys, e = e.length > 0 && e[e.length - 1]) && (h[0] === 6 || h[0] === 2)) {
                    r = 0;
                    continue;
                }
                if (h[0] === 3 && (!e || h[1] > e[0] && h[1] < e[3])) {
                    r.label = h[1];
                    break;
                }
                if (h[0] === 6 && r.label < e[1]) {
                    r.label = e[1];
                    e = h;
                    break;
                }
                if (e && r.label < e[2]) {
                    r.label = e[2];
                    r.ops.push(h);
                    break;
                }
                if (e[2]) r.ops.pop();
                r.trys.pop();
                continue;
            }
            h = i.call(t, r);
        } catch (t) {
            h = [ 6, t ];
            s = 0;
        } finally {
            n = e = 0;
        }
        if (h[0] & 5) throw h[1];
        return {
            value: h[0] ? h[1] : void 0,
            done: true
        };
    }
};

import SequentialContainer from "./Base";

import { ContainerIterator } from "../ContainerBase";

import { throwIteratorAccessError } from "../../utils/throwError";

var LinkListIterator = function(t) {
    __extends(LinkListIterator, t);
    function LinkListIterator(i, r, n, s) {
        var e = t.call(this, s) || this;
        e.o = i;
        e.u = r;
        e.container = n;
        if (e.iteratorType === 0) {
            e.pre = function() {
                if (this.o.L === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.L;
                return this;
            };
            e.next = function() {
                if (this.o === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m;
                return this;
            };
        } else {
            e.pre = function() {
                if (this.o.m === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m;
                return this;
            };
            e.next = function() {
                if (this.o === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.L;
                return this;
            };
        }
        return e;
    }
    Object.defineProperty(LinkListIterator.prototype, "pointer", {
        get: function() {
            if (this.o === this.u) {
                throwIteratorAccessError();
            }
            return this.o.H;
        },
        set: function(t) {
            if (this.o === this.u) {
                throwIteratorAccessError();
            }
            this.o.H = t;
        },
        enumerable: false,
        configurable: true
    });
    LinkListIterator.prototype.copy = function() {
        return new LinkListIterator(this.o, this.u, this.container, this.iteratorType);
    };
    return LinkListIterator;
}(ContainerIterator);

var LinkList = function(t) {
    __extends(LinkList, t);
    function LinkList(i) {
        if (i === void 0) {
            i = [];
        }
        var r = t.call(this) || this;
        r.u = {};
        r.l = r.M = r.u.L = r.u.m = r.u;
        var n = r;
        i.forEach((function(t) {
            n.pushBack(t);
        }));
        return r;
    }
    LinkList.prototype.U = function(t) {
        var i = t.L, r = t.m;
        i.m = r;
        r.L = i;
        if (t === this.l) {
            this.l = r;
        }
        if (t === this.M) {
            this.M = i;
        }
        this.i -= 1;
    };
    LinkList.prototype.V = function(t, i) {
        var r = i.m;
        var n = {
            H: t,
            L: i,
            m: r
        };
        i.m = n;
        r.L = n;
        if (i === this.u) {
            this.l = n;
        }
        if (r === this.u) {
            this.M = n;
        }
        this.i += 1;
    };
    LinkList.prototype.clear = function() {
        this.i = 0;
        this.l = this.M = this.u.L = this.u.m = this.u;
    };
    LinkList.prototype.begin = function() {
        return new LinkListIterator(this.l, this.u, this);
    };
    LinkList.prototype.end = function() {
        return new LinkListIterator(this.u, this.u, this);
    };
    LinkList.prototype.rBegin = function() {
        return new LinkListIterator(this.M, this.u, this, 1);
    };
    LinkList.prototype.rEnd = function() {
        return new LinkListIterator(this.u, this.u, this, 1);
    };
    LinkList.prototype.front = function() {
        return this.l.H;
    };
    LinkList.prototype.back = function() {
        return this.M.H;
    };
    LinkList.prototype.getElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var i = this.l;
        while (t--) {
            i = i.m;
        }
        return i.H;
    };
    LinkList.prototype.eraseElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var i = this.l;
        while (t--) {
            i = i.m;
        }
        this.U(i);
        return this.i;
    };
    LinkList.prototype.eraseElementByValue = function(t) {
        var i = this.l;
        while (i !== this.u) {
            if (i.H === t) {
                this.U(i);
            }
            i = i.m;
        }
        return this.i;
    };
    LinkList.prototype.eraseElementByIterator = function(t) {
        var i = t.o;
        if (i === this.u) {
            throwIteratorAccessError();
        }
        t = t.next();
        this.U(i);
        return t;
    };
    LinkList.prototype.pushBack = function(t) {
        this.V(t, this.M);
        return this.i;
    };
    LinkList.prototype.popBack = function() {
        if (this.i === 0) return;
        var t = this.M.H;
        this.U(this.M);
        return t;
    };
    LinkList.prototype.pushFront = function(t) {
        this.V(t, this.u);
        return this.i;
    };
    LinkList.prototype.popFront = function() {
        if (this.i === 0) return;
        var t = this.l.H;
        this.U(this.l);
        return t;
    };
    LinkList.prototype.setElementByPos = function(t, i) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var r = this.l;
        while (t--) {
            r = r.m;
        }
        r.H = i;
    };
    LinkList.prototype.insert = function(t, i, r) {
        if (r === void 0) {
            r = 1;
        }
        if (t < 0 || t > this.i) {
            throw new RangeError;
        }
        if (r <= 0) return this.i;
        if (t === 0) {
            while (r--) this.pushFront(i);
        } else if (t === this.i) {
            while (r--) this.pushBack(i);
        } else {
            var n = this.l;
            for (var s = 1; s < t; ++s) {
                n = n.m;
            }
            var e = n.m;
            this.i += r;
            while (r--) {
                n.m = {
                    H: i,
                    L: n
                };
                n.m.L = n;
                n = n.m;
            }
            n.m = e;
            e.L = n;
        }
        return this.i;
    };
    LinkList.prototype.find = function(t) {
        var i = this.l;
        while (i !== this.u) {
            if (i.H === t) {
                return new LinkListIterator(i, this.u, this);
            }
            i = i.m;
        }
        return this.end();
    };
    LinkList.prototype.reverse = function() {
        if (this.i <= 1) {
            return this;
        }
        var t = this.l;
        var i = this.M;
        var r = 0;
        while (r << 1 < this.i) {
            var n = t.H;
            t.H = i.H;
            i.H = n;
            t = t.m;
            i = i.L;
            r += 1;
        }
        return this;
    };
    LinkList.prototype.unique = function() {
        if (this.i <= 1) {
            return this.i;
        }
        var t = this.l;
        while (t !== this.u) {
            var i = t;
            while (i.m !== this.u && i.H === i.m.H) {
                i = i.m;
                this.i -= 1;
            }
            t.m = i.m;
            t.m.L = t;
            t = t.m;
        }
        return this.i;
    };
    LinkList.prototype.sort = function(t) {
        if (this.i <= 1) {
            return this;
        }
        var i = [];
        this.forEach((function(t) {
            i.push(t);
        }));
        i.sort(t);
        var r = this.l;
        i.forEach((function(t) {
            r.H = t;
            r = r.m;
        }));
        return this;
    };
    LinkList.prototype.merge = function(t) {
        var i = this;
        if (this.i === 0) {
            t.forEach((function(t) {
                i.pushBack(t);
            }));
        } else {
            var r = this.l;
            t.forEach((function(t) {
                while (r !== i.u && r.H <= t) {
                    r = r.m;
                }
                i.V(t, r.L);
            }));
        }
        return this.i;
    };
    LinkList.prototype.forEach = function(t) {
        var i = this.l;
        var r = 0;
        while (i !== this.u) {
            t(i.H, r++, this);
            i = i.m;
        }
    };
    LinkList.prototype[Symbol.iterator] = function() {
        var t;
        return __generator(this, (function(i) {
            switch (i.label) {
              case 0:
                if (this.i === 0) return [ 2 ];
                t = this.l;
                i.label = 1;

              case 1:
                if (!(t !== this.u)) return [ 3, 3 ];
                return [ 4, t.H ];

              case 2:
                i.sent();
                t = t.m;
                return [ 3, 1 ];

              case 3:
                return [ 2 ];
            }
        }));
    };
    return LinkList;
}(SequentialContainer);

export default LinkList;
//# sourceMappingURL=LinkList.js.map
