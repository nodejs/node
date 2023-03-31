"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.TreeNodeEnableIndex = exports.TreeNode = void 0;

class TreeNode {
    constructor(e, t, s = 1) {
        this.T = undefined;
        this.K = undefined;
        this.it = undefined;
        this.u = e;
        this.l = t;
        this.ee = s;
    }
    L() {
        let e = this;
        if (e.ee === 1 && e.it.it === e) {
            e = e.K;
        } else if (e.T) {
            e = e.T;
            while (e.K) {
                e = e.K;
            }
        } else {
            let t = e.it;
            while (t.T === e) {
                e = t;
                t = e.it;
            }
            e = t;
        }
        return e;
    }
    B() {
        let e = this;
        if (e.K) {
            e = e.K;
            while (e.T) {
                e = e.T;
            }
            return e;
        } else {
            let t = e.it;
            while (t.K === e) {
                e = t;
                t = e.it;
            }
            if (e.K !== t) {
                return t;
            } else return e;
        }
    }
    te() {
        const e = this.it;
        const t = this.K;
        const s = t.T;
        if (e.it === this) e.it = t; else if (e.T === this) e.T = t; else e.K = t;
        t.it = e;
        t.T = this;
        this.it = t;
        this.K = s;
        if (s) s.it = this;
        return t;
    }
    se() {
        const e = this.it;
        const t = this.T;
        const s = t.K;
        if (e.it === this) e.it = t; else if (e.T === this) e.T = t; else e.K = t;
        t.it = e;
        t.K = this;
        this.it = t;
        this.T = s;
        if (s) s.it = this;
        return t;
    }
}

exports.TreeNode = TreeNode;

class TreeNodeEnableIndex extends TreeNode {
    constructor() {
        super(...arguments);
        this.st = 1;
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
        this.st = 1;
        if (this.T) {
            this.st += this.T.st;
        }
        if (this.K) {
            this.st += this.K.st;
        }
    }
}

exports.TreeNodeEnableIndex = TreeNodeEnableIndex;
//# sourceMappingURL=TreeNode.js.map
