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
import Deque from '../SequentialContainer/Deque';
import { Base } from "../ContainerBase/index";
var Queue = /** @class */ (function (_super) {
    __extends(Queue, _super);
    function Queue(container) {
        if (container === void 0) { container = []; }
        var _this = _super.call(this) || this;
        _this.queue = new Deque(container);
        _this.length = _this.queue.size();
        return _this;
    }
    Queue.prototype.clear = function () {
        this.queue.clear();
        this.length = 0;
    };
    /**
     * @description Inserts element to queue's end.
     */
    Queue.prototype.push = function (element) {
        this.queue.pushBack(element);
        this.length += 1;
    };
    /**
     * @description Removes the first element.
     */
    Queue.prototype.pop = function () {
        this.queue.popFront();
        if (this.length)
            this.length -= 1;
    };
    /**
     * @description Access the first element.
     */
    Queue.prototype.front = function () {
        return this.queue.front();
    };
    return Queue;
}(Base));
export default Queue;
