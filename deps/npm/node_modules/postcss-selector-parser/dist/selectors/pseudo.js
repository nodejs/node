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
var Pseudo = /** @class */ (function (_super) {
    __extends(Pseudo, _super);
    function Pseudo(opts) {
        var _this = _super.call(this, opts) || this;
        _this.type = types_1.PSEUDO;
        return _this;
    }
    Pseudo.prototype._stringify = function (options, depth, max) {
        var _this = this;
        if (depth >= max) {
            throw new Error("Cannot serialize selector: nesting depth exceeds the maximum of ".concat(max, "."));
        }
        var params = this.length
            ? "(" +
                this.map(function (child) { return _this._stringifyChild(child, options, depth + 1, max); }).join(",") +
                ")"
            : "";
        return [this.rawSpaceBefore, this.stringifyProperty("value"), params, this.rawSpaceAfter].join("");
    };
    return Pseudo;
}(container_1.default));
exports.default = Pseudo;
//# sourceMappingURL=pseudo.js.map