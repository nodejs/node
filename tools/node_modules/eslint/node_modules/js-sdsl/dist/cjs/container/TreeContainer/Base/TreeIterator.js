"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../../ContainerBase");

class TreeIterator extends _ContainerBase.ContainerIterator {
    constructor(t, e, r) {
        super(r);
        this.I = t;
        this.S = e;
        if (this.iteratorType === 0) {
            this.pre = function() {
                if (this.I === this.S.U) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.I = this.I.pre();
                return this;
            };
            this.next = function() {
                if (this.I === this.S) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.I = this.I.next();
                return this;
            };
        } else {
            this.pre = function() {
                if (this.I === this.S.J) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.I = this.I.next();
                return this;
            };
            this.next = function() {
                if (this.I === this.S) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.I = this.I.pre();
                return this;
            };
        }
    }
    get index() {
        let t = this.I;
        const e = this.S.tt;
        if (t === this.S) {
            if (e) {
                return e.et - 1;
            }
            return 0;
        }
        let r = 0;
        if (t.U) {
            r += t.U.et;
        }
        while (t !== e) {
            const e = t.tt;
            if (t === e.J) {
                r += 1;
                if (e.U) {
                    r += e.U.et;
                }
            }
            t = e;
        }
        return r;
    }
    equals(t) {
        return this.I === t.I;
    }
}

var _default = TreeIterator;

exports.default = _default;