"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.RandomIterator = void 0;

var _ContainerBase = require("../../ContainerBase");

var _throwError = require("../../../utils/throwError");

class RandomIterator extends _ContainerBase.ContainerIterator {
    constructor(t, r, i, s, h) {
        super(h);
        this.o = t;
        this.D = r;
        this.R = i;
        this.N = s;
        if (this.iteratorType === 0) {
            this.pre = function() {
                if (this.o === 0) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o -= 1;
                return this;
            };
            this.next = function() {
                if (this.o === this.D()) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o += 1;
                return this;
            };
        } else {
            this.pre = function() {
                if (this.o === this.D() - 1) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o += 1;
                return this;
            };
            this.next = function() {
                if (this.o === -1) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o -= 1;
                return this;
            };
        }
    }
    get pointer() {
        if (this.o < 0 || this.o > this.D() - 1) {
            throw new RangeError;
        }
        return this.R(this.o);
    }
    set pointer(t) {
        if (this.o < 0 || this.o > this.D() - 1) {
            throw new RangeError;
        }
        this.N(this.o, t);
    }
}

exports.RandomIterator = RandomIterator;
//# sourceMappingURL=RandomIterator.js.map
