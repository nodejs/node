"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.RandomIterator = void 0;

var _ContainerBase = require("../../ContainerBase");

class RandomIterator extends _ContainerBase.ContainerIterator {
    constructor(t, r, e, i, s) {
        super(s);
        this.I = t;
        this.D = r;
        this.g = e;
        this.m = i;
        if (this.iteratorType === 0) {
            this.pre = function() {
                if (this.I === 0) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.I -= 1;
                return this;
            };
            this.next = function() {
                if (this.I === this.D()) {
                    throw new RangeError("Random Iterator access denied!");
                }
                this.I += 1;
                return this;
            };
        } else {
            this.pre = function() {
                if (this.I === this.D() - 1) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.I += 1;
                return this;
            };
            this.next = function() {
                if (this.I === -1) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.I -= 1;
                return this;
            };
        }
    }
    get pointer() {
        if (this.I < 0 || this.I > this.D() - 1) {
            throw new RangeError;
        }
        return this.g(this.I);
    }
    set pointer(t) {
        if (this.I < 0 || this.I > this.D() - 1) {
            throw new RangeError;
        }
        this.m(this.I, t);
    }
    equals(t) {
        return this.I === t.I;
    }
}

exports.RandomIterator = RandomIterator;