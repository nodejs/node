var __extends = this && this.t || function() {
    var extendStatics = function(e, i) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, i) {
            e.__proto__ = i;
        } || function(e, i) {
            for (var r in i) if (Object.prototype.hasOwnProperty.call(i, r)) e[r] = i[r];
        };
        return extendStatics(e, i);
    };
    return function(e, i) {
        if (typeof i !== "function" && i !== null) throw new TypeError("Class extends value " + String(i) + " is not a constructor or null");
        extendStatics(e, i);
        function __() {
            this.constructor = e;
        }
        e.prototype = i === null ? Object.create(i) : (__.prototype = i.prototype, new __);
    };
}();

var __read = this && this._ || function(e, i) {
    var r = typeof Symbol === "function" && e[Symbol.iterator];
    if (!r) return e;
    var t = r.call(e), n, s = [], f;
    try {
        while ((i === void 0 || i-- > 0) && !(n = t.next()).done) s.push(n.value);
    } catch (e) {
        f = {
            error: e
        };
    } finally {
        try {
            if (n && !n.done && (r = t["return"])) r.call(t);
        } finally {
            if (f) throw f.error;
        }
    }
    return s;
};

import { Container } from "../../ContainerBase";

import { TreeNode, TreeNodeEnableIndex } from "./TreeNode";

