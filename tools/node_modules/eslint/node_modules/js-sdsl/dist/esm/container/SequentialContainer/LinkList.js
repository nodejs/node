var __extends = this && this.t || function() {
    var extendStatics = function(i, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(i, t) {
            i.__proto__ = t;
        } || function(i, t) {
            for (var n in t) if (Object.prototype.hasOwnProperty.call(t, n)) i[n] = t[n];
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

var __generator = this && this.i || function(i, t) {
    var n = {
        label: 0,
        sent: function() {
            if (s[0] & 1) throw s[1];
            return s[1];
        },
        trys: [],
        ops: []
    }, r, e, s, h;
    return h = {
        next: verb(0),
        throw: verb(1),
        return: verb(2)
    }, typeof Symbol === "function" && (h[Symbol.iterator] = function() {
        return this;
    }), h;
    function verb(i) {
        return function(t) {
            return step([ i, t ]);
        };
    }
    function step(h) {
        if (r) throw new TypeError("Generator is already executing.");
        while (n) try {
            if (r = 1, e && (s = h[0] & 2 ? e["return"] : h[0] ? e["throw"] || ((s = e["return"]) && s.call(e), 
            0) : e.next) && !(s = s.call(e, h[1])).done) return s;
            if (e = 0, s) h = [ h[0] & 2, s.value ];
            switch (h[0]) {
              case 0:
              case 1:
                s = h;
                break;

              case 4:
                n.label++;
                return {
                    value: h[1],
                    done: false
                };

              case 5:
                n.label++;
                e = h[1];
                h = [ 0 ];
                continue;

              case 7:
                h = n.ops.pop();
                n.trys.pop();
                continue;

              default:
                if (!(s = n.trys, s = s.length > 0 && s[s.length - 1]) && (h[0] === 6 || h[0] === 2)) {
                    n = 0;
                    continue;
                }
                if (h[0] === 3 && (!s || h[1] > s[0] && h[1] < s[3])) {
                    n.label = h[1];
                    break;
                }
                if (h[0] === 6 && n.label < s[1]) {
                    n.label = s[1];
                    s = h;
                    break;
                }
                if (s && n.label < s[2]) {
                    n.label = s[2];
                    n.ops.push(h);
                    break;
                }
                if (s[2]) n.ops.pop();
                n.trys.pop();
                continue;
            }
            h = t.call(i, n);
        } catch (i) {
            h = [ 6, i ];
            e = 0;
        } finally {
            r = s = 0;
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

var LinkNode = function() {
    function LinkNode(i) {
        this.L = undefined;
        this.F = undefined;
        this.H = undefined;
        this.L = i;
    }
    return LinkNode;
}();

export { LinkNode };

var LinkListIterator = function(i) {
    __extends(LinkListIterator, i);
    function LinkListIterator(t, n, r) {
        var e = i.call(this, r) || this;
        e.D = t;
        e.J = n;
        if (e.iteratorType === 0) {
            e.pre = function() {
                if (this.D.F === this.J) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.D = this.D.F;
                return this;
            };
            e.next = function() {
                if (this.D === this.J) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.D = this.D.H;
                return this;
            };
        } else {
            e.pre = function() {
                if (this.D.H === this.J) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.D = this.D.H;
                return this;
            };
            e.next = function() {
                if (this.D === this.J) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.D = this.D.F;
                return this;
            };
        }
        return e;
    }
    Object.defineProperty(LinkListIterator.prototype, "pointer", {
        get: function() {
            if (this.D === this.J) {
                throw new RangeError("LinkList iterator access denied!");
            }
            return this.D.L;
        },
        set: function(i) {
            if (this.D === this.J) {
                throw new RangeError("LinkList iterator access denied!");
            }
            this.D.L = i;
        },
        enumerable: false,
        configurable: true
    });
    LinkListIterator.prototype.equals = function(i) {
        return this.D === i.D;
    };
    LinkListIterator.prototype.copy = function() {
        return new LinkListIterator(this.D, this.J, this.iteratorType);
    };
    return LinkListIterator;
}(ContainerIterator);

export { LinkListIterator };

var LinkList = function(i) {
    __extends(LinkList, i);
    function LinkList(t) {
        if (t === void 0) {
            t = [];
        }
        var n = i.call(this) || this;
        n.J = new LinkNode;
        n.K = undefined;
        n.U = undefined;
        t.forEach((function(i) {
            return n.pushBack(i);
        }));
        return n;
    }
    LinkList.prototype.clear = function() {
        this.o = 0;
        this.K = this.U = undefined;
        this.J.F = this.J.H = undefined;
    };
    LinkList.prototype.begin = function() {
        return new LinkListIterator(this.K || this.J, this.J);
    };
    LinkList.prototype.end = function() {
        return new LinkListIterator(this.J, this.J);
    };
    LinkList.prototype.rBegin = function() {
        return new LinkListIterator(this.U || this.J, this.J, 1);
    };
    LinkList.prototype.rEnd = function() {
        return new LinkListIterator(this.J, this.J, 1);
    };
    LinkList.prototype.front = function() {
        return this.K ? this.K.L : undefined;
    };
    LinkList.prototype.back = function() {
        return this.U ? this.U.L : undefined;
    };
    LinkList.prototype.forEach = function(i) {
        if (!this.o) return;
        var t = this.K;
        var n = 0;
        while (t !== this.J) {
            i(t.L, n++);
            t = t.H;
        }
    };
    LinkList.prototype.getElementByPos = function(i) {
        if (i < 0 || i > this.o - 1) {
            throw new RangeError;
        }
        var t = this.K;
        while (i--) {
            t = t.H;
        }
        return t.L;
    };
    LinkList.prototype.eraseElementByPos = function(i) {
        if (i < 0 || i > this.o - 1) {
            throw new RangeError;
        }
        if (i === 0) this.popFront(); else if (i === this.o - 1) this.popBack(); else {
            var t = this.K;
            while (i--) {
                t = t.H;
            }
            t = t;
            var n = t.F;
            var r = t.H;
            r.F = n;
            n.H = r;
            this.o -= 1;
        }
    };
    LinkList.prototype.eraseElementByValue = function(i) {
        while (this.K && this.K.L === i) this.popFront();
        while (this.U && this.U.L === i) this.popBack();
        if (!this.K) return;
        var t = this.K;
        while (t !== this.J) {
            if (t.L === i) {
                var n = t.F;
                var r = t.H;
                r.F = n;
                n.H = r;
                this.o -= 1;
            }
            t = t.H;
        }
    };
    LinkList.prototype.eraseElementByIterator = function(i) {
        var t = i.D;
        if (t === this.J) {
            throw new RangeError("Invalid iterator");
        }
        i = i.next();
        if (this.K === t) this.popFront(); else if (this.U === t) this.popBack(); else {
            var n = t.F;
            var r = t.H;
            r.F = n;
            n.H = r;
            this.o -= 1;
        }
        return i;
    };
    LinkList.prototype.pushBack = function(i) {
        this.o += 1;
        var t = new LinkNode(i);
        if (!this.U) {
            this.K = this.U = t;
            this.J.H = this.K;
            this.K.F = this.J;
        } else {
            this.U.H = t;
            t.F = this.U;
            this.U = t;
        }
        this.U.H = this.J;
        this.J.F = this.U;
    };
    LinkList.prototype.popBack = function() {
        if (!this.U) return;
        this.o -= 1;
        if (this.K === this.U) {
            this.K = this.U = undefined;
            this.J.H = undefined;
        } else {
            this.U = this.U.F;
            this.U.H = this.J;
        }
        this.J.F = this.U;
    };
    LinkList.prototype.setElementByPos = function(i, t) {
        if (i < 0 || i > this.o - 1) {
            throw new RangeError;
        }
        var n = this.K;
        while (i--) {
            n = n.H;
        }
        n.L = t;
    };
    LinkList.prototype.insert = function(i, t, n) {
        if (n === void 0) {
            n = 1;
        }
        if (i < 0 || i > this.o) {
            throw new RangeError;
        }
        if (n <= 0) return;
        if (i === 0) {
            while (n--) this.pushFront(t);
        } else if (i === this.o) {
            while (n--) this.pushBack(t);
        } else {
            var r = this.K;
            for (var e = 1; e < i; ++e) {
                r = r.H;
            }
            var s = r.H;
            this.o += n;
            while (n--) {
                r.H = new LinkNode(t);
                r.H.F = r;
                r = r.H;
            }
            r.H = s;
            s.F = r;
        }
    };
    LinkList.prototype.find = function(i) {
        if (!this.K) return this.end();
        var t = this.K;
        while (t !== this.J) {
            if (t.L === i) {
                return new LinkListIterator(t, this.J);
            }
            t = t.H;
        }
        return this.end();
    };
    LinkList.prototype.reverse = function() {
        if (this.o <= 1) return;
        var i = this.K;
        var t = this.U;
        var n = 0;
        while (n << 1 < this.o) {
            var r = i.L;
            i.L = t.L;
            t.L = r;
            i = i.H;
            t = t.F;
            n += 1;
        }
    };
    LinkList.prototype.unique = function() {
        if (this.o <= 1) return;
        var i = this.K;
        while (i !== this.J) {
            var t = i;
            while (t.H && t.L === t.H.L) {
                t = t.H;
                this.o -= 1;
            }
            i.H = t.H;
            i.H.F = i;
            i = i.H;
        }
    };
    LinkList.prototype.sort = function(i) {
        if (this.o <= 1) return;
        var t = [];
        this.forEach((function(i) {
            return t.push(i);
        }));
        t.sort(i);
        var n = this.K;
        t.forEach((function(i) {
            n.L = i;
            n = n.H;
        }));
    };
    LinkList.prototype.pushFront = function(i) {
        this.o += 1;
        var t = new LinkNode(i);
        if (!this.K) {
            this.K = this.U = t;
            this.U.H = this.J;
            this.J.F = this.U;
        } else {
            t.H = this.K;
            this.K.F = t;
            this.K = t;
        }
        this.J.H = this.K;
        this.K.F = this.J;
    };
    LinkList.prototype.popFront = function() {
        if (!this.K) return;
        this.o -= 1;
        if (this.K === this.U) {
            this.K = this.U = undefined;
            this.J.F = this.U;
        } else {
            this.K = this.K.H;
            this.K.F = this.J;
        }
        this.J.H = this.K;
    };
    LinkList.prototype.merge = function(i) {
        var t = this;
        if (!this.K) {
            i.forEach((function(i) {
                return t.pushBack(i);
            }));
            return;
        }
        var n = this.K;
        i.forEach((function(i) {
            while (n && n !== t.J && n.L <= i) {
                n = n.H;
            }
            if (n === t.J) {
                t.pushBack(i);
                n = t.U;
            } else if (n === t.K) {
                t.pushFront(i);
                n = t.K;
            } else {
                t.o += 1;
                var r = n.F;
                r.H = new LinkNode(i);
                r.H.F = r;
                r.H.H = n;
                n.F = r.H;
            }
        }));
    };
    LinkList.prototype[Symbol.iterator] = function() {
        return function() {
            var i;
            return __generator(this, (function(t) {
                switch (t.label) {
                  case 0:
                    if (!this.K) return [ 2 ];
                    i = this.K;
                    t.label = 1;

                  case 1:
                    if (!(i !== this.J)) return [ 3, 3 ];
                    return [ 4, i.L ];

                  case 2:
                    t.sent();
                    i = i.H;
                    return [ 3, 1 ];

                  case 3:
                    return [ 2 ];
                }
            }));
        }.bind(this)();
    };
    return LinkList;
}(SequentialContainer);

export default LinkList;