"use strict";
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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
var container_1 = __importDefault(require("./container"));
var types_1 = require("./types");
var Root = /** @class */ (function (_super) {
    __extends(Root, _super);
    function Root(opts) {
        var _this = _super.call(this, opts) || this;
        _this.type = types_1.ROOT;
        return _this;
    }
    Root.prototype._stringify = function (options, depth, max) {
        var _this = this;
        var str = this.reduce(function (memo, selector) {
            memo.push(_this._stringifyChild(selector, options, depth, max));
            return memo;
        }, []).join(",");
        return this.trailingComma ? str + "," : str;
    };
    Root.prototype.error = function (message, options) {
        if (this._error) {
            return this._error(message, options);
        }
        else {
            return new Error(message);
        }
    };
    Object.defineProperty(Root.prototype, "errorGenerator", {
        set: function (handler) {
            this._error = handler;
        },
        enumerable: false,
        configurable: true
    });
    return Root;
}(container_1.default));
exports.default = Root;
//# sourceMappingURL=root.js.map