"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _createClass2 = require("babel-runtime/helpers/createClass");

var _createClass3 = _interopRequireDefault(_createClass2);

var _location = require("../util/location");

var _context = require("./context");

var _types = require("./types");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var State = function () {
  function State() {
    (0, _classCallCheck3.default)(this, State);
  }

  (0, _createClass3.default)(State, [{
    key: "init",
    value: function init(options, input) {
      this.strict = options.strictMode === false ? false : options.sourceType === "module";

      this.input = input;

      this.potentialArrowAt = -1;

      this.inMethod = this.inFunction = this.inGenerator = this.inAsync = false;

      this.labels = [];

      this.decorators = [];

      this.tokens = [];

      this.comments = [];

      this.trailingComments = [];
      this.leadingComments = [];
      this.commentStack = [];

      this.pos = this.lineStart = 0;
      this.curLine = 1;

      this.type = _types.types.eof;
      this.value = null;
      this.start = this.end = this.pos;
      this.startLoc = this.endLoc = this.curPosition();

      this.lastTokEndLoc = this.lastTokStartLoc = null;
      this.lastTokStart = this.lastTokEnd = this.pos;

      this.context = [_context.types.braceStatement];
      this.exprAllowed = true;

      this.containsEsc = this.containsOctal = false;
      this.octalPosition = null;

      return this;
    }

    // TODO


    // TODO


    // Used to signify the start of a potential arrow function


    // Flags to track whether we are in a function, a generator.


    // Labels in scope.


    // Leading decorators.


    // Token store.


    // Comment store.


    // Comment attachment store


    // The current position of the tokenizer in the input.


    // Properties of the current token:
    // Its type


    // For tokens that include more information than their type, the value


    // Its start and end offset


    // And, if locations are used, the {line, column} object
    // corresponding to those offsets


    // Position information for the previous token


    // The context stack is used to superficially track syntactic
    // context to predict whether a regular expression is allowed in a
    // given position.


    // Used to signal to callers of `readWord1` whether the word
    // contained any escape sequences. This is needed because words with
    // escape sequences must not be interpreted as keywords.


    // TODO

  }, {
    key: "curPosition",
    value: function curPosition() {
      return new _location.Position(this.curLine, this.pos - this.lineStart);
    }
  }, {
    key: "clone",
    value: function clone(skipArrays) {
      var state = new State();
      for (var key in this) {
        var val = this[key];

        if ((!skipArrays || key === "context") && Array.isArray(val)) {
          val = val.slice();
        }

        state[key] = val;
      }
      return state;
    }
  }]);
  return State;
}();

exports.default = State;