var TreeContainer = function(e) {
    __extends(TreeContainer, e);
    function TreeContainer(i, r) {
        if (i === void 0) {
            i = function(e, i) {
                if (e < i) return -1;
                if (e > i) return 1;
                return 0;
            };
        }
        if (r === void 0) {
            r = false;
        }
        var t = e.call(this) || this;
        t.rr = undefined;
        t.ie = function(e, i) {
            if (e === undefined) return false;
            var r = t.ie(e.Y, i);
            if (r) return true;
            if (i(e)) return true;
            return t.ie(e.Z, i);
        };
        t.A = i;
        if (r) {
            t.re = TreeNodeEnableIndex;
            t.ir = function(e, i, r) {
                var t = this.te(e, i, r);
                if (t) {
                    var n = t.tt;
                    while (n !== this.J) {
                        n.rt += 1;
                        n = n.tt;
                    }
                    var s = this.ne(t);
                    if (s) {
                        var f = s, h = f.parentNode, u = f.grandParent, a = f.curNode;
                        h.recount();
                        u.recount();
                        a.recount();
                    }
                }
            };
            t.se = function(e) {
                var i = this.fe(e);
                while (i !== this.J) {
                    i.rt -= 1;
                    i = i.tt;
                }
            };
        } else {
            t.re = TreeNode;
            t.ir = function(e, i, r) {
                var t = this.te(e, i, r);
                if (t) this.ne(t);
            };
            t.se = t.fe;
        }
        t.J = new t.re;
        return t;
    }
    TreeContainer.prototype.$ = function(e, i) {
        var r;
        while (e) {
            var t = this.A(e.W, i);
            if (t < 0) {
                e = e.Z;
            } else if (t > 0) {
                r = e;
                e = e.Y;
            } else return e;
        }
        return r === undefined ? this.J : r;
    };
    TreeContainer.prototype.er = function(e, i) {
        var r;
        while (e) {
            var t = this.A(e.W, i);
            if (t <= 0) {
                e = e.Z;
            } else {
                r = e;
                e = e.Y;
            }
        }
        return r === undefined ? this.J : r;
    };
    TreeContainer.prototype.tr = function(e, i) {
        var r;
        while (e) {
            var t = this.A(e.W, i);
            if (t < 0) {
                r = e;
                e = e.Z;
            } else if (t > 0) {
                e = e.Y;
            } else return e;
        }
        return r === undefined ? this.J : r;
    };
    TreeContainer.prototype.nr = function(e, i) {
        var r;
        while (e) {
            var t = this.A(e.W, i);
            if (t < 0) {
                r = e;
                e = e.Z;
            } else {
                e = e.Y;
            }
        }
        return r === undefined ? this.J : r;
    };
    TreeContainer.prototype.he = function(e) {
        while (true) {
            var i = e.tt;
            if (i === this.J) return;
            if (e.ee === 1) {
                e.ee = 0;
                return;
            }
            if (e === i.Y) {
                var r = i.Z;
                if (r.ee === 1) {
                    r.ee = 0;
                    i.ee = 1;
                    if (i === this.rr) {
                        this.rr = i.rotateLeft();
                    } else i.rotateLeft();
                } else {
                    if (r.Z && r.Z.ee === 1) {
                        r.ee = i.ee;
                        i.ee = 0;
                        r.Z.ee = 0;
                        if (i === this.rr) {
                            this.rr = i.rotateLeft();
                        } else i.rotateLeft();
                        return;
                    } else if (r.Y && r.Y.ee === 1) {
                        r.ee = 1;
                        r.Y.ee = 0;
                        r.rotateRight();
                    } else {
                        r.ee = 1;
                        e = i;
                    }
                }
            } else {
                var r = i.Y;
                if (r.ee === 1) {
                    r.ee = 0;
                    i.ee = 1;
                    if (i === this.rr) {
                        this.rr = i.rotateRight();
                    } else i.rotateRight();
                } else {
                    if (r.Y && r.Y.ee === 1) {
                        r.ee = i.ee;
                        i.ee = 0;
                        r.Y.ee = 0;
                        if (i === this.rr) {
                            this.rr = i.rotateRight();
                        } else i.rotateRight();
                        return;
                    } else if (r.Z && r.Z.ee === 1) {
                        r.ee = 1;
                        r.Z.ee = 0;
                        r.rotateLeft();
                    } else {
                        r.ee = 1;
                        e = i;
                    }
                }
            }
        }
    };
    TreeContainer.prototype.fe = function(e) {
        var i, r;
        if (this.o === 1) {
            this.clear();
            return this.J;
        }
        var t = e;
        while (t.Y || t.Z) {
            if (t.Z) {
                t = t.Z;
                while (t.Y) t = t.Y;
            } else {
                t = t.Y;
            }
            i = __read([ t.W, e.W ], 2), e.W = i[0], t.W = i[1];
            r = __read([ t.L, e.L ], 2), e.L = r[0], t.L = r[1];
            e = t;
        }
        if (this.J.Y === t) {
            this.J.Y = t.tt;
        } else if (this.J.Z === t) {
            this.J.Z = t.tt;
        }
        this.he(t);
        var n = t.tt;
        if (t === n.Y) {
            n.Y = undefined;
        } else n.Z = undefined;
        this.o -= 1;
        this.rr.ee = 0;
        return n;
    };
    TreeContainer.prototype.ne = function(e) {
        while (true) {
            var i = e.tt;
            if (i.ee === 0) return;
            var r = i.tt;
            if (i === r.Y) {
                var t = r.Z;
                if (t && t.ee === 1) {
                    t.ee = i.ee = 0;
                    if (r === this.rr) return;
                    r.ee = 1;
                    e = r;
                    continue;
                } else if (e === i.Z) {
                    e.ee = 0;
                    if (e.Y) e.Y.tt = i;
                    if (e.Z) e.Z.tt = r;
                    i.Z = e.Y;
                    r.Y = e.Z;
                    e.Y = i;
                    e.Z = r;
                    if (r === this.rr) {
                        this.rr = e;
                        this.J.tt = e;
                    } else {
                        var n = r.tt;
                        if (n.Y === r) {
                            n.Y = e;
                        } else n.Z = e;
                    }
                    e.tt = r.tt;
                    i.tt = e;
                    r.tt = e;
                    r.ee = 1;
                    return {
                        parentNode: i,
                        grandParent: r,
                        curNode: e
                    };
                } else {
                    i.ee = 0;
                    if (r === this.rr) {
                        this.rr = r.rotateRight();
                    } else r.rotateRight();
                    r.ee = 1;
                }
            } else {
                var t = r.Y;
                if (t && t.ee === 1) {
                    t.ee = i.ee = 0;
                    if (r === this.rr) return;
                    r.ee = 1;
                    e = r;
                    continue;
                } else if (e === i.Y) {
                    e.ee = 0;
                    if (e.Y) e.Y.tt = r;
                    if (e.Z) e.Z.tt = i;
                    r.Z = e.Y;
                    i.Y = e.Z;
                    e.Y = r;
                    e.Z = i;
                    if (r === this.rr) {
                        this.rr = e;
                        this.J.tt = e;
                    } else {
                        var n = r.tt;
                        if (n.Y === r) {
                            n.Y = e;
                        } else n.Z = e;
                    }
                    e.tt = r.tt;
                    i.tt = e;
                    r.tt = e;
                    r.ee = 1;
                    return {
                        parentNode: i,
                        grandParent: r,
                        curNode: e
                    };
                } else {
                    i.ee = 0;
                    if (r === this.rr) {
                        this.rr = r.rotateLeft();
                    } else r.rotateLeft();
                    r.ee = 1;
                }
            }
            return;
        }
    };
    TreeContainer.prototype.te = function(e, i, r) {
        if (this.rr === undefined) {
            this.o += 1;
            this.rr = new this.re(e, i);
            this.rr.ee = 0;
            this.rr.tt = this.J;
            this.J.tt = this.rr;
            this.J.Y = this.rr;
            this.J.Z = this.rr;
            return;
        }
        var t;
        var n = this.J.Y;
        var s = this.A(n.W, e);
        if (s === 0) {
            n.L = i;
            return;
        } else if (s > 0) {
            n.Y = new this.re(e, i);
            n.Y.tt = n;
            t = n.Y;
            this.J.Y = t;
        } else {
            var f = this.J.Z;
            var h = this.A(f.W, e);
            if (h === 0) {
                f.L = i;
                return;
            } else if (h < 0) {
                f.Z = new this.re(e, i);
                f.Z.tt = f;
                t = f.Z;
                this.J.Z = t;
            } else {
                if (r !== undefined) {
                    var u = r.D;
                    if (u !== this.J) {
                        var a = this.A(u.W, e);
                        if (a === 0) {
                            u.L = i;
                            return;
                        } else if (a > 0) {
                            var o = u.pre();
                            var l = this.A(o.W, e);
                            if (l === 0) {
                                o.L = i;
                                return;
                            } else if (l < 0) {
                                t = new this.re(e, i);
                                if (o.Z === undefined) {
                                    o.Z = t;
                                    t.tt = o;
                                } else {
                                    u.Y = t;
                                    t.tt = u;
                                }
                            }
                        }
                    }
                }
                if (t === undefined) {
                    t = this.rr;
                    while (true) {
                        var d = this.A(t.W, e);
                        if (d > 0) {
                            if (t.Y === undefined) {
                                t.Y = new this.re(e, i);
                                t.Y.tt = t;
                                t = t.Y;
                                break;
                            }
                            t = t.Y;
                        } else if (d < 0) {
                            if (t.Z === undefined) {
                                t.Z = new this.re(e, i);
                                t.Z.tt = t;
                                t = t.Z;
                                break;
                            }
                            t = t.Z;
                        } else {
                            t.L = i;
                            return;
                        }
                    }
                }
            }
        }
        this.o += 1;
        return t;
    };
    TreeContainer.prototype.clear = function() {
        this.o = 0;
        this.rr = undefined;
        this.J.tt = undefined;
        this.J.Y = this.J.Z = undefined;
    };
    TreeContainer.prototype.updateKeyByIterator = function(e, i) {
        var r = e.D;
        if (r === this.J) {
            throw new TypeError("Invalid iterator!");
        }
        if (this.o === 1) {
            r.W = i;
            return true;
        }
        if (r === this.J.Y) {
            if (this.A(r.next().W, i) > 0) {
                r.W = i;
                return true;
            }
            return false;
        }
        if (r === this.J.Z) {
            if (this.A(r.pre().W, i) < 0) {
                r.W = i;
                return true;
            }
            return false;
        }
        var t = r.pre().W;
        if (this.A(t, i) >= 0) return false;
        var n = r.next().W;
        if (this.A(n, i) <= 0) return false;
        r.W = i;
        return true;
    };
    TreeContainer.prototype.eraseElementByPos = function(e) {
        var i = this;
        if (e < 0 || e > this.o - 1) {
            throw new RangeError;
        }
        var r = 0;
        this.ie(this.rr, (function(t) {
            if (e === r) {
                i.se(t);
                return true;
            }
            r += 1;
            return false;
        }));
    };
    TreeContainer.prototype.ar = function(e, i) {
        while (e) {
            var r = this.A(e.W, i);
            if (r < 0) {
                e = e.Z;
            } else if (r > 0) {
                e = e.Y;
            } else return e;
        }
        return e;
    };
    TreeContainer.prototype.eraseElementByKey = function(e) {
        if (!this.o) return;
        var i = this.ar(this.rr, e);
        if (i === undefined) return;
        this.se(i);
    };
    TreeContainer.prototype.eraseElementByIterator = function(e) {
        var i = e.D;
        if (i === this.J) {
            throw new RangeError("Invalid iterator");
        }
        if (i.Z === undefined) {
            e = e.next();
        }
        this.se(i);
        return e;
    };
    TreeContainer.prototype.getHeight = function() {
        if (!this.o) return 0;
        var traversal = function(e) {
            if (!e) return 0;
            return Math.max(traversal(e.Y), traversal(e.Z)) + 1;
        };
        return traversal(this.rr);
    };
    return TreeContainer;
}(Container);

export default TreeContainer;