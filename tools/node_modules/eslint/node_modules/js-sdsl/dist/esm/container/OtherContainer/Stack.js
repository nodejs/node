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
import { Base } from "../ContainerBase/index";
var Stack = /** @class */ (function (_super) {
    __extends(Stack, _super);
    function Stack(container) {
        if (container === void 0) { container = []; }
        var _this = _super.call(this) || this;
        _this.stack = [];
        container.forEach(function (element) { return _this.push(element); });
        return _this;
    }
    Stack.prototype.clear = function () {
        this.length = 0;
        this.stack.length = 0;
    };
    /**
     * @description Insert element to stack's end.
     */
    Stack.prototype.push = function (element) {
        this.stack.push(element);
        this.length += 1;
    };
    /**
     * @description Removes the end element.
     */
    Stack.prototype.pop = function () {
        this.stack.pop();
        if (this.length > 0)
            this.length -= 1;
    };
    /**
     * @description Accesses the end element.
     */
    Stack.prototype.top = function () {
        return this.stack[this.length - 1];
    };
    return Stack;
}(Base));
export default Stack;
