"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _location = require("../util/location");
var _context = require("./context");
var _types = require("./types");
class State {
  constructor() {
    this.strict = void 0;
    this.curLine = void 0;
    this.lineStart = void 0;
    this.startLoc = void 0;
    this.endLoc = void 0;
    this.errors = [];
    this.potentialArrowAt = -1;
    this.noArrowAt = [];
    this.noArrowParamsConversionAt = [];
    this.maybeInArrowParameters = false;
    this.inType = false;
    this.noAnonFunctionType = false;
    this.hasFlowComment = false;
    this.isAmbientContext = false;
    this.inAbstractClass = false;
    this.inDisallowConditionalTypesContext = false;
    this.topicContext = {
      maxNumOfResolvableTopics: 0,
      maxTopicIndex: null
    };
    this.soloAwait = false;
    this.inFSharpPipelineDirectBody = false;
    this.labels = [];
    this.comments = [];
    this.commentStack = [];
    this.pos = 0;
    this.type = 137;
    this.value = null;
    this.start = 0;
    this.end = 0;
    this.lastTokEndLoc = null;
    this.lastTokStartLoc = null;
    this.lastTokStart = 0;
    this.context = [_context.types.brace];
    this.canStartJSXElement = true;
    this.containsEsc = false;
    this.firstInvalidTemplateEscapePos = null;
    this.strictErrors = new Map();
    this.tokensLength = 0;
  }
  init({
    strictMode,
    sourceType,
    startLine,
    startColumn
  }) {
    this.strict = strictMode === false ? false : strictMode === true ? true : sourceType === "module";
    this.curLine = startLine;
    this.lineStart = -startColumn;
    this.startLoc = this.endLoc = new _location.Position(startLine, startColumn, 0);
  }
  curPosition() {
    return new _location.Position(this.curLine, this.pos - this.lineStart, this.pos);
  }
  clone(skipArrays) {
    const state = new State();
    const keys = Object.keys(this);
    for (let i = 0, length = keys.length; i < length; i++) {
      const key = keys[i];
      let val = this[key];
      if (!skipArrays && Array.isArray(val)) {
        val = val.slice();
      }
      state[key] = val;
    }
    return state;
  }
}
exports.default = State;

//# sourceMappingURL=state.js.map
