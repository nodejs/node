"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _TreeNode = require("./TreeNode");

var _ContainerBase = require("../../ContainerBase");

var _throwError = require("../../../utils/throwError");

class TreeContainer extends _ContainerBase.Container {
    constructor(e = function(e, t) {
        if (e < t) return -1;
        if (e > t) return 1;
        return 0;
    }, t = false) {
        super();
        this.rr = undefined;
        this.v = e;
        if (t) {
            this.re = _TreeNode.TreeNodeEnableIndex;
            this.M = function(e, t, i) {
                const s = this.ne(e, t, i);
                if (s) {
                    let e = s.tt;
                    while (e !== this.h) {
                        e.rt += 1;
                        e = e.tt;
                    }
                    const t = this.he(s);
                    if (t) {
                        const {parentNode: e, grandParent: i, curNode: s} = t;
                        e.ie();
                        i.ie();
                        s.ie();
                    }
                }
                return this.i;
            };
            this.K = function(e) {
                let t = this.fe(e);
                while (t !== this.h) {
                    t.rt -= 1;
                    t = t.tt;
                }
            };
        } else {
            this.re = _TreeNode.TreeNode;
            this.M = function(e, t, i) {
                const s = this.ne(e, t, i);
                if (s) this.he(s);
                return this.i;
            };
            this.K = this.fe;
        }
        this.h = new this.re;
    }
    $(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                e = e.Z;
            } else if (s > 0) {
                i = e;
                e = e.Y;
            } else return e;
        }
        return i;
    }
    er(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s <= 0) {
                e = e.Z;
            } else {
                i = e;
                e = e.Y;
            }
        }
        return i;
    }
    tr(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                i = e;
                e = e.Z;
            } else if (s > 0) {
                e = e.Y;
            } else return e;
        }
        return i;
    }
    sr(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                i = e;
                e = e.Z;
            } else {
                e = e.Y;
            }
        }
        return i;
    }
    ue(e) {
        while (true) {
            const t = e.tt;
            if (t === this.h) return;
            if (e.ee === 1) {
                e.ee = 0;
                return;
            }
            if (e === t.Y) {
                const i = t.Z;
                if (i.ee === 1) {
                    i.ee = 0;
                    t.ee = 1;
                    if (t === this.rr) {
                        this.rr = t.te();
                    } else t.te();
                } else {
                    if (i.Z && i.Z.ee === 1) {
                        i.ee = t.ee;
                        t.ee = 0;
                        i.Z.ee = 0;
                        if (t === this.rr) {
                            this.rr = t.te();
                        } else t.te();
                        return;
                    } else if (i.Y && i.Y.ee === 1) {
                        i.ee = 1;
                        i.Y.ee = 0;
                        i.se();
                    } else {
                        i.ee = 1;
                        e = t;
                    }
                }
            } else {
                const i = t.Y;
                if (i.ee === 1) {
                    i.ee = 0;
                    t.ee = 1;
                    if (t === this.rr) {
                        this.rr = t.se();
                    } else t.se();
                } else {
                    if (i.Y && i.Y.ee === 1) {
                        i.ee = t.ee;
                        t.ee = 0;
                        i.Y.ee = 0;
                        if (t === this.rr) {
                            this.rr = t.se();
                        } else t.se();
                        return;
                    } else if (i.Z && i.Z.ee === 1) {
                        i.ee = 1;
                        i.Z.ee = 0;
                        i.te();
                    } else {
                        i.ee = 1;
                        e = t;
                    }
                }
            }
        }
    }
    fe(e) {
        if (this.i === 1) {
            this.clear();
            return this.h;
        }
        let t = e;
        while (t.Y || t.Z) {
            if (t.Z) {
                t = t.Z;
                while (t.Y) t = t.Y;
            } else {
                t = t.Y;
            }
            [e.u, t.u] = [ t.u, e.u ];
            [e.l, t.l] = [ t.l, e.l ];
            e = t;
        }
        if (this.h.Y === t) {
            this.h.Y = t.tt;
        } else if (this.h.Z === t) {
            this.h.Z = t.tt;
        }
        this.ue(t);
        const i = t.tt;
        if (t === i.Y) {
            i.Y = undefined;
        } else i.Z = undefined;
        this.i -= 1;
        this.rr.ee = 0;
        return i;
    }
    oe(e, t) {
        if (e === undefined) return false;
        const i = this.oe(e.Y, t);
        if (i) return true;
        if (t(e)) return true;
        return this.oe(e.Z, t);
    }
    he(e) {
        while (true) {
            const t = e.tt;
            if (t.ee === 0) return;
            const i = t.tt;
            if (t === i.Y) {
                const s = i.Z;
                if (s && s.ee === 1) {
                    s.ee = t.ee = 0;
                    if (i === this.rr) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === t.Z) {
                    e.ee = 0;
                    if (e.Y) e.Y.tt = t;
                    if (e.Z) e.Z.tt = i;
                    t.Z = e.Y;
                    i.Y = e.Z;
                    e.Y = t;
                    e.Z = i;
                    if (i === this.rr) {
                        this.rr = e;
                        this.h.tt = e;
                    } else {
                        const t = i.tt;
                        if (t.Y === i) {
                            t.Y = e;
                        } else t.Z = e;
                    }
                    e.tt = i.tt;
                    t.tt = e;
                    i.tt = e;
                    i.ee = 1;
                    return {
                        parentNode: t,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    t.ee = 0;
                    if (i === this.rr) {
                        this.rr = i.se();
                    } else i.se();
                    i.ee = 1;
                }
            } else {
                const s = i.Y;
                if (s && s.ee === 1) {
                    s.ee = t.ee = 0;
                    if (i === this.rr) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === t.Y) {
                    e.ee = 0;
                    if (e.Y) e.Y.tt = i;
                    if (e.Z) e.Z.tt = t;
                    i.Z = e.Y;
                    t.Y = e.Z;
                    e.Y = i;
                    e.Z = t;
                    if (i === this.rr) {
                        this.rr = e;
                        this.h.tt = e;
                    } else {
                        const t = i.tt;
                        if (t.Y === i) {
                            t.Y = e;
                        } else t.Z = e;
                    }
                    e.tt = i.tt;
                    t.tt = e;
                    i.tt = e;
                    i.ee = 1;
                    return {
                        parentNode: t,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    t.ee = 0;
                    if (i === this.rr) {
                        this.rr = i.te();
                    } else i.te();
                    i.ee = 1;
                }
            }
            return;
        }
    }
    ne(e, t, i) {
        if (this.rr === undefined) {
            this.i += 1;
            this.rr = new this.re(e, t);
            this.rr.ee = 0;
            this.rr.tt = this.h;
            this.h.tt = this.rr;
            this.h.Y = this.rr;
            this.h.Z = this.rr;
            return;
        }
        let s;
        const r = this.h.Y;
        const n = this.v(r.u, e);
        if (n === 0) {
            r.l = t;
            return;
        } else if (n > 0) {
            r.Y = new this.re(e, t);
            r.Y.tt = r;
            s = r.Y;
            this.h.Y = s;
        } else {
            const r = this.h.Z;
            const n = this.v(r.u, e);
            if (n === 0) {
                r.l = t;
                return;
            } else if (n < 0) {
                r.Z = new this.re(e, t);
                r.Z.tt = r;
                s = r.Z;
                this.h.Z = s;
            } else {
                if (i !== undefined) {
                    const r = i.o;
                    if (r !== this.h) {
                        const i = this.v(r.u, e);
                        if (i === 0) {
                            r.l = t;
                            return;
                        } else if (i > 0) {
                            const i = r.L();
                            const n = this.v(i.u, e);
                            if (n === 0) {
                                i.l = t;
                                return;
                            } else if (n < 0) {
                                s = new this.re(e, t);
                                if (i.Z === undefined) {
                                    i.Z = s;
                                    s.tt = i;
                                } else {
                                    r.Y = s;
                                    s.tt = r;
                                }
                            }
                        }
                    }
                }
                if (s === undefined) {
                    s = this.rr;
                    while (true) {
                        const i = this.v(s.u, e);
                        if (i > 0) {
                            if (s.Y === undefined) {
                                s.Y = new this.re(e, t);
                                s.Y.tt = s;
                                s = s.Y;
                                break;
                            }
                            s = s.Y;
                        } else if (i < 0) {
                            if (s.Z === undefined) {
                                s.Z = new this.re(e, t);
                                s.Z.tt = s;
                                s = s.Z;
                                break;
                            }
                            s = s.Z;
                        } else {
                            s.l = t;
                            return;
                        }
                    }
                }
            }
        }
        this.i += 1;
        return s;
    }
    I(e, t) {
        while (e) {
            const i = this.v(e.u, t);
            if (i < 0) {
                e = e.Z;
            } else if (i > 0) {
                e = e.Y;
            } else return e;
        }
        return e || this.h;
    }
    clear() {
        this.i = 0;
        this.rr = undefined;
        this.h.tt = undefined;
        this.h.Y = this.h.Z = undefined;
    }
    updateKeyByIterator(e, t) {
        const i = e.o;
        if (i === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        if (this.i === 1) {
            i.u = t;
            return true;
        }
        if (i === this.h.Y) {
            if (this.v(i.B().u, t) > 0) {
                i.u = t;
                return true;
            }
            return false;
        }
        if (i === this.h.Z) {
            if (this.v(i.L().u, t) < 0) {
                i.u = t;
                return true;
            }
            return false;
        }
        const s = i.L().u;
        if (this.v(s, t) >= 0) return false;
        const r = i.B().u;
        if (this.v(r, t) <= 0) return false;
        i.u = t;
        return true;
    }
    eraseElementByPos(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        let t = 0;
        const i = this;
        this.oe(this.rr, (function(s) {
            if (e === t) {
                i.K(s);
                return true;
            }
            t += 1;
            return false;
        }));
        return this.i;
    }
    eraseElementByKey(e) {
        if (this.i === 0) return false;
        const t = this.I(this.rr, e);
        if (t === this.h) return false;
        this.K(t);
        return true;
    }
    eraseElementByIterator(e) {
        const t = e.o;
        if (t === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        const i = t.Z === undefined;
        const s = e.iteratorType === 0;
        if (s) {
            if (i) e.next();
        } else {
            if (!i || t.Y === undefined) e.next();
        }
        this.K(t);
        return e;
    }
    forEach(e) {
        let t = 0;
        for (const i of this) e(i, t++, this);
    }
    getElementByPos(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        let t;
        let i = 0;
        for (const s of this) {
            if (i === e) {
                t = s;
                break;
            }
            i += 1;
        }
        return t;
    }
    getHeight() {
        if (this.i === 0) return 0;
        const traversal = function(e) {
            if (!e) return 0;
            return Math.max(traversal(e.Y), traversal(e.Z)) + 1;
        };
        return traversal(this.rr);
    }
}

var _default = TreeContainer;

exports.default = _default;
//# sourceMappingURL=index.js.map
