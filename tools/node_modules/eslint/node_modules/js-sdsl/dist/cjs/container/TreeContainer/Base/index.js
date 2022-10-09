"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../../ContainerBase");

var _TreeNode = require("./TreeNode");

class TreeContainer extends _ContainerBase.Container {
    constructor(e = ((e, t) => {
        if (e < t) return -1;
        if (e > t) return 1;
        return 0;
    }), t = false) {
        super();
        this.X = undefined;
        this.ie = (e, t) => {
            if (e === undefined) return false;
            const i = this.ie(e.U, t);
            if (i) return true;
            if (t(e)) return true;
            return this.ie(e.J, t);
        };
        this.p = e;
        if (t) {
            this.ne = _TreeNode.TreeNodeEnableIndex;
            this.ee = function(e, t, i) {
                const s = this.he(e, t, i);
                if (s) {
                    let e = s.tt;
                    while (e !== this.S) {
                        e.et += 1;
                        e = e.tt;
                    }
                    const t = this.fe(s);
                    if (t) {
                        const {parentNode: e, grandParent: i, curNode: s} = t;
                        e.recount();
                        i.recount();
                        s.recount();
                    }
                }
            };
            this.ue = function(e) {
                let t = this.le(e);
                while (t !== this.S) {
                    t.et -= 1;
                    t = t.tt;
                }
            };
        } else {
            this.ne = _TreeNode.TreeNode;
            this.ee = function(e, t, i) {
                const s = this.he(e, t, i);
                if (s) this.fe(s);
            };
            this.ue = this.le;
        }
        this.S = new this.ne;
    }
    W(e, t) {
        let i;
        while (e) {
            const s = this.p(e.T, t);
            if (s < 0) {
                e = e.J;
            } else if (s > 0) {
                i = e;
                e = e.U;
            } else return e;
        }
        return i === undefined ? this.S : i;
    }
    Y(e, t) {
        let i;
        while (e) {
            const s = this.p(e.T, t);
            if (s <= 0) {
                e = e.J;
            } else {
                i = e;
                e = e.U;
            }
        }
        return i === undefined ? this.S : i;
    }
    Z(e, t) {
        let i;
        while (e) {
            const s = this.p(e.T, t);
            if (s < 0) {
                i = e;
                e = e.J;
            } else if (s > 0) {
                e = e.U;
            } else return e;
        }
        return i === undefined ? this.S : i;
    }
    $(e, t) {
        let i;
        while (e) {
            const s = this.p(e.T, t);
            if (s < 0) {
                i = e;
                e = e.J;
            } else {
                e = e.U;
            }
        }
        return i === undefined ? this.S : i;
    }
    oe(e) {
        while (true) {
            const t = e.tt;
            if (t === this.S) return;
            if (e.se === 1) {
                e.se = 0;
                return;
            }
            if (e === t.U) {
                const i = t.J;
                if (i.se === 1) {
                    i.se = 0;
                    t.se = 1;
                    if (t === this.X) {
                        this.X = t.rotateLeft();
                    } else t.rotateLeft();
                } else {
                    if (i.J && i.J.se === 1) {
                        i.se = t.se;
                        t.se = 0;
                        i.J.se = 0;
                        if (t === this.X) {
                            this.X = t.rotateLeft();
                        } else t.rotateLeft();
                        return;
                    } else if (i.U && i.U.se === 1) {
                        i.se = 1;
                        i.U.se = 0;
                        i.rotateRight();
                    } else {
                        i.se = 1;
                        e = t;
                    }
                }
            } else {
                const i = t.U;
                if (i.se === 1) {
                    i.se = 0;
                    t.se = 1;
                    if (t === this.X) {
                        this.X = t.rotateRight();
                    } else t.rotateRight();
                } else {
                    if (i.U && i.U.se === 1) {
                        i.se = t.se;
                        t.se = 0;
                        i.U.se = 0;
                        if (t === this.X) {
                            this.X = t.rotateRight();
                        } else t.rotateRight();
                        return;
                    } else if (i.J && i.J.se === 1) {
                        i.se = 1;
                        i.J.se = 0;
                        i.rotateLeft();
                    } else {
                        i.se = 1;
                        e = t;
                    }
                }
            }
        }
    }
    le(e) {
        if (this.o === 1) {
            this.clear();
            return this.S;
        }
        let t = e;
        while (t.U || t.J) {
            if (t.J) {
                t = t.J;
                while (t.U) t = t.U;
            } else {
                t = t.U;
            }
            [e.T, t.T] = [ t.T, e.T ];
            [e.L, t.L] = [ t.L, e.L ];
            e = t;
        }
        if (this.S.U === t) {
            this.S.U = t.tt;
        } else if (this.S.J === t) {
            this.S.J = t.tt;
        }
        this.oe(t);
        const i = t.tt;
        if (t === i.U) {
            i.U = undefined;
        } else i.J = undefined;
        this.o -= 1;
        this.X.se = 0;
        return i;
    }
    fe(e) {
        while (true) {
            const t = e.tt;
            if (t.se === 0) return;
            const i = t.tt;
            if (t === i.U) {
                const s = i.J;
                if (s && s.se === 1) {
                    s.se = t.se = 0;
                    if (i === this.X) return;
                    i.se = 1;
                    e = i;
                    continue;
                } else if (e === t.J) {
                    e.se = 0;
                    if (e.U) e.U.tt = t;
                    if (e.J) e.J.tt = i;
                    t.J = e.U;
                    i.U = e.J;
                    e.U = t;
                    e.J = i;
                    if (i === this.X) {
                        this.X = e;
                        this.S.tt = e;
                    } else {
                        const t = i.tt;
                        if (t.U === i) {
                            t.U = e;
                        } else t.J = e;
                    }
                    e.tt = i.tt;
                    t.tt = e;
                    i.tt = e;
                    i.se = 1;
                    return {
                        parentNode: t,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    t.se = 0;
                    if (i === this.X) {
                        this.X = i.rotateRight();
                    } else i.rotateRight();
                    i.se = 1;
                }
            } else {
                const s = i.U;
                if (s && s.se === 1) {
                    s.se = t.se = 0;
                    if (i === this.X) return;
                    i.se = 1;
                    e = i;
                    continue;
                } else if (e === t.U) {
                    e.se = 0;
                    if (e.U) e.U.tt = i;
                    if (e.J) e.J.tt = t;
                    i.J = e.U;
                    t.U = e.J;
                    e.U = i;
                    e.J = t;
                    if (i === this.X) {
                        this.X = e;
                        this.S.tt = e;
                    } else {
                        const t = i.tt;
                        if (t.U === i) {
                            t.U = e;
                        } else t.J = e;
                    }
                    e.tt = i.tt;
                    t.tt = e;
                    i.tt = e;
                    i.se = 1;
                    return {
                        parentNode: t,
                        grandParent: i,
                        curNode: e
                    };
                } else {
                    t.se = 0;
                    if (i === this.X) {
                        this.X = i.rotateLeft();
                    } else i.rotateLeft();
                    i.se = 1;
                }
            }
            return;
        }
    }
    he(e, t, i) {
        if (this.X === undefined) {
            this.o += 1;
            this.X = new this.ne(e, t);
            this.X.se = 0;
            this.X.tt = this.S;
            this.S.tt = this.X;
            this.S.U = this.X;
            this.S.J = this.X;
            return;
        }
        let s;
        const r = this.S.U;
        const n = this.p(r.T, e);
        if (n === 0) {
            r.L = t;
            return;
        } else if (n > 0) {
            r.U = new this.ne(e, t);
            r.U.tt = r;
            s = r.U;
            this.S.U = s;
        } else {
            const r = this.S.J;
            const n = this.p(r.T, e);
            if (n === 0) {
                r.L = t;
                return;
            } else if (n < 0) {
                r.J = new this.ne(e, t);
                r.J.tt = r;
                s = r.J;
                this.S.J = s;
            } else {
                if (i !== undefined) {
                    const r = i.I;
                    if (r !== this.S) {
                        const i = this.p(r.T, e);
                        if (i === 0) {
                            r.L = t;
                            return;
                        } else if (i > 0) {
                            const i = r.pre();
                            const n = this.p(i.T, e);
                            if (n === 0) {
                                i.L = t;
                                return;
                            } else if (n < 0) {
                                s = new this.ne(e, t);
                                if (i.J === undefined) {
                                    i.J = s;
                                    s.tt = i;
                                } else {
                                    r.U = s;
                                    s.tt = r;
                                }
                            }
                        }
                    }
                }
                if (s === undefined) {
                    s = this.X;
                    while (true) {
                        const i = this.p(s.T, e);
                        if (i > 0) {
                            if (s.U === undefined) {
                                s.U = new this.ne(e, t);
                                s.U.tt = s;
                                s = s.U;
                                break;
                            }
                            s = s.U;
                        } else if (i < 0) {
                            if (s.J === undefined) {
                                s.J = new this.ne(e, t);
                                s.J.tt = s;
                                s = s.J;
                                break;
                            }
                            s = s.J;
                        } else {
                            s.L = t;
                            return;
                        }
                    }
                }
            }
        }
        this.o += 1;
        return s;
    }
    clear() {
        this.o = 0;
        this.X = undefined;
        this.S.tt = undefined;
        this.S.U = this.S.J = undefined;
    }
    updateKeyByIterator(e, t) {
        const i = e.I;
        if (i === this.S) {
            throw new TypeError("Invalid iterator!");
        }
        if (this.o === 1) {
            i.T = t;
            return true;
        }
        if (i === this.S.U) {
            if (this.p(i.next().T, t) > 0) {
                i.T = t;
                return true;
            }
            return false;
        }
        if (i === this.S.J) {
            if (this.p(i.pre().T, t) < 0) {
                i.T = t;
                return true;
            }
            return false;
        }
        const s = i.pre().T;
        if (this.p(s, t) >= 0) return false;
        const r = i.next().T;
        if (this.p(r, t) <= 0) return false;
        i.T = t;
        return true;
    }
    eraseElementByPos(e) {
        if (e < 0 || e > this.o - 1) {
            throw new RangeError;
        }
        let t = 0;
        this.ie(this.X, (i => {
            if (e === t) {
                this.ue(i);
                return true;
            }
            t += 1;
            return false;
        }));
    }
    re(e, t) {
        while (e) {
            const i = this.p(e.T, t);
            if (i < 0) {
                e = e.J;
            } else if (i > 0) {
                e = e.U;
            } else return e;
        }
        return e;
    }
    eraseElementByKey(e) {
        if (!this.o) return;
        const t = this.re(this.X, e);
        if (t === undefined) return;
        this.ue(t);
    }
    eraseElementByIterator(e) {
        const t = e.I;
        if (t === this.S) {
            throw new RangeError("Invalid iterator");
        }
        if (t.J === undefined) {
            e = e.next();
        }
        this.ue(t);
        return e;
    }
    getHeight() {
        if (!this.o) return 0;
        const traversal = e => {
            if (!e) return 0;
            return Math.max(traversal(e.U), traversal(e.J)) + 1;
        };
        return traversal(this.X);
    }
}

var _default = TreeContainer;

exports.default = _default;