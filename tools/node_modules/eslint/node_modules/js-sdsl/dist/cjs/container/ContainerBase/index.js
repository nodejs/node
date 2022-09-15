"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Container = exports.Base = exports.ContainerIterator = void 0;
class ContainerIterator {
    constructor(iteratorType = ContainerIterator.NORMAL) {
        this.iteratorType = iteratorType;
    }
}
exports.ContainerIterator = ContainerIterator;
ContainerIterator.NORMAL = false;
ContainerIterator.REVERSE = true;
class Base {
    constructor() {
        /**
         * @description Container's size.
         * @protected
         */
        this.length = 0;
    }
    /**
     * @return The size of the container.
     */
    size() {
        return this.length;
    }
    /**
     * @return Boolean about if the container is empty.
     */
    empty() {
        return this.length === 0;
    }
}
exports.Base = Base;
class Container extends Base {
}
exports.Container = Container;
