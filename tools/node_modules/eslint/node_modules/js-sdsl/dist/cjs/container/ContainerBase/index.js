"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.ContainerIterator = exports.Container = exports.Base = void 0;

class ContainerIterator {
    constructor(t = 0) {
        this.iteratorType = t;
    }
}

exports.ContainerIterator = ContainerIterator;

class Base {
    constructor() {
        this.o = 0;
    }
    size() {
        return this.o;
    }
    empty() {
        return this.o === 0;
    }
}

exports.Base = Base;

class Container extends Base {}

exports.Container = Container;