"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _types = require("../tokenizer/types");
var _default = superClass => class V8IntrinsicMixin extends superClass {
  parseV8Intrinsic() {
    if (this.match(54)) {
      const v8IntrinsicStartLoc = this.state.startLoc;
      const node = this.startNode();
      this.next();
      if ((0, _types.tokenIsIdentifier)(this.state.type)) {
        const name = this.parseIdentifierName();
        const identifier = this.createIdentifier(node, name);
        identifier.type = "V8IntrinsicIdentifier";
        if (this.match(10)) {
          return identifier;
        }
      }
      this.unexpected(v8IntrinsicStartLoc);
    }
  }
  parseExprAtom(refExpressionErrors) {
    return this.parseV8Intrinsic() || super.parseExprAtom(refExpressionErrors);
  }
};
exports.default = _default;

//# sourceMappingURL=v8intrinsic.js.map
