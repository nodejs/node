"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.TreeNodeEnableIndex = exports.TreeNode = void 0;

class TreeNode {
    constructor(e, t) {
        this.ee = 1;
        this.u = undefined;
        this.l = undefined;
        this.Y = undefined;
        this.Z = undefined;
        this.tt = undefined;
        this.u = e;
        this.l = t;
    }
    L() {
        let e = this;
        if (e.ee === 1 && e.tt.tt === e) {
            e = e.Z;
        } else if (e.Y) {
            e = e.Y;
            while (e.Z) {
                e = e.Z;
            }
        } else {
            let t = e.tt;
            while (t.Y === e) {
                e = t;
                t = e.tt;
            }
            e = t;
        }
        return e;
    }
    B() {
        let e = this;
        if (e.Z) {
            e = e.Z;
            while (e.Y) {
                e = e.Y;
            }
            return e;
        } else {
            let t = e.tt;
            while (t.Z === e) {
                e = t;
                t = e.tt;
            }
            if (e.Z !== t) {
                return t;
            } else return e;
        }
    }
    te() {
        const e = this.tt;
        const t = this.Z;
        const s = t.Y;
        if (e.tt === this) e.tt = t; else if (e.Y === this) e.Y = t; else e.Z = t;
        t.tt = e;
        t.Y = this;
        this.tt = t;
        this.Z = s;
        if (s) s.tt = this;
        return t;
    }
    se() {
        const e = this.tt;
        const t = this.Y;
        const s = t.Z;
        if (e.tt === this) e.tt = t; else if (e.Y === this) e.Y = t; else e.Z = t;
        t.tt = e;
        t.Z = this;
        this.tt = t;
        this.Y = s;
        if (s) s.tt = this;
        return t;
    }
}

exports.TreeNode = TreeNode;

class TreeNodeEnableIndex extends TreeNode {
    constructor() {
        super(...arguments);
        this.rt = 1;
    }
    te() {
        const e = super.te();
        this.ie();
        e.ie();
        return e;
    }
    se() {
        const e = super.se();
        this.ie();
        e.ie();
        return e;
    }
    ie() {
        this.rt = 1;
        if (this.Y) {
            this.rt += this.Y.rt;
        }
        if (this.Z) {
            this.rt += this.Z.rt;
        }
    }
}

exports.TreeNodeEnableIndex = TreeNodeEnableIndex;
//# sourceMappingURL=TreeNode.js.map
