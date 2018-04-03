"use strict";

exports.__esModule = true;
exports.TYPES = void 0;

var _toFastProperties = _interopRequireDefault(require("to-fast-properties"));

require("./core");

require("./es2015");

require("./flow");

require("./jsx");

require("./misc");

require("./experimental");

require("./typescript");

var _utils = require("./utils");

exports.VISITOR_KEYS = _utils.VISITOR_KEYS;
exports.ALIAS_KEYS = _utils.ALIAS_KEYS;
exports.FLIPPED_ALIAS_KEYS = _utils.FLIPPED_ALIAS_KEYS;
exports.NODE_FIELDS = _utils.NODE_FIELDS;
exports.BUILDER_KEYS = _utils.BUILDER_KEYS;
exports.DEPRECATED_KEYS = _utils.DEPRECATED_KEYS;

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

(0, _toFastProperties.default)(_utils.VISITOR_KEYS);
(0, _toFastProperties.default)(_utils.ALIAS_KEYS);
(0, _toFastProperties.default)(_utils.FLIPPED_ALIAS_KEYS);
(0, _toFastProperties.default)(_utils.NODE_FIELDS);
(0, _toFastProperties.default)(_utils.BUILDER_KEYS);
(0, _toFastProperties.default)(_utils.DEPRECATED_KEYS);
var TYPES = Object.keys(_utils.VISITOR_KEYS).concat(Object.keys(_utils.FLIPPED_ALIAS_KEYS)).concat(Object.keys(_utils.DEPRECATED_KEYS));
exports.TYPES = TYPES;