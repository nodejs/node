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
var cssesc_1 = __importDefault(require("cssesc"));
var util_1 = require("../util");
var node_1 = __importDefault(require("./node"));
var Namespace = /** @class */ (function (_super) {
    __extends(Namespace, _super);
    function Namespace() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    Object.defineProperty(Namespace.prototype, "namespace", {
        get: function () {
            return this._namespace;
        },
        set: function (namespace) {
            if (namespace === true || namespace === "*" || namespace === "&") {
                this._namespace = namespace;
                if (this.raws) {
                    delete this.raws.namespace;
                }
                return;
            }
            var escaped = (0, cssesc_1.default)(namespace, { isIdentifier: true });
            this._namespace = namespace;
            if (escaped !== namespace) {
                (0, util_1.ensureObject)(this, "raws");
                this.raws.namespace = escaped;
            }
            else if (this.raws) {
                delete this.raws.namespace;
            }
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Namespace.prototype, "ns", {
        get: function () {
            return this._namespace;
        },
        set: function (namespace) {
            this.namespace = namespace;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(Namespace.prototype, "namespaceString", {
        get: function () {
            if (this.namespace) {
                var ns = this.stringifyProperty("namespace");
                if (ns === true) {
                    return "";
                }
                else {
                    return ns;
                }
            }
            else {
                return "";
            }
        },
        enumerable: false,
        configurable: true
    });
    Namespace.prototype.qualifiedName = function (value) {
        if (this.namespace) {
            return "".concat(this.namespaceString, "|").concat(value);
        }
        else {
            return value;
        }
    };
    Namespace.prototype.valueToString = function () {
        return this.qualifiedName(_super.prototype.valueToString.call(this));
    };
    return Namespace;
}(node_1.default));
exports.default = Namespace;
//# sourceMappingURL=namespace.js.map