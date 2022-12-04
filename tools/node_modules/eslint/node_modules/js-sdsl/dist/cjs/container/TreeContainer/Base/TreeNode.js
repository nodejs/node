"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.TreeNodeEnableIndex = exports.TreeNode = void 0;

class TreeNode {
    constructor(e, t) {
        this.se = 1;
        this.T = undefined;
        this.L = undefined;
        this.U = undefined;
        this.J = undefined;
        this.tt = undefined;
        this.T = e;
        this.L = t;
    }
    pre() {
        let e = this;
        if (e.se === 1 && e.tt.tt === e) {
            e = e.J;
        } else if (e.U) {
            e = e.U;
            while (e.J) {
                e = e.J;
            }
        } else {
            let t = e.tt;
            while (t.U === e) {
                e = t;
                t = e.tt;
            }
            e = t;
        }
        return e;
    }
    next() {
        let e = this;
        if (e.J) {
            e = e.J;
            while (e.U) {
                e = e.U;
            }
            return e;
        } else {
            let t = e.tt;
            while (t.J === e) {
                e = t;
                t = e.tt;
            }
            if (e.J !== t) {
                return t;
            } else return e;
        }
    }
    rotateLeft() {
        const e = this.tt;
        const t = this.J;
        const s = t.U;
        if (e.tt === this) e.tt = t; else if (e.U === this) e.U = t; else e.J = t;
        t.tt = e;
        t.U = this;
        this.tt = t;
        this.J = s;
        if (s) s.tt = this;
        return t;
    }
    rotateRight() {
        const e = this.tt;
        const t = this.U;
        const s = t.J;
        if (e.tt === this) e.tt = t; else if (e.U === this) e.U = t; else e.J = t;
        t.tt = e;
        t.J = this;
        this.tt = t;
        this.U = s;
        if (s) s.tt = this;
        return t;
    }
}

exports.TreeNode = TreeNode;

class TreeNodeEnableIndex extends TreeNode {
    constructor() {
        super(...arguments);
        this.U = undefined;
        this.J = undefined;
        this.tt = undefined;
        this.et = 1;
    }
    rotateLeft() {
        const e = super.rotateLeft();
        this.recount();
        e.recount();
        return e;
    }
    rotateRight() {
        const e = super.rotateRight();
        this.recount();
        e.recount();
        return e;
    }
    recount() {
        this.et = 1;
        if (this.U) this.et += this.U.et;
        if (this.J) this.et += this.J.et;
    }
}

exports.TreeNodeEnableIndex = TreeNodeEnableIndex;