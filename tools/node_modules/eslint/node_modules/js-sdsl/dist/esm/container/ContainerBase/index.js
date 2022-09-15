var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var ContainerIterator = /** @class */ (function () {
    function ContainerIterator(iteratorType) {
        if (iteratorType === void 0) { iteratorType = ContainerIterator.NORMAL; }
        this.iteratorType = iteratorType;
    }
    ContainerIterator.NORMAL = false;
    ContainerIterator.REVERSE = true;
    return ContainerIterator;
}());
export { ContainerIterator };
var Base = /** @class */ (function () {
    function Base() {
        /**
         * @description Container's size.
         * @protected
         */
        this.length = 0;
    }
    /**
     * @return The size of the container.
     */
    Base.prototype.size = function () {
        return this.length;
    };
    /**
     * @return Boolean about if the container is empty.
     */
    Base.prototype.empty = function () {
        return this.length === 0;
    };
    return Base;
}());
export { Base };
var Container = /** @class */ (function (_super) {
    __extends(Container, _super);
    function Container() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    return Container;
}(Base));
export { Container };
