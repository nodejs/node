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

var RandomIterator = function(t) {
    __extends(RandomIterator, t);
    function RandomIterator(r, n, e, i, o) {
        var a = t.call(this, o) || this;
        a.D = r;
        a.I = n;
        a.g = e;
        a.R = i;
        if (a.iteratorType === 0) {
            a.pre = function() {
                if (this.D === 0) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.D -= 1;
                return this;
            };
            a.next = function() {
                if (this.D === this.I()) {
                    throw new RangeError("Random Iterator access denied!");
                }
                this.D += 1;
                return this;
            };
        } else {
            a.pre = function() {
                if (this.D === this.I() - 1) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.D += 1;
                return this;
            };
            a.next = function() {
                if (this.D === -1) {
                    throw new RangeError("Random iterator access denied!");
                }
                this.D -= 1;
                return this;
            };
        }
        return a;
    }
    Object.defineProperty(RandomIterator.prototype, "pointer", {
        get: function() {
            if (this.D < 0 || this.D > this.I() - 1) {
                throw new RangeError;
            }
            return this.g(this.D);
        },
        set: function(t) {
            if (this.D < 0 || this.D > this.I() - 1) {
                throw new RangeError;
            }
            this.R(this.D, t);
        },
        enumerable: false,
        configurable: true
    });
    RandomIterator.prototype.equals = function(t) {
        return this.D === t.D;
    };
    return RandomIterator;
}(ContainerIterator);

export { RandomIterator };