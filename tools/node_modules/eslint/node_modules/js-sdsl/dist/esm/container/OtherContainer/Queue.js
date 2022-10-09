var __extends = this && this.t || function() {
    var extendStatics = function(e, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, t) {
            e.__proto__ = t;
        } || function(e, t) {
            for (var n in t) if (Object.prototype.hasOwnProperty.call(t, n)) e[n] = t[n];
        };
        return extendStatics(e, t);
    };
    return function(e, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(e, t);
        function __() {
            this.constructor = e;
        }
        e.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

import Deque from "../SequentialContainer/Deque";

import { Base } from "../ContainerBase";

var Queue = function(e) {
    __extends(Queue, e);
    function Queue(t) {
        if (t === void 0) {
            t = [];
        }
        var n = e.call(this) || this;
        n.q = new Deque(t);
        n.o = n.q.size();
        return n;
    }
    Queue.prototype.clear = function() {
        this.q.clear();
        this.o = 0;
    };
    Queue.prototype.push = function(e) {
        this.q.pushBack(e);
        this.o += 1;
    };
    Queue.prototype.pop = function() {
        this.q.popFront();
        if (this.o) this.o -= 1;
    };
    Queue.prototype.front = function() {
        return this.q.front();
    };
    return Queue;
}(Base);

export default Queue;