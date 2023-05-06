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

import { TreeNode, TreeNodeEnableIndex } from "./TreeNode";

import { Container } from "../../ContainerBase";

import { throwIteratorAccessError } from "../../../utils/throwError";

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
        t.j = i;
        t.enableIndex = r;
        t.re = r ? TreeNodeEnableIndex : TreeNode;
        t.u = new t.re;
        return t;
    }
    TreeContainer.prototype.$ = function(e, i) {
        var r = this.u;
        while (e) {
            var t = this.j(e.p, i);
            if (t < 0) {
                e = e.Z;
            } else if (t > 0) {
                r = e;
                e = e.Y;
            } else return e;
        }
        return r;
    };
    TreeContainer.prototype.tr = function(e, i) {
        var r = this.u;
        while (e) {
            var t = this.j(e.p, i);
            if (t <= 0) {
                e = e.Z;
            } else {
                r = e;
                e = e.Y;
            }
        }
        return r;
    };
    TreeContainer.prototype.er = function(e, i) {
        var r = this.u;
        while (e) {
            var t = this.j(e.p, i);
            if (t < 0) {
                r = e;
                e = e.Z;
            } else if (t > 0) {
                e = e.Y;
            } else return e;
        }
        return r;
    };
    TreeContainer.prototype.nr = function(e, i) {
        var r = this.u;
        while (e) {
            var t = this.j(e.p, i);
            if (t < 0) {
                r = e;
                e = e.Z;
            } else {
                e = e.Y;
            }
        }
        return r;
    };
    TreeContainer.prototype.se = function(e) {
        while (true) {
            var i = e.sr;
            if (i === this.u) return;
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
                        this.rr = i.te();
                    } else i.te();
                } else {
                    if (r.Z && r.Z.ee === 1) {
                        r.ee = i.ee;
                        i.ee = 0;
                        r.Z.ee = 0;
                        if (i === this.rr) {
                            this.rr = i.te();
                        } else i.te();
                        return;
                    } else if (r.Y && r.Y.ee === 1) {
                        r.ee = 1;
                        r.Y.ee = 0;
                        r.ne();
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
                        this.rr = i.ne();
                    } else i.ne();
                } else {
                    if (r.Y && r.Y.ee === 1) {
                        r.ee = i.ee;
                        i.ee = 0;
                        r.Y.ee = 0;
                        if (i === this.rr) {
                            this.rr = i.ne();
                        } else i.ne();
                        return;
                    } else if (r.Z && r.Z.ee === 1) {
                        r.ee = 1;
                        r.Z.ee = 0;
                        r.te();
                    } else {
                        r.ee = 1;
                        e = i;
                    }
                }
            }
        }
    };
    TreeContainer.prototype.U = function(e) {
        if (this.i === 1) {
            this.clear();
            return;
        }
        var i = e;
        while (i.Y || i.Z) {
            if (i.Z) {
                i = i.Z;
                while (i.Y) i = i.Y;
            } else {
                i = i.Y;
            }
            var r = e.p;
            e.p = i.p;
            i.p = r;
            var t = e.H;
            e.H = i.H;
            i.H = t;
            e = i;
        }
        if (this.u.Y === i) {
            this.u.Y = i.sr;
        } else if (this.u.Z === i) {
            this.u.Z = i.sr;
        }
        this.se(i);
        var n = i.sr;
        if (i === n.Y) {
            n.Y = undefined;
        } else n.Z = undefined;
        this.i -= 1;
        this.rr.ee = 0;
        if (this.enableIndex) {
            while (n !== this.u) {
                n.hr -= 1;
                n = n.sr;
            }
        }
    };
    TreeContainer.prototype.ir = function(e) {
        var i = typeof e === "number" ? e : undefined;
        var r = typeof e === "function" ? e : undefined;
        var t = typeof e === "undefined" ? [] : undefined;
        var n = 0;
        var s = this.rr;
        var f = [];
        while (f.length || s) {
            if (s) {
                f.push(s);
                s = s.Y;
            } else {
                s = f.pop();
                if (n === i) return s;
                t && t.push(s);
                r && r(s, n, this);
                n += 1;
                s = s.Z;
            }
        }
        return t;
    };
    TreeContainer.prototype.fe = function(e) {
        while (true) {
            var i = e.sr;
            if (i.ee === 0) return;
            var r = i.sr;
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
                    if (e.Y) {
                        e.Y.sr = i;
                    }
                    if (e.Z) {
                        e.Z.sr = r;
                    }
                    i.Z = e.Y;
                    r.Y = e.Z;
                    e.Y = i;
                    e.Z = r;
                    if (r === this.rr) {
                        this.rr = e;
                        this.u.sr = e;
                    } else {
                        var n = r.sr;
                        if (n.Y === r) {
                            n.Y = e;
                        } else n.Z = e;
                    }
                    e.sr = r.sr;
                    i.sr = e;
                    r.sr = e;
                    r.ee = 1;
                } else {
                    i.ee = 0;
                    if (r === this.rr) {
                        this.rr = r.ne();
                    } else r.ne();
                    r.ee = 1;
                    return;
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
                    if (e.Y) {
                        e.Y.sr = r;
                    }
                    if (e.Z) {
                        e.Z.sr = i;
                    }
                    r.Z = e.Y;
                    i.Y = e.Z;
                    e.Y = r;
                    e.Z = i;
                    if (r === this.rr) {
                        this.rr = e;
                        this.u.sr = e;
                    } else {
                        var n = r.sr;
                        if (n.Y === r) {
                            n.Y = e;
                        } else n.Z = e;
                    }
                    e.sr = r.sr;
                    i.sr = e;
                    r.sr = e;
                    r.ee = 1;
                } else {
                    i.ee = 0;
                    if (r === this.rr) {
                        this.rr = r.te();
                    } else r.te();
                    r.ee = 1;
                    return;
                }
            }
            if (this.enableIndex) {
                i.ie();
                r.ie();
                e.ie();
            }
            return;
        }
    };
    TreeContainer.prototype.v = function(e, i, r) {
        if (this.rr === undefined) {
            this.i += 1;
            this.rr = new this.re(e, i, 0);
            this.rr.sr = this.u;
            this.u.sr = this.u.Y = this.u.Z = this.rr;
            return this.i;
        }
        var t;
        var n = this.u.Y;
        var s = this.j(n.p, e);
        if (s === 0) {
            n.H = i;
            return this.i;
        } else if (s > 0) {
            n.Y = new this.re(e, i);
            n.Y.sr = n;
            t = n.Y;
            this.u.Y = t;
        } else {
            var f = this.u.Z;
            var h = this.j(f.p, e);
            if (h === 0) {
                f.H = i;
                return this.i;
            } else if (h < 0) {
                f.Z = new this.re(e, i);
                f.Z.sr = f;
                t = f.Z;
                this.u.Z = t;
            } else {
                if (r !== undefined) {
                    var u = r.o;
                    if (u !== this.u) {
                        var a = this.j(u.p, e);
                        if (a === 0) {
                            u.H = i;
                            return this.i;
                        } else if (a > 0) {
                            var o = u.L();
                            var l = this.j(o.p, e);
                            if (l === 0) {
                                o.H = i;
                                return this.i;
                            } else if (l < 0) {
                                t = new this.re(e, i);
                                if (o.Z === undefined) {
                                    o.Z = t;
                                    t.sr = o;
                                } else {
                                    u.Y = t;
                                    t.sr = u;
                                }
                            }
                        }
                    }
                }
                if (t === undefined) {
                    t = this.rr;
                    while (true) {
                        var v = this.j(t.p, e);
                        if (v > 0) {
                            if (t.Y === undefined) {
                                t.Y = new this.re(e, i);
                                t.Y.sr = t;
                                t = t.Y;
                                break;
                            }
                            t = t.Y;
                        } else if (v < 0) {
                            if (t.Z === undefined) {
                                t.Z = new this.re(e, i);
                                t.Z.sr = t;
                                t = t.Z;
                                break;
                            }
                            t = t.Z;
                        } else {
                            t.H = i;
                            return this.i;
                        }
                    }
                }
            }
        }
        if (this.enableIndex) {
            var d = t.sr;
            while (d !== this.u) {
                d.hr += 1;
                d = d.sr;
            }
        }
        this.fe(t);
        this.i += 1;
        return this.i;
    };
    TreeContainer.prototype.ar = function(e, i) {
        while (e) {
            var r = this.j(e.p, i);
            if (r < 0) {
                e = e.Z;
            } else if (r > 0) {
                e = e.Y;
            } else return e;
        }
        return e || this.u;
    };
    TreeContainer.prototype.clear = function() {
        this.i = 0;
        this.rr = undefined;
        this.u.sr = undefined;
        this.u.Y = this.u.Z = undefined;
    };
    TreeContainer.prototype.updateKeyByIterator = function(e, i) {
        var r = e.o;
        if (r === this.u) {
            throwIteratorAccessError();
        }
        if (this.i === 1) {
            r.p = i;
            return true;
        }
        var t = r.m().p;
        if (r === this.u.Y) {
            if (this.j(t, i) > 0) {
                r.p = i;
                return true;
            }
            return false;
        }
        var n = r.L().p;
        if (r === this.u.Z) {
            if (this.j(n, i) < 0) {
                r.p = i;
                return true;
            }
            return false;
        }
        if (this.j(n, i) >= 0 || this.j(t, i) <= 0) return false;
        r.p = i;
        return true;
    };
    TreeContainer.prototype.eraseElementByPos = function(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        var i = this.ir(e);
        this.U(i);
        return this.i;
    };
    TreeContainer.prototype.eraseElementByKey = function(e) {
        if (this.i === 0) return false;
        var i = this.ar(this.rr, e);
        if (i === this.u) return false;
        this.U(i);
        return true;
    };
    TreeContainer.prototype.eraseElementByIterator = function(e) {
        var i = e.o;
        if (i === this.u) {
            throwIteratorAccessError();
        }
        var r = i.Z === undefined;
        var t = e.iteratorType === 0;
        if (t) {
            if (r) e.next();
        } else {
            if (!r || i.Y === undefined) e.next();
        }
        this.U(i);
        return e;
    };
    TreeContainer.prototype.getHeight = function() {
        if (this.i === 0) return 0;
        function traversal(e) {
            if (!e) return 0;
            return Math.max(traversal(e.Y), traversal(e.Z)) + 1;
        }
        return traversal(this.rr);
    };
    return TreeContainer;
}(Container);

export default TreeContainer;
//# sourceMappingURL=index.js.map
