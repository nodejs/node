var __extends = this && this.t || function() {
    var extendStatics = function(t, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, r) {
            t.__proto__ = r;
        } || function(t, r) {
            for (var n in r) if (Object.prototype.hasOwnProperty.call(r, n)) t[n] = r[n];
        };
        return extendStatics(t, r);
    };
    return function(t, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(t, r);
        function __() {
            this.constructor = t;
        }
        t.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

import { ContainerIterator } from "../../ContainerBase";

import { throwIteratorAccessError } from "../../../utils/throwError";

var RandomIterator = function(t) {
    __extends(RandomIterator, t);
    function RandomIterator(r, n, o, i, e) {
        var s = t.call(this, e) || this;
        s.o = r;
        s.D = n;
        s.R = o;
        s.C = i;
        if (s.iteratorType === 0) {
            s.pre = function() {
                if (this.o === 0) {
                    throwIteratorAccessError();
                }
                this.o -= 1;
                return this;
            };
            s.next = function() {
                if (this.o === this.D()) {
                    throwIteratorAccessError();
                }
                this.o += 1;
                return this;
            };
        } else {
            s.pre = function() {
                if (this.o === this.D() - 1) {
                    throwIteratorAccessError();
                }
                this.o += 1;
                return this;
            };
            s.next = function() {
                if (this.o === -1) {
                    throwIteratorAccessError();
                }
                this.o -= 1;
                return this;
            };
        }
        return s;
    }
    Object.defineProperty(RandomIterator.prototype, "pointer", {
        get: function() {
            if (this.o < 0 || this.o > this.D() - 1) {
                throw new RangeError;
            }
            return this.R(this.o);
        },
        set: function(t) {
            if (this.o < 0 || this.o > this.D() - 1) {
                throw new RangeError;
            }
            this.C(this.o, t);
        },
        enumerable: false,
        configurable: true
    });
    return RandomIterator;
}(ContainerIterator);

export { RandomIterator };
//# sourceMappingURL=RandomIterator.js.map
