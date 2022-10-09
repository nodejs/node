var __extends = this && this.t || function() {
    var extendStatics = function(t, n) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, n) {
            t.__proto__ = n;
        } || function(t, n) {
            for (var i in n) if (Object.prototype.hasOwnProperty.call(n, i)) t[i] = n[i];
        };
        return extendStatics(t, n);
    };
    return function(t, n) {
        if (typeof n !== "function" && n !== null) throw new TypeError("Class extends value " + String(n) + " is not a constructor or null");
        extendStatics(t, n);
        function __() {
            this.constructor = t;
        }
        t.prototype = n === null ? Object.create(n) : (__.prototype = n.prototype, new __);
    };
}();

import { Base } from "../ContainerBase";

var Stack = function(t) {
    __extends(Stack, t);
    function Stack(n) {
        if (n === void 0) {
            n = [];
        }
        var i = t.call(this) || this;
        i.S = [];
        n.forEach((function(t) {
            return i.push(t);
        }));
        return i;
    }
    Stack.prototype.clear = function() {
        this.o = 0;
        this.S.length = 0;
    };
    Stack.prototype.push = function(t) {
        this.S.push(t);
        this.o += 1;
    };
    Stack.prototype.pop = function() {
        this.S.pop();
        if (this.o > 0) this.o -= 1;
    };
    Stack.prototype.top = function() {
        return this.S[this.o - 1];
    };
    return Stack;
}(Base);

export default Stack;