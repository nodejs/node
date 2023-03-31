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
        this.X = undefined;
        this.v = e;
        this.enableIndex = t;
        this.re = t ? _TreeNode.TreeNodeEnableIndex : _TreeNode.TreeNode;
        this.h = new this.re;
    }
    U(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                e = e.K;
            } else if (s > 0) {
                i = e;
                e = e.T;
            } else return e;
        }
        return i;
    }
    Y(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s <= 0) {
                e = e.K;
            } else {
                i = e;
                e = e.T;
            }
        }
        return i;
    }
    Z(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                i = e;
                e = e.K;
            } else if (s > 0) {
                e = e.T;
            } else return e;
        }
        return i;
    }
    $(e, t) {
        let i = this.h;
        while (e) {
            const s = this.v(e.u, t);
            if (s < 0) {
                i = e;
                e = e.K;
            } else {
                e = e.T;
            }
        }
        return i;
    }
    ne(e) {
        while (true) {
            const t = e.it;
            if (t === this.h) return;
            if (e.ee === 1) {
                e.ee = 0;
                return;
            }
            if (e === t.T) {
                const i = t.K;
                if (i.ee === 1) {
                    i.ee = 0;
                    t.ee = 1;
                    if (t === this.X) {
                        this.X = t.te();
                    } else t.te();
                } else {
                    if (i.K && i.K.ee === 1) {
                        i.ee = t.ee;
                        t.ee = 0;
                        i.K.ee = 0;
                        if (t === this.X) {
                            this.X = t.te();
                        } else t.te();
                        return;
                    } else if (i.T && i.T.ee === 1) {
                        i.ee = 1;
                        i.T.ee = 0;
                        i.se();
                    } else {
                        i.ee = 1;
                        e = t;
                    }
                }
            } else {
                const i = t.T;
                if (i.ee === 1) {
                    i.ee = 0;
                    t.ee = 1;
                    if (t === this.X) {
                        this.X = t.se();
                    } else t.se();
                } else {
                    if (i.T && i.T.ee === 1) {
                        i.ee = t.ee;
                        t.ee = 0;
                        i.T.ee = 0;
                        if (t === this.X) {
                            this.X = t.se();
                        } else t.se();
                        return;
                    } else if (i.K && i.K.ee === 1) {
                        i.ee = 1;
                        i.K.ee = 0;
                        i.te();
                    } else {
                        i.ee = 1;
                        e = t;
                    }
                }
            }
        }
    }
    V(e) {
        if (this.i === 1) {
            this.clear();
            return;
        }
        let t = e;
        while (t.T || t.K) {
            if (t.K) {
                t = t.K;
                while (t.T) t = t.T;
            } else {
                t = t.T;
            }
            const i = e.u;
            e.u = t.u;
            t.u = i;
            const s = e.l;
            e.l = t.l;
            t.l = s;
            e = t;
        }
        if (this.h.T === t) {
            this.h.T = t.it;
        } else if (this.h.K === t) {
            this.h.K = t.it;
        }
        this.ne(t);
        let i = t.it;
        if (t === i.T) {
            i.T = undefined;
        } else i.K = undefined;
        this.i -= 1;
        this.X.ee = 0;
        if (this.enableIndex) {
            while (i !== this.h) {
                i.st -= 1;
                i = i.it;
            }
        }
    }
    tt(e) {
        const t = typeof e === "number" ? e : undefined;
        const i = typeof e === "function" ? e : undefined;
        const s = typeof e === "undefined" ? [] : undefined;
        let r = 0;
        let n = this.X;
        const h = [];
        while (h.length || n) {
            if (n) {
                h.push(n);
                n = n.T;
            } else {
                n = h.pop();
                if (r === t) return n;
                s && s.push(n);
                i && i(n, r, this);
                r += 1;
                n = n.K;
            }
        }
        return s;
    }
    he(e) {
        while (true) {
            const t = e.it;
            if (t.ee === 0) return;
            const i = t.it;
            if (t === i.T) {
                const s = i.K;
                if (s && s.ee === 1) {
                    s.ee = t.ee = 0;
                    if (i === this.X) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === t.K) {
                    e.ee = 0;
                    if (e.T) {
                        e.T.it = t;
                    }
                    if (e.K) {
                        e.K.it = i;
                    }
                    t.K = e.T;
                    i.T = e.K;
                    e.T = t;
                    e.K = i;
                    if (i === this.X) {
                        this.X = e;
                        this.h.it = e;
                    } else {
                        const t = i.it;
                        if (t.T === i) {
                            t.T = e;
                        } else t.K = e;
                    }
                    e.it = i.it;
                    t.it = e;
                    i.it = e;
                    i.ee = 1;
                } else {
                    t.ee = 0;
                    if (i === this.X) {
                        this.X = i.se();
                    } else i.se();
                    i.ee = 1;
                    return;
                }
            } else {
                const s = i.T;
                if (s && s.ee === 1) {
                    s.ee = t.ee = 0;
                    if (i === this.X) return;
                    i.ee = 1;
                    e = i;
                    continue;
                } else if (e === t.T) {
                    e.ee = 0;
                    if (e.T) {
                        e.T.it = i;
                    }
                    if (e.K) {
                        e.K.it = t;
                    }
                    i.K = e.T;
                    t.T = e.K;
                    e.T = i;
                    e.K = t;
                    if (i === this.X) {
                        this.X = e;
                        this.h.it = e;
                    } else {
                        const t = i.it;
                        if (t.T === i) {
                            t.T = e;
                        } else t.K = e;
                    }
                    e.it = i.it;
                    t.it = e;
                    i.it = e;
                    i.ee = 1;
                } else {
                    t.ee = 0;
                    if (i === this.X) {
                        this.X = i.te();
                    } else i.te();
                    i.ee = 1;
                    return;
                }
            }
            if (this.enableIndex) {
                t.ie();
                i.ie();
                e.ie();
            }
            return;
        }
    }
    M(e, t, i) {
        if (this.X === undefined) {
            this.i += 1;
            this.X = new this.re(e, t, 0);
            this.X.it = this.h;
            this.h.it = this.h.T = this.h.K = this.X;
            return this.i;
        }
        let s;
        const r = this.h.T;
        const n = this.v(r.u, e);
        if (n === 0) {
            r.l = t;
            return this.i;
        } else if (n > 0) {
            r.T = new this.re(e, t);
            r.T.it = r;
            s = r.T;
            this.h.T = s;
        } else {
            const r = this.h.K;
            const n = this.v(r.u, e);
            if (n === 0) {
                r.l = t;
                return this.i;
            } else if (n < 0) {
                r.K = new this.re(e, t);
                r.K.it = r;
                s = r.K;
                this.h.K = s;
            } else {
                if (i !== undefined) {
                    const r = i.o;
                    if (r !== this.h) {
                        const i = this.v(r.u, e);
                        if (i === 0) {
                            r.l = t;
                            return this.i;
                        } else if (i > 0) {
                            const i = r.L();
                            const n = this.v(i.u, e);
                            if (n === 0) {
                                i.l = t;
                                return this.i;
                            } else if (n < 0) {
                                s = new this.re(e, t);
                                if (i.K === undefined) {
                                    i.K = s;
                                    s.it = i;
                                } else {
                                    r.T = s;
                                    s.it = r;
                                }
                            }
                        }
                    }
                }
                if (s === undefined) {
                    s = this.X;
                    while (true) {
                        const i = this.v(s.u, e);
                        if (i > 0) {
                            if (s.T === undefined) {
                                s.T = new this.re(e, t);
                                s.T.it = s;
                                s = s.T;
                                break;
                            }
                            s = s.T;
                        } else if (i < 0) {
                            if (s.K === undefined) {
                                s.K = new this.re(e, t);
                                s.K.it = s;
                                s = s.K;
                                break;
                            }
                            s = s.K;
                        } else {
                            s.l = t;
                            return this.i;
                        }
                    }
                }
            }
        }
        if (this.enableIndex) {
            let e = s.it;
            while (e !== this.h) {
                e.st += 1;
                e = e.it;
            }
        }
        this.he(s);
        this.i += 1;
        return this.i;
    }
    rt(e, t) {
        while (e) {
            const i = this.v(e.u, t);
            if (i < 0) {
                e = e.K;
            } else if (i > 0) {
                e = e.T;
            } else return e;
        }
        return e || this.h;
    }
    clear() {
        this.i = 0;
        this.X = undefined;
        this.h.it = undefined;
        this.h.T = this.h.K = undefined;
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
        const s = i.B().u;
        if (i === this.h.T) {
            if (this.v(s, t) > 0) {
                i.u = t;
                return true;
            }
            return false;
        }
        const r = i.L().u;
        if (i === this.h.K) {
            if (this.v(r, t) < 0) {
                i.u = t;
                return true;
            }
            return false;
        }
        if (this.v(r, t) >= 0 || this.v(s, t) <= 0) return false;
        i.u = t;
        return true;
    }
    eraseElementByPos(e) {
        if (e < 0 || e > this.i - 1) {
            throw new RangeError;
        }
        const t = this.tt(e);
        this.V(t);
        return this.i;
    }
    eraseElementByKey(e) {
        if (this.i === 0) return false;
        const t = this.rt(this.X, e);
        if (t === this.h) return false;
        this.V(t);
        return true;
    }
    eraseElementByIterator(e) {
        const t = e.o;
        if (t === this.h) {
            (0, _throwError.throwIteratorAccessError)();
        }
        const i = t.K === undefined;
        const s = e.iteratorType === 0;
        if (s) {
            if (i) e.next();
        } else {
            if (!i || t.T === undefined) e.next();
        }
        this.V(t);
        return e;
    }
    getHeight() {
        if (this.i === 0) return 0;
        function traversal(e) {
            if (!e) return 0;
            return Math.max(traversal(e.T), traversal(e.K)) + 1;
        }
        return traversal(this.X);
    }
}

var _default = TreeContainer;

exports.default = _default;
//# sourceMappingURL=index.js.map
