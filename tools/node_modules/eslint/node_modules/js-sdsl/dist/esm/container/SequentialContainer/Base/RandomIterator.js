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
import { checkWithinAccessParams } from "../../../utils/checkParams";
import { ContainerIterator } from "../../ContainerBase/index";
var RandomIterator = /** @class */ (function (_super) {
    __extends(RandomIterator, _super);
    function RandomIterator(index, size, getElementByPos, setElementByPos, iteratorType) {
        var _this = _super.call(this, iteratorType) || this;
        _this.node = index;
        _this.size = size;
        _this.getElementByPos = getElementByPos;
        _this.setElementByPos = setElementByPos;
        if (_this.iteratorType === ContainerIterator.NORMAL) {
            _this.pre = function () {
                if (this.node === 0) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node -= 1;
                return this;
            };
            _this.next = function () {
                if (this.node === this.size()) {
                    throw new RangeError('Deque Iterator access denied!');
                }
                this.node += 1;
                return this;
            };
        }
        else {
            _this.pre = function () {
                if (this.node === this.size() - 1) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node += 1;
                return this;
            };
            _this.next = function () {
                if (this.node === -1) {
                    throw new RangeError('Deque iterator access denied!');
                }
                this.node -= 1;
                return this;
            };
        }
        return _this;
    }
    Object.defineProperty(RandomIterator.prototype, "pointer", {
        get: function () {
            checkWithinAccessParams(this.node, 0, this.size() - 1);
            return this.getElementByPos(this.node);
        },
        set: function (newValue) {
            checkWithinAccessParams(this.node, 0, this.size() - 1);
            this.setElementByPos(this.node, newValue);
        },
        enumerable: false,
        configurable: true
    });
    RandomIterator.prototype.equals = function (obj) {
        return this.node === obj.node;
    };
    return RandomIterator;
}(ContainerIterator));
export { RandomIterator };
