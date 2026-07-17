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
var node_1 = __importDefault(require("./node"));
var types_1 = require("./types");
var ID = /** @class */ (function (_super) {
    __extends(ID, _super);
    function ID(opts) {
        var _this = _super.call(this, opts) || this;
        _this.type = types_1.ID;
        return _this;
    }
    ID.prototype.valueToString = function () {
        return "#" + _super.prototype.valueToString.call(this);
    };
    return ID;
}(node_1.default));
exports.default = ID;
//# sourceMappingURL=id.js.map