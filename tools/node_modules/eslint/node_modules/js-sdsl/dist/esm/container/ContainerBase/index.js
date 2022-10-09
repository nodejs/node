var __extends = this && this.t || function() {
    var extendStatics = function(n, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(n, t) {
            n.__proto__ = t;
        } || function(n, t) {
            for (var r in t) if (Object.prototype.hasOwnProperty.call(t, r)) n[r] = t[r];
        };
        return extendStatics(n, t);
    };
    return function(n, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(n, t);
        function __() {
            this.constructor = n;
        }
        n.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

var ContainerIterator = function() {
    function ContainerIterator(n) {
        if (n === void 0) {
            n = 0;
        }
        this.iteratorType = n;
    }
    return ContainerIterator;
}();

export { ContainerIterator };

var Base = function() {
    function Base() {
        this.o = 0;
    }
    Base.prototype.size = function() {
        return this.o;
    };
    Base.prototype.empty = function() {
        return this.o === 0;
    };
    return Base;
}();

export { Base };

var Container = function(n) {
    __extends(Container, n);
    function Container() {
        return n !== null && n.apply(this, arguments) || this;
    }
    return Container;
}(Base);

export { Container };