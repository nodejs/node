var __extends = this && this.t || function() {
    var extendStatics = function(t, i) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, i) {
            t.__proto__ = i;
        } || function(t, i) {
            for (var r in i) if (Object.prototype.hasOwnProperty.call(i, r)) t[r] = i[r];
        };
        return extendStatics(t, i);
    };
    return function(t, i) {
        if (typeof i !== "function" && i !== null) throw new TypeError("Class extends value " + String(i) + " is not a constructor or null");
        extendStatics(t, i);
        function __() {
            this.constructor = t;
        }
        t.prototype = i === null ? Object.create(i) : (__.prototype = i.prototype, new __);
    };
}();

import { Container, ContainerIterator } from "../../ContainerBase";

import checkObject from "../../../utils/checkObject";

import { throwIteratorAccessError } from "../../../utils/throwError";

var HashContainerIterator = function(t) {
    __extends(HashContainerIterator, t);
    function HashContainerIterator(i, r, e) {
        var n = t.call(this, e) || this;
        n.o = i;
        n.h = r;
        if (n.iteratorType === 0) {
            n.pre = function() {
                if (this.o.W === this.h) {
                    throwIteratorAccessError();
                }
                this.o = this.o.W;
                return this;
            };
            n.next = function() {
                if (this.o === this.h) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m;
                return this;
            };
        } else {
            n.pre = function() {
                if (this.o.m === this.h) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m;
                return this;
            };
            n.next = function() {
                if (this.o === this.h) {
                    throwIteratorAccessError();
                }
                this.o = this.o.W;
                return this;
            };
        }
        return n;
    }
    return HashContainerIterator;
}(ContainerIterator);

export { HashContainerIterator };

var HashContainer = function(t) {
    __extends(HashContainer, t);
    function HashContainer() {
        var i = t.call(this) || this;
        i._ = [];
        i.I = {};
        i.HASH_TAG = Symbol("@@HASH_TAG");
        Object.setPrototypeOf(i.I, null);
        i.h = {};
        i.h.W = i.h.m = i.l = i.M = i.h;
        return i;
    }
    HashContainer.prototype.X = function(t) {
        var i = t.W, r = t.m;
        i.m = r;
        r.W = i;
        if (t === this.l) {
            this.l = r;
        }
        if (t === this.M) {
            this.M = i;
        }
        this.i -= 1;
    };
    HashContainer.prototype.v = function(t, i, r) {
        if (r === undefined) r = checkObject(t);
        var e;
        if (r) {
            var n = t[this.HASH_TAG];
            if (n !== undefined) {
                this._[n].H = i;
                return this.i;
            }
            Object.defineProperty(t, this.HASH_TAG, {
                value: this._.length,
                configurable: true
            });
            e = {
                p: t,
                H: i,
                W: this.M,
                m: this.h
            };
            this._.push(e);
        } else {
            var s = this.I[t];
            if (s) {
                s.H = i;
                return this.i;
            }
            e = {
                p: t,
                H: i,
                W: this.M,
                m: this.h
            };
            this.I[t] = e;
        }
        if (this.i === 0) {
            this.l = e;
            this.h.m = e;
        } else {
            this.M.m = e;
        }
        this.M = e;
        this.h.W = e;
        return ++this.i;
    };
    HashContainer.prototype.g = function(t, i) {
        if (i === undefined) i = checkObject(t);
        if (i) {
            var r = t[this.HASH_TAG];
            if (r === undefined) return this.h;
            return this._[r];
        } else {
            return this.I[t] || this.h;
        }
    };
    HashContainer.prototype.clear = function() {
        var t = this.HASH_TAG;
        this._.forEach((function(i) {
            delete i.p[t];
        }));
        this._ = [];
        this.I = {};
        Object.setPrototypeOf(this.I, null);
        this.i = 0;
        this.l = this.M = this.h.W = this.h.m = this.h;
    };
    HashContainer.prototype.eraseElementByKey = function(t, i) {
        var r;
        if (i === undefined) i = checkObject(t);
        if (i) {
            var e = t[this.HASH_TAG];
            if (e === undefined) return false;
            delete t[this.HASH_TAG];
            r = this._[e];
            delete this._[e];
        } else {
            r = this.I[t];
            if (r === undefined) return false;
            delete this.I[t];
        }
        this.X(r);
        return true;
    };
    HashContainer.prototype.eraseElementByIterator = function(t) {
        var i = t.o;
        if (i === this.h) {
            throwIteratorAccessError();
        }
        this.X(i);
        return t.next();
    };
    HashContainer.prototype.eraseElementByPos = function(t) {
        if (t < 0 || t > this.i - 1) {
            throw new RangeError;
        }
        var i = this.l;
        while (t--) {
            i = i.m;
        }
        this.X(i);
        return this.i;
    };
    return HashContainer;
}(Container);

export { HashContainer };
//# sourceMappingURL=index.js.map
