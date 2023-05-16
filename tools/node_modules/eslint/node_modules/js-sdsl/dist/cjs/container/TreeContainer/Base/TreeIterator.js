"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../../ContainerBase");

var _throwError = require("../../../utils/throwError");

class TreeIterator extends _ContainerBase.ContainerIterator {
    constructor(t, r, i) {
        super(i);
        this.o = t;
        this.h = r;
        if (this.iteratorType === 0) {
            this.pre = function() {
                if (this.o === this.h.T) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o = this.o.L();
                return this;
            };
            this.next = function() {
                if (this.o === this.h) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o = this.o.B();
                return this;
            };
        } else {
            this.pre = function() {
                if (this.o === this.h.K) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o = this.o.B();
                return this;
            };
            this.next = function() {
                if (this.o === this.h) {
                    (0, _throwError.throwIteratorAccessError)();
                }
                this.o = this.o.L();
                return this;
            };
        }
    }
    get index() {
        let t = this.o;
        const r = this.h.it;
        if (t === this.h) {
            if (r) {
                return r.st - 1;
            }
            return 0;
        }
        let i = 0;
        if (t.T) {
            i += t.T.st;
        }
        while (t !== r) {
            const r = t.it;
            if (t === r.K) {
                i += 1;
                if (r.T) {
                    i += r.T.st;
                }
            }
            t = r;
        }
        return i;
    }
}

var _default = TreeIterator;

exports.default = _default;
//# sourceMappingURL=TreeIterator.js.map
