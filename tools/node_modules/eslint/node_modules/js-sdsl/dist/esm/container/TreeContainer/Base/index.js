var __extends = this && this.t || function() {
    var extendStatics = function(e, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, r) {
            e.__proto__ = r;
        } || function(e, r) {
            for (var i in r) if (Object.prototype.hasOwnProperty.call(r, i)) e[i] = r[i];
        };
        return extendStatics(e, r);
    };
    return function(e, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(e, r);
        function __() {
            this.constructor = e;
        }
        e.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

var __read = this && this.P || function(e, r) {
    var i = typeof Symbol === "function" && e[Symbol.iterator];
    if (!i) return e;
    var t = i.call(e), n, s = [], f;
    try {
        while ((r === void 0 || r-- > 0) && !(n = t.next()).done) s.push(n.value);
    } catch (e) {
        f = {
            error: e
        };
    } finally {
        try {
            if (n && !n.done && (i = t["return"])) i.call(t);
        } finally {
            if (f) throw f.error;
        }
    }
    return s;
};

var __values = this && this.Z || function(e) {
    var r = typeof Symbol === "function" && Symbol.iterator, i = r && e[r], t = 0;
    if (i) return i.call(e);
    if (e && typeof e.length === "number") return {
        next: function() {
            if (e && t >= e.length) e = void 0;
            return {
                value: e && e[t++],
                done: !e
            };
        }
    };
    throw new TypeError(r ? "Object is not iterable." : "Symbol.iterator is not defined.");
};

import { TreeNode, TreeNodeEnableIndex } from "./TreeNode";

import { Container } from "../../ContainerBase";

import { throwIteratorAccessError } from "../../../utils/throwError";

var TreeContainer = function(e) {
    __extends(TreeContainer, e);
    function TreeContainer(r, i) {
        if (r === void 0) {
            r = function(e, r) {
                if (e < r) return -1;
                if (e > r) return 1;
                return 0;
            };
        }
        if (i === void 0) {
            i = false;
        }
        var t = e.call(this) || this;
        t.ir = undefined;
        t.j = r;
        if (i) {
            t.re = TreeNodeEnableIndex;
            t.v = function(e, r, i) {
                var t = this.se(e, r, i);
                if (t) {
                    var n = t.hr;
                    while (n !== this.h) {
                        n.cr += 1;
                        n = n.hr;
                    }
                    var s = this.fe(t);
                    if (s) {
                        var f = s, h = f.parentNode, u = f.grandParent, a = f.curNode;
                        h.ie();
                        u.ie();
                        a.ie();
                    }
                }
                return this.i;
            };
            t.X = function(e) {
                var r = this.he(e);
                while (r !== this.h) {
                    r.cr -= 1;
                    r = r.hr;
                }
            };
        } else {
            t.re = TreeNode;
            t.v = function(e, r, i) {
                var t = this.se(e, r, i);
                if (t) this.fe(t);
                return this.i;
            };
            t.X = t.he;
        }
        t.h = new t.re;
        return t;
    }
    TreeContainer.prototype.nr = function(e, r) {
        var i = this.h;
        while (e) {
            var t = this.j(e.p, r);
            if (t < 0) {
                e = e.tr;
            } else if (t > 0) {
                i = e;
                e = e.er;
            } else return e;
        }
        return i;
    };
    TreeContainer.prototype.ar = function(e, r) {
        var i = this.h;
        while (e) {
            var t = this.j(e.p, r);
            if (t <= 0) {
                e = e.tr;
            } else {
                i = e;
                e = e.er;
            }
        }
        return i;
    };
    TreeContainer.prototype.ur = function(e, r) {
        var i = this.h;
        while (e) {
            var t = this.j(e.p, r);
            if (t < 0) {
                i = e;
                e = e.tr;
            } else if (t > 0) {
                e = e.er;
            } else return e;
        }
        return i;
    };
    TreeContainer.prototype.sr = function(e, r) {
        var i = this.h;
        while (e) {
            var t = this.j(e.p, r);
            if (t < 0) {
                i = e;
                e = e.tr;
            } else {
                e = e.er;
            }
        }
        return i;
    };
    TreeContainer.prototype.ue = function(e) {
        while (true) {
            var r = e.hr;
            if (r === this.h) return;
            if (e.ee === 1) {
                e.ee = 0;
                return;
            }
            if (e === r.er) {
                var i = r.tr;
                if (i.ee === 1) {
                    i.ee = 0;
                    r.ee = 1;
                    if (r === this.ir) {
                        this.ir = r.ne();
                    } else r.ne();
                } else {
                    if (i.tr && i.tr.ee === 1) {
                        i.ee = r.ee;
                        r.ee = 0;
                        i.tr.ee = 0;
                        if (r === this.ir) {
                            this.ir = r.ne();
                        } else r.ne();
                        return;
                    } else if (i.er && i.er.ee === 1) {
                        i.ee = 1;
                        i.er.ee = 0;
                        i.te();
                    } else {
                        i.ee = 1;
                        e = r;
                    }
                }
            } else {
                var i = r.er;
                if (i.ee === 1) {
                    i.ee = 0;
                    r.ee = 1;
                    if (r === this.ir) {
                        this.ir = r.te();
                    } else r.te();
                } else {
                    if (i.er && i.er.ee === 1) {
                        i.ee = r.ee;
                        r.ee = 0;
                        i.er.ee = 0;
                        if (r === this.ir) {
                            this.ir = r.te();
                        } else r.te();
                        return;
                    } else if (i.tr && i.tr.ee === 1) {
                        i.ee = 1;
                        i.tr.ee = 0;
                        i.ne();
                    } else {
                        i.ee = 1;
                        e = r;
                    }
                }
            }
        }
    };
    TreeContainer.prototype.he = function(e) {
        var r, i;
        if (this.i === 1) {
            this.clear();
            return this.h;
        }
        var t = e;
        while (t.er || t.tr) {
            if (t.tr) {
                t = t.tr;
                while (t.er) t = t.er;
            } else {
                t = t.er;
            }
            r = __read([ t.p, e.p ], 2), e.p = r[0], t.p = r[1];
            i = __read([ t.H, e.H ], 2), e.H = i[0], t.H = i[1];
            e = t;
        }
        if (this.h.er === t) {
            this.h.er = t.hr;
        } else if (this.h.tr === t) {
            this.h.tr = t.hr;
        }
        this.ue(t);
        var n = t.hr;
        if (t === n.er) {
            n.er = undefined;
        } else n.tr = undefined;
        this.i -= 1;
        this.ir.ee = 0;
        return n;
    };
    TreeContainer.prototype.ae = function(e, r) {
        if (e === undefined) return false;
        var i = this.ae(e.er, r);
        if (i) return true;
        if (r(e)) return true;
        return this.ae(e.tr, r);
    };
    TreeContainer.prototype.fe = function(e) {
        while (true) {
            var r = e.hr;
            if (r.ee === 0) return;
            var i = r.hr;
            if (r === i.er) {
                var t = i.tr;
                if (t && t.ee === 1) {
                    t.ee = r.ee = 0;
                    if (i === this.ir) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === r.tr) {
                    e.ee = 0;
                    if (e.er) e.er.hr = r;
                    if (e.tr) e.tr.hr = i;
                    r.tr = e.er;
                    i.er = e.tr;
                    e.er = r;
                    e.tr = i;
                    if (i === this.ir) {
                        this.ir = e;
                        this.h.hr = e;
                    } else {
                        var n = i.hr;
                        if (n.er === i) {
                            n.er = e;
                        } else n.tr = e;
                    }
                    e.hr = i.hr;
                    r.hr = e;
                    i.hr = e;
                    i.ee = 1;
                    return {
                        parentNode: r,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    r.ee = 0;
                    if (i === this.ir) {
                        this.ir = i.te();
                    } else i.te();
                    i.ee = 1;
                }
            } else {
                var t = i.er;
                if (t && t.ee === 1) {
                    t.ee = r.ee = 0;
                    if (i === this.ir) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === r.er) {
                    e.ee = 0;
                    if (e.er) e.er.hr = i;
                    if (e.tr) e.tr.hr = r;
                    i.tr = e.er;
                    r.er = e.tr;
                    e.er = i;
                    e.tr = r;
                    if (i === this.ir) {
                        this.ir = e;
                        this.h.hr = e;
                    } else {
                        var n = i.hr;
                        if (n.er === i) {
                            n.er = e;
                        } else n.tr = e;
                    }
                    e.hr = i.hr;
                    r.hr = e;
                    i.hr = e;
                    i.ee = 1;
                    return {
                        parentNode: r,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    r.ee = 0;
                    if (i === this.ir) {
                        this.ir = i.ne();
                    } else i.ne();
                    i.ee = 1;
                }
            }
            return;
        }
    };
    TreeContainer.prototype.se = function(e, r, i) {
        if (this.ir === undefined) {
            this.i += 1;
            this.ir = new this.re(e, r);
            this.ir.ee = 0;
            this.ir.hr = this.h;
            this.h.hr = this.ir;
            this.h.er = this.ir;
            this.h.tr = this.ir;
            return;
        }
        var t;
        var n = this.h.er;
        var s = this.j(n.p, e);
        if (s === 0) {
            n.H = r;
            return;
        } else if (s > 0) {
            n.er = new this.re(e, r);
            n.er.hr = n;
            t = n.er;
            this.h.er = t;
        } else {
            var f = this.h.tr;
            var h = this.j(f.p, e);
            if (h === 0) {
                f.H = r;
                return;
            } else if (h < 0) {
                f.tr = new this.re(e, r);
                f.tr.hr = f;
                t = f.tr;
                this.h.tr = t;
            } else {
                if (i !== undefined) {
                    var u = i.o;
                    if (u !== this.h) {
                        var a = this.j(u.p, e);
                        if (a === 0) {
                            u.H = r;
                            return;
                        } else if (a > 0) {
                            var o = u.W();
                            var l = this.j(o.p, e);
                            if (l === 0) {
                                o.H = r;
                                return;
                            } else if (l < 0) {
                                t = new this.re(e, r);
                                if (o.tr === undefined) {
                                    o.tr = t;
                                    t.hr = o;
                                } else {
                                    u.er = t;
                                    t.hr = u;
                                }
                            }
                        }
                    }
                }
                if (t === undefined) {
                    t = this.ir;
                    while (true) {
                        var v = this.j(t.p, e);
                        if (v > 0) {
                            if (t.er === undefined) {
                                t.er = new this.re(e, r);
                                t.er.hr = t;
                                t = t.er;
                                break;
                            }
                            t = t.er;
                        } else if (v < 0) {
                            if (t.tr === undefined) {
                                t.tr = new this.re(e, r);
                                t.tr.hr = t;
                                t = t.tr;
                                break;
                            }
                            t = t.tr;
                        } else {
                            t.H = r;
                            return;
                        }
                    }
                }
            }
        }
        this.i += 1;
        return t;
    };
    TreeContainer.prototype.g = function(e, r) {
        while (e) {
            var i = this.j(e.p, r);
            if (i < 0) {
                e = e.tr;
            } else if (i > 0) {
                e = e.er;
            } else return e;
        }
        return e || this.h;
    };
    TreeContainer.prototype.clear = function() {
        this.i = 0;
        this.ir = undefined;
        this.h.hr = undefined;
        this.h.er = this.h.tr = undefined;
    };
    TreeContainer.prototype.updateKeyByIterator = function(e, r) {
        var i = e.o;
        if (i === this.h) {
            throwIteratorAccessError();
        }
        if (this.i === 1) {
            i.p = r;
            return true;
        }
        if (i === this.h.er) {
            if (this.j(i.m().p, r) > 0) {
                i.p = r;
                return true;
            }
            return false;
        }
        if (i === this.h.tr) {
            if (this.j(i.W().p, r) < 0) {
                i.p = r;
                return true;
            }
            return false;
        }
        var t = i.W().p;
        if (this.j(t, r) >= 0) return false;
        var n = i.m().p;
        if (this.j(n, r) <= 0) return false;
        i.p = r;
        return true;
    };
    TreeContainer.prototype.eraseElementByPos = function(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        var r = 0;
        var i = this;
        this.ae(this.ir, (function(t) {
            if (e === r) {
                i.X(t);
                return true;
            }
            r += 1;
            return false;
        }));
        return this.i;
    };
    TreeContainer.prototype.eraseElementByKey = function(e) {
        if (this.i === 0) return false;
        var r = this.g(this.ir, e);
        if (r === this.h) return false;
        this.X(r);
        return true;
    };
    TreeContainer.prototype.eraseElementByIterator = function(e) {
        var r = e.o;
        if (r === this.h) {
            throwIteratorAccessError();
        }
        var i = r.tr === undefined;
        var t = e.iteratorType === 0;
        if (t) {
            if (i) e.next();
        } else {
            if (!i || r.er === undefined) e.next();
        }
        this.X(r);
        return e;
    };
    TreeContainer.prototype.forEach = function(e) {
        var r, i;
        var t = 0;
        try {
            for (var n = __values(this), s = n.next(); !s.done; s = n.next()) {
                var f = s.value;
                e(f, t++, this);
            }
        } catch (e) {
            r = {
                error: e
            };
        } finally {
            try {
                if (s && !s.done && (i = n.return)) i.call(n);
            } finally {
                if (r) throw r.error;
            }
        }
    };
    TreeContainer.prototype.getElementByPos = function(e) {
        var r, i;
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        var t;
        var n = 0;
        try {
            for (var s = __values(this), f = s.next(); !f.done; f = s.next()) {
                var h = f.value;
                if (n === e) {
                    t = h;
                    break;
                }
                n += 1;
            }
        } catch (e) {
            r = {
                error: e
            };
        } finally {
            try {
                if (f && !f.done && (i = s.return)) i.call(s);
            } finally {
                if (r) throw r.error;
            }
        }
        return t;
    };
    TreeContainer.prototype.getHeight = function() {
        if (this.i === 0) return 0;
        var traversal = function(e) {
            if (!e) return 0;
            return Math.max(traversal(e.er), traversal(e.tr)) + 1;
        };
        return traversal(this.ir);
    };
    return TreeContainer;
}(Container);

export default TreeContainer;
//# sourceMappingURL=index.js.map
