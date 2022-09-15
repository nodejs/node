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
import { ContainerIterator } from "../../ContainerBase/index";
var TreeIterator = /** @class */ (function (_super) {
    __extends(TreeIterator, _super);
    function TreeIterator(node, header, iteratorType) {
        var _this = _super.call(this, iteratorType) || this;
        _this.node = node;
        _this.header = header;
        if (_this.iteratorType === ContainerIterator.NORMAL) {
            _this.pre = function () {
                if (this.node === this.header.left) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre();
                return this;
            };
            _this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next();
                return this;
            };
        }
        else {
            _this.pre = function () {
                if (this.node === this.header.right) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.next();
                return this;
            };
            _this.next = function () {
                if (this.node === this.header) {
                    throw new RangeError('LinkList iterator access denied!');
                }
                this.node = this.node.pre();
                return this;
            };
        }
        return _this;
    }
    TreeIterator.prototype.equals = function (obj) {
        return this.node === obj.node;
    };
    return TreeIterator;
}(ContainerIterator));
export default TreeIterator;
