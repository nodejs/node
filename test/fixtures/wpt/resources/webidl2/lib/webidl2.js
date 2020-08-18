(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else if(typeof exports === 'object')
		exports["WebIDL2"] = factory();
	else
		root["WebIDL2"] = factory();
})(this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _lib_webidl2_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "parse", function() { return _lib_webidl2_js__WEBPACK_IMPORTED_MODULE_0__["parse"]; });

/* harmony import */ var _lib_writer_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(30);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "write", function() { return _lib_writer_js__WEBPACK_IMPORTED_MODULE_1__["write"]; });

/* harmony import */ var _lib_validator_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(31);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "validate", function() { return _lib_validator_js__WEBPACK_IMPORTED_MODULE_2__["validate"]; });

/* harmony import */ var _lib_tokeniser_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(2);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "WebIDLParseError", function() { return _lib_tokeniser_js__WEBPACK_IMPORTED_MODULE_3__["WebIDLParseError"]; });







/***/ }),
/* 1 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "parse", function() { return parse; });
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var _productions_enum_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(15);
/* harmony import */ var _productions_includes_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(16);
/* harmony import */ var _productions_extended_attributes_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var _productions_typedef_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(17);
/* harmony import */ var _productions_callback_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(18);
/* harmony import */ var _productions_interface_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(19);
/* harmony import */ var _productions_mixin_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(25);
/* harmony import */ var _productions_dictionary_js__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(26);
/* harmony import */ var _productions_namespace_js__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(28);
/* harmony import */ var _productions_callback_interface_js__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(29);
/* harmony import */ var _productions_helpers_js__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(4);















/**
 * @param {Tokeniser} tokeniser
 * @param {object} options
 * @param {boolean} [options.concrete]
 */
function parseByTokens(tokeniser, options) {
  const source = tokeniser.source;

  function error(str) {
    tokeniser.error(str);
  }

  function consume(...candidates) {
    return tokeniser.consume(...candidates);
  }

  function callback() {
    const callback = consume("callback");
    if (!callback) return;
    if (tokeniser.probe("interface")) {
      return _productions_callback_interface_js__WEBPACK_IMPORTED_MODULE_10__["CallbackInterface"].parse(tokeniser, callback);
    }
    return _productions_callback_js__WEBPACK_IMPORTED_MODULE_5__["CallbackFunction"].parse(tokeniser, callback);
  }

  function interface_(opts) {
    const base = consume("interface");
    if (!base) return;
    const ret = _productions_mixin_js__WEBPACK_IMPORTED_MODULE_7__["Mixin"].parse(tokeniser, base, opts) ||
      _productions_interface_js__WEBPACK_IMPORTED_MODULE_6__["Interface"].parse(tokeniser, base, opts) ||
      error("Interface has no proper body");
    return ret;
  }

  function partial() {
    const partial = consume("partial");
    if (!partial) return;
    return _productions_dictionary_js__WEBPACK_IMPORTED_MODULE_8__["Dictionary"].parse(tokeniser, { partial }) ||
      interface_({ partial }) ||
      _productions_namespace_js__WEBPACK_IMPORTED_MODULE_9__["Namespace"].parse(tokeniser, { partial }) ||
      error("Partial doesn't apply to anything");
  }

  function definition() {
    return callback() ||
      interface_() ||
      partial() ||
      _productions_dictionary_js__WEBPACK_IMPORTED_MODULE_8__["Dictionary"].parse(tokeniser) ||
      _productions_enum_js__WEBPACK_IMPORTED_MODULE_1__["Enum"].parse(tokeniser) ||
      _productions_typedef_js__WEBPACK_IMPORTED_MODULE_4__["Typedef"].parse(tokeniser) ||
      _productions_includes_js__WEBPACK_IMPORTED_MODULE_2__["Includes"].parse(tokeniser) ||
      _productions_namespace_js__WEBPACK_IMPORTED_MODULE_9__["Namespace"].parse(tokeniser);
  }

  function definitions() {
    if (!source.length) return [];
    const defs = [];
    while (true) {
      const ea = _productions_extended_attributes_js__WEBPACK_IMPORTED_MODULE_3__["ExtendedAttributes"].parse(tokeniser);
      const def = definition();
      if (!def) {
        if (ea.length) error("Stray extended attributes");
        break;
      }
      Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_11__["autoParenter"])(def).extAttrs = ea;
      defs.push(def);
    }
    const eof = consume("eof");
    if (options.concrete) {
      defs.push(eof);
    }
    return defs;
  }
  const res = definitions();
  if (tokeniser.position < source.length) error("Unrecognised tokens");
  return res;
}

/**
 * @param {string} str
 * @param {object} [options]
 * @param {*} [options.sourceName]
 * @param {boolean} [options.concrete]
 */
function parse(str, options = {}) {
  const tokeniser = new _tokeniser_js__WEBPACK_IMPORTED_MODULE_0__["Tokeniser"](str);
  if (typeof options.sourceName !== "undefined") {
    tokeniser.source.name = options.sourceName;
  }
  return parseByTokens(tokeniser, options);
}


/***/ }),
/* 2 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "typeNameKeywords", function() { return typeNameKeywords; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "stringTypes", function() { return stringTypes; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "argumentNameKeywords", function() { return argumentNameKeywords; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Tokeniser", function() { return Tokeniser; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "WebIDLParseError", function() { return WebIDLParseError; });
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(3);
/* harmony import */ var _productions_helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



// These regular expressions use the sticky flag so they will only match at
// the current location (ie. the offset of lastIndex).
const tokenRe = {
  // This expression uses a lookahead assertion to catch false matches
  // against integers early.
  "decimal": /-?(?=[0-9]*\.|[0-9]+[eE])(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][-+]?[0-9]+)?|[0-9]+[Ee][-+]?[0-9]+)/y,
  "integer": /-?(0([Xx][0-9A-Fa-f]+|[0-7]*)|[1-9][0-9]*)/y,
  "identifier": /[_-]?[A-Za-z][0-9A-Z_a-z-]*/y,
  "string": /"[^"]*"/y,
  "whitespace": /[\t\n\r ]+/y,
  "comment": /((\/(\/.*|\*([^*]|\*[^/])*\*\/)[\t\n\r ]*)+)/y,
  "other": /[^\t\n\r 0-9A-Za-z]/y
};

const typeNameKeywords = [
  "ArrayBuffer",
  "DataView",
  "Int8Array",
  "Int16Array",
  "Int32Array",
  "Uint8Array",
  "Uint16Array",
  "Uint32Array",
  "Uint8ClampedArray",
  "Float32Array",
  "Float64Array",
  "any",
  "object",
  "symbol"
];

const stringTypes = [
  "ByteString",
  "DOMString",
  "USVString"
];

const argumentNameKeywords = [
  "async",
  "attribute",
  "callback",
  "const",
  "constructor",
  "deleter",
  "dictionary",
  "enum",
  "getter",
  "includes",
  "inherit",
  "interface",
  "iterable",
  "maplike",
  "namespace",
  "partial",
  "required",
  "setlike",
  "setter",
  "static",
  "stringifier",
  "typedef",
  "unrestricted"
];

const nonRegexTerminals = [
  "-Infinity",
  "FrozenArray",
  "Infinity",
  "NaN",
  "Promise",
  "boolean",
  "byte",
  "double",
  "false",
  "float",
  "long",
  "mixin",
  "null",
  "octet",
  "optional",
  "or",
  "readonly",
  "record",
  "sequence",
  "short",
  "true",
  "unsigned",
  "void"
].concat(argumentNameKeywords, stringTypes, typeNameKeywords);

const punctuations = [
  "(",
  ")",
  ",",
  "...",
  ":",
  ";",
  "<",
  "=",
  ">",
  "?",
  "[",
  "]",
  "{",
  "}"
];

const reserved = [
  // "constructor" is now a keyword
  "_constructor",
  "toString",
  "_toString",
];

/**
 * @typedef {ArrayItemType<ReturnType<typeof tokenise>>} Token
 * @param {string} str
 */
function tokenise(str) {
  const tokens = [];
  let lastCharIndex = 0;
  let trivia = "";
  let line = 1;
  let index = 0;
  while (lastCharIndex < str.length) {
    const nextChar = str.charAt(lastCharIndex);
    let result = -1;

    if (/[\t\n\r ]/.test(nextChar)) {
      result = attemptTokenMatch("whitespace", { noFlushTrivia: true });
    } else if (nextChar === '/') {
      result = attemptTokenMatch("comment", { noFlushTrivia: true });
    }

    if (result !== -1) {
      const currentTrivia = tokens.pop().value;
      line += (currentTrivia.match(/\n/g) || []).length;
      trivia += currentTrivia;
      index -= 1;
    } else if (/[-0-9.A-Z_a-z]/.test(nextChar)) {
      result = attemptTokenMatch("decimal");
      if (result === -1) {
        result = attemptTokenMatch("integer");
      }
      if (result === -1) {
        result = attemptTokenMatch("identifier");
        const lastIndex = tokens.length - 1;
        const token = tokens[lastIndex];
        if (result !== -1) {
          if (reserved.includes(token.value)) {
            const message = `${Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(token.value)} is a reserved identifier and must not be used.`;
            throw new WebIDLParseError(Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["syntaxError"])(tokens, lastIndex, null, message));
          } else if (nonRegexTerminals.includes(token.value)) {
            token.type = token.value;
          }
        }
      }
    } else if (nextChar === '"') {
      result = attemptTokenMatch("string");
    }

    for (const punctuation of punctuations) {
      if (str.startsWith(punctuation, lastCharIndex)) {
        tokens.push({ type: punctuation, value: punctuation, trivia, line, index });
        trivia = "";
        lastCharIndex += punctuation.length;
        result = lastCharIndex;
        break;
      }
    }

    // other as the last try
    if (result === -1) {
      result = attemptTokenMatch("other");
    }
    if (result === -1) {
      throw new Error("Token stream not progressing");
    }
    lastCharIndex = result;
    index += 1;
  }

  // remaining trivia as eof
  tokens.push({
    type: "eof",
    value: "",
    trivia
  });

  return tokens;

  /**
   * @param {keyof typeof tokenRe} type
   * @param {object} options
   * @param {boolean} [options.noFlushTrivia]
   */
  function attemptTokenMatch(type, { noFlushTrivia } = {}) {
    const re = tokenRe[type];
    re.lastIndex = lastCharIndex;
    const result = re.exec(str);
    if (result) {
      tokens.push({ type, value: result[0], trivia, line, index });
      if (!noFlushTrivia) {
        trivia = "";
      }
      return re.lastIndex;
    }
    return -1;
  }
}

class Tokeniser {
  /**
   * @param {string} idl
   */
  constructor(idl) {
    this.source = tokenise(idl);
    this.position = 0;
  }

  /**
   * @param {string} message
   * @return {never}
   */
  error(message) {
    throw new WebIDLParseError(Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["syntaxError"])(this.source, this.position, this.current, message));
  }

  /**
   * @param {string} type
   */
  probe(type) {
    return this.source.length > this.position && this.source[this.position].type === type;
  }

  /**
   * @param  {...string} candidates
   */
  consume(...candidates) {
    for (const type of candidates) {
      if (!this.probe(type)) continue;
      const token = this.source[this.position];
      this.position++;
      return token;
    }
  }

  /**
   * @param {number} position
   */
  unconsume(position) {
    this.position = position;
  }
}

class WebIDLParseError extends Error {
  /**
   * @param {object} options
   * @param {string} options.message
   * @param {string} options.bareMessage
   * @param {string} options.context
   * @param {number} options.line
   * @param {*} options.sourceName
   * @param {string} options.input
   * @param {*[]} options.tokens
   */
  constructor({ message, bareMessage, context, line, sourceName, input, tokens }) {
    super(message);

    this.name = "WebIDLParseError"; // not to be mangled
    this.bareMessage = bareMessage;
    this.context = context;
    this.line = line;
    this.sourceName = sourceName;
    this.input = input;
    this.tokens = tokens;
  }
}


/***/ }),
/* 3 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "syntaxError", function() { return syntaxError; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "validationError", function() { return validationError; });
/**
 * @param {string} text
 */
function lastLine(text) {
  const splitted = text.split("\n");
  return splitted[splitted.length - 1];
}

/**
 * @typedef {object} WebIDL2ErrorOptions
 * @property {"error" | "warning"} [level]
 * @property {Function} [autofix]
 *
 * @param {string} message error message
 * @param {"Syntax" | "Validation"} kind error type
 * @param {WebIDL2ErrorOptions} [options]
 */
function error(source, position, current, message, kind, { level = "error", autofix, ruleName } = {}) {
  /**
   * @param {number} count
   */
  function sliceTokens(count) {
    return count > 0 ?
      source.slice(position, position + count) :
      source.slice(Math.max(position + count, 0), position);
  }

  function tokensToText(inputs, { precedes } = {}) {
    const text = inputs.map(t => t.trivia + t.value).join("");
    const nextToken = source[position];
    if (nextToken.type === "eof") {
      return text;
    }
    if (precedes) {
      return text + nextToken.trivia;
    }
    return text.slice(nextToken.trivia.length);
  }

  const maxTokens = 5; // arbitrary but works well enough
  const line =
    source[position].type !== "eof" ? source[position].line :
    source.length > 1 ? source[position - 1].line :
    1;

  const precedingLastLine = lastLine(
    tokensToText(sliceTokens(-maxTokens), { precedes: true })
  );

  const subsequentTokens = sliceTokens(maxTokens);
  const subsequentText = tokensToText(subsequentTokens);
  const subsequentFirstLine = subsequentText.split("\n")[0];

  const spaced = " ".repeat(precedingLastLine.length) + "^";
  const sourceContext = precedingLastLine + subsequentFirstLine + "\n" + spaced;

  const contextType = kind === "Syntax" ? "since" : "inside";
  const inSourceName = source.name ? ` in ${source.name}` : "";
  const grammaticalContext = (current && current.name) ? `, ${contextType} \`${current.partial ? "partial " : ""}${current.type} ${current.name}\`` : "";
  const context = `${kind} error at line ${line}${inSourceName}${grammaticalContext}:\n${sourceContext}`;
  return {
    message: `${context} ${message}`,
    bareMessage: message,
    context,
    line,
    sourceName: source.name,
    level,
    ruleName,
    autofix,
    input: subsequentText,
    tokens: subsequentTokens
  };
}

/**
 * @param {string} message error message
 */
function syntaxError(source, position, current, message) {
  return error(source, position, current, message, "Syntax");
}

/**
 * @param {string} message error message
 * @param {WebIDL2ErrorOptions} [options]
 */
function validationError(token, current, ruleName, message, options = {}) {
  options.ruleName = ruleName;
  return error(current.source, token.index, current, message, "Validation", options);
}


/***/ }),
/* 4 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "unescape", function() { return unescape; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "list", function() { return list; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "const_value", function() { return const_value; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "const_data", function() { return const_data; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "primitive_type", function() { return primitive_type; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "argument_list", function() { return argument_list; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "type_with_extended_attributes", function() { return type_with_extended_attributes; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "return_type", function() { return return_type; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "stringifier", function() { return stringifier; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "getLastIndentation", function() { return getLastIndentation; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "getMemberIndentation", function() { return getMemberIndentation; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "autofixAddExposedWindow", function() { return autofixAddExposedWindow; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "getFirstToken", function() { return getFirstToken; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "findLastIndex", function() { return findLastIndex; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "autoParenter", function() { return autoParenter; });
/* harmony import */ var _type_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(5);
/* harmony import */ var _argument_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(11);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var _operation_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(13);
/* harmony import */ var _attribute_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(14);
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(2);







/**
 * @param {string} identifier
 */
function unescape(identifier) {
  return identifier.startsWith('_') ? identifier.slice(1) : identifier;
}

/**
 * Parses comma-separated list
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {object} args
 * @param {Function} args.parser parser function for each item
 * @param {boolean} [args.allowDangler] whether to allow dangling comma
 * @param {string} [args.listName] the name to be shown on error messages
 */
function list(tokeniser, { parser, allowDangler, listName = "list" }) {
  const first = parser(tokeniser);
  if (!first) {
    return [];
  }
  first.tokens.separator = tokeniser.consume(",");
  const items = [first];
  while (first.tokens.separator) {
    const item = parser(tokeniser);
    if (!item) {
      if (!allowDangler) {
        tokeniser.error(`Trailing comma in ${listName}`);
      }
      break;
    }
    item.tokens.separator = tokeniser.consume(",");
    items.push(item);
    if (!item.tokens.separator) break;
  }
  return items;
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function const_value(tokeniser) {
  return tokeniser.consume("true", "false", "Infinity", "-Infinity", "NaN", "decimal", "integer");
}

/**
 * @param {object} token
 * @param {string} token.type
 * @param {string} token.value
 */
function const_data({ type, value }) {
  switch (type) {
    case "true":
    case "false":
      return { type: "boolean", value: type === "true" };
    case "Infinity":
    case "-Infinity":
      return { type: "Infinity", negative: type.startsWith("-") };
    case "[":
      return { type: "sequence", value: [] };
    case "{":
      return { type: "dictionary" };
    case "decimal":
    case "integer":
      return { type: "number", value };
    case "string":
      return { type: "string", value: value.slice(1, -1) };
    default:
      return { type };
  }
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function primitive_type(tokeniser) {
  function integer_type() {
    const prefix = tokeniser.consume("unsigned");
    const base = tokeniser.consume("short", "long");
    if (base) {
      const postfix = tokeniser.consume("long");
      return new _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"]({ source, tokens: { prefix, base, postfix } });
    }
    if (prefix) tokeniser.error("Failed to parse integer type");
  }

  function decimal_type() {
    const prefix = tokeniser.consume("unrestricted");
    const base = tokeniser.consume("float", "double");
    if (base) {
      return new _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"]({ source, tokens: { prefix, base } });
    }
    if (prefix) tokeniser.error("Failed to parse float type");
  }

  const { source } = tokeniser;
  const num_type = integer_type(tokeniser) || decimal_type(tokeniser);
  if (num_type) return num_type;
  const base = tokeniser.consume("boolean", "byte", "octet");
  if (base) {
    return new _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"]({ source, tokens: { base } });
  }
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function argument_list(tokeniser) {
  return list(tokeniser, { parser: _argument_js__WEBPACK_IMPORTED_MODULE_1__["Argument"].parse, listName: "arguments list" });
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} typeName
 */
function type_with_extended_attributes(tokeniser, typeName) {
  const extAttrs = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__["ExtendedAttributes"].parse(tokeniser);
  const ret = _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"].parse(tokeniser, typeName);
  if (ret) autoParenter(ret).extAttrs = extAttrs;
  return ret;
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} typeName
 */
function return_type(tokeniser, typeName) {
  const typ = _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"].parse(tokeniser, typeName || "return-type");
  if (typ) {
    return typ;
  }
  const voidToken = tokeniser.consume("void");
  if (voidToken) {
    const ret = new _type_js__WEBPACK_IMPORTED_MODULE_0__["Type"]({ source: tokeniser.source, tokens: { base: voidToken } });
    ret.type = "return-type";
    return ret;
  }
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function stringifier(tokeniser) {
  const special = tokeniser.consume("stringifier");
  if (!special) return;
  const member = _attribute_js__WEBPACK_IMPORTED_MODULE_4__["Attribute"].parse(tokeniser, { special }) ||
    _operation_js__WEBPACK_IMPORTED_MODULE_3__["Operation"].parse(tokeniser, { special }) ||
    tokeniser.error("Unterminated stringifier");
  return member;
}

/**
 * @param {string} str
 */
function getLastIndentation(str) {
  const lines = str.split("\n");
  // the first line visually binds to the preceding token
  if (lines.length) {
    const match = lines[lines.length - 1].match(/^\s+/);
    if (match) {
      return match[0];
    }
  }
  return "";
}

/**
 * @param {string} parentTrivia
 */
function getMemberIndentation(parentTrivia) {
  const indentation = getLastIndentation(parentTrivia);
  const indentCh = indentation.includes("\t") ? "\t" : "  ";
  return indentation + indentCh;
}

/**
 * @param {object} def
 * @param {import("./extended-attributes.js").ExtendedAttributes} def.extAttrs
 */
function autofixAddExposedWindow(def) {
  return () => {
    if (def.extAttrs.length){
      const tokeniser = new _tokeniser_js__WEBPACK_IMPORTED_MODULE_5__["Tokeniser"]("Exposed=Window,");
      const exposed = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__["SimpleExtendedAttribute"].parse(tokeniser);
      exposed.tokens.separator = tokeniser.consume(",");
      const existing = def.extAttrs[0];
      if (!/^\s/.test(existing.tokens.name.trivia)) {
        existing.tokens.name.trivia = ` ${existing.tokens.name.trivia}`;
      }
      def.extAttrs.unshift(exposed);
    } else {
      autoParenter(def).extAttrs = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__["ExtendedAttributes"].parse(new _tokeniser_js__WEBPACK_IMPORTED_MODULE_5__["Tokeniser"]("[Exposed=Window]"));
      const trivia = def.tokens.base.trivia;
      def.extAttrs.tokens.open.trivia = trivia;
      def.tokens.base.trivia = `\n${getLastIndentation(trivia)}`;
    }
  };
}

/**
 * Get the first syntax token for the given IDL object.
 * @param {*} data
 */
function getFirstToken(data) {
  if (data.extAttrs.length) {
    return data.extAttrs.tokens.open;
  }
  if (data.type === "operation" && !data.special) {
    return getFirstToken(data.idlType);
  }
  const tokens = Object.values(data.tokens).sort((x, y) => x.index - y.index);
  return tokens[0];
}

/**
 * @template T
 * @param {T[]} array
 * @param {(item: T) => boolean} predicate
 */
function findLastIndex(array, predicate) {
  const index = array.slice().reverse().findIndex(predicate);
  if (index === -1) {
    return index;
  }
  return array.length - index - 1;
}

/**
 * Returns a proxy that auto-assign `parent` field.
 * @template T
 * @param {T} data
 * @param {*} [parent] The object that will be assigned to `parent`.
 *                     If absent, it will be `data` by default.
 * @return {T}
 */
function autoParenter(data, parent) {
  if (!parent) {
    // Defaults to `data` unless specified otherwise.
    parent = data;
  }
  if (!data) {
    // This allows `autoParenter(undefined)` which again allows
    // `autoParenter(parse())` where the function may return nothing.
    return data;
  }
  return new Proxy(data, {
    get(target, p) {
      const value = target[p];
      if (Array.isArray(value)) {
        // Wraps the array so that any added items will also automatically
        // get their `parent` values.
        return autoParenter(value, target);
      }
      return value;
    },
    set(target, p, value) {
      target[p] = value;
      if (!value) {
        return true;
      } else if (Array.isArray(value)) {
        // Assigning an array will add `parent` to its items.
        for (const item of value) {
          if (typeof item.parent !== "undefined") {
            item.parent = parent;
          }
        }
      } else if (typeof value.parent !== "undefined") {
        value.parent = parent;
      }
      return true;
    }
  });
}


/***/ }),
/* 5 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Type", function() { return Type; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(2);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(3);
/* harmony import */ var _validators_helpers_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(8);







/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} typeName
 */
function generic_type(tokeniser, typeName) {
  const base = tokeniser.consume("FrozenArray", "Promise", "sequence", "record");
  if (!base) {
    return;
  }
  const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Type({ source: tokeniser.source, tokens: { base } }));
  ret.tokens.open = tokeniser.consume("<") || tokeniser.error(`No opening bracket after ${base.type}`);
  switch (base.type) {
    case "Promise": {
      if (tokeniser.probe("[")) tokeniser.error("Promise type cannot have extended attribute");
      const subtype = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["return_type"])(tokeniser, typeName) || tokeniser.error("Missing Promise subtype");
      ret.subtype.push(subtype);
      break;
    }
    case "sequence":
    case "FrozenArray": {
      const subtype = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser, typeName) || tokeniser.error(`Missing ${base.type} subtype`);
      ret.subtype.push(subtype);
      break;
    }
    case "record": {
      if (tokeniser.probe("[")) tokeniser.error("Record key cannot have extended attribute");
      const keyType = tokeniser.consume(..._tokeniser_js__WEBPACK_IMPORTED_MODULE_2__["stringTypes"]) || tokeniser.error(`Record key must be one of: ${_tokeniser_js__WEBPACK_IMPORTED_MODULE_2__["stringTypes"].join(", ")}`);
      const keyIdlType = new Type({ source: tokeniser.source, tokens: { base: keyType }});
      keyIdlType.tokens.separator = tokeniser.consume(",") || tokeniser.error("Missing comma after record key type");
      keyIdlType.type = typeName;
      const valueType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser, typeName) || tokeniser.error("Error parsing generic type record");
      ret.subtype.push(keyIdlType, valueType);
      break;
    }
  }
  if (!ret.idlType) tokeniser.error(`Error parsing generic type ${base.type}`);
  ret.tokens.close = tokeniser.consume(">") || tokeniser.error(`Missing closing bracket after ${base.type}`);
  return ret.this;
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function type_suffix(tokeniser, obj) {
  const nullable = tokeniser.consume("?");
  if (nullable) {
    obj.tokens.nullable = nullable;
  }
  if (tokeniser.probe("?")) tokeniser.error("Can't nullable more than once");
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} typeName
 */
function single_type(tokeniser, typeName) {
  let ret = generic_type(tokeniser, typeName) || Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["primitive_type"])(tokeniser);
  if (!ret) {
    const base = tokeniser.consume("identifier", ..._tokeniser_js__WEBPACK_IMPORTED_MODULE_2__["stringTypes"], ..._tokeniser_js__WEBPACK_IMPORTED_MODULE_2__["typeNameKeywords"]);
    if (!base) {
      return;
    }
    ret = new Type({ source: tokeniser.source, tokens: { base } });
    if (tokeniser.probe("<")) tokeniser.error(`Unsupported generic type ${base.value}`);
  }
  if (ret.generic === "Promise" && tokeniser.probe("?")) {
    tokeniser.error("Promise type cannot be nullable");
  }
  ret.type = typeName || null;
  type_suffix(tokeniser, ret);
  if (ret.nullable && ret.idlType === "any") tokeniser.error("Type `any` cannot be made nullable");
  return ret;
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} type
 */
function union_type(tokeniser, type) {
  const tokens = {};
  tokens.open = tokeniser.consume("(");
  if (!tokens.open) return;
  const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Type({ source: tokeniser.source, tokens }));
  ret.type = type || null;
  while (true) {
    const typ = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser) || tokeniser.error("No type after open parenthesis or 'or' in union type");
    if (typ.idlType === "any") tokeniser.error("Type `any` cannot be included in a union type");
    if (typ.generic === "Promise") tokeniser.error("Type `Promise` cannot be included in a union type");
    ret.subtype.push(typ);
    const or = tokeniser.consume("or");
    if (or) {
      typ.tokens.separator = or;
    }
    else break;
  }
  if (ret.idlType.length < 2) {
    tokeniser.error("At least two types are expected in a union type but found less");
  }
  tokens.close = tokeniser.consume(")") || tokeniser.error("Unterminated union type");
  type_suffix(tokeniser, ret);
  return ret.this;
}

class Type extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   * @param {string} typeName
   */
  static parse(tokeniser, typeName) {
    return single_type(tokeniser, typeName) || union_type(tokeniser, typeName);
  }

  constructor({ source, tokens }) {
    super({ source, tokens });
    Object.defineProperty(this, "subtype", { value: [], writable: true });
    this.extAttrs = new _extended_attributes_js__WEBPACK_IMPORTED_MODULE_5__["ExtendedAttributes"]({});
  }

  get generic() {
    if (this.subtype.length && this.tokens.base) {
      return this.tokens.base.value;
    }
    return "";
  }
  get nullable() {
    return Boolean(this.tokens.nullable);
  }
  get union() {
    return Boolean(this.subtype.length) && !this.tokens.base;
  }
  get idlType() {
    if (this.subtype.length) {
      return this.subtype;
    }
    // Adding prefixes/postfixes for "unrestricted float", etc.
    const name = [
      this.tokens.prefix,
      this.tokens.base,
      this.tokens.postfix
    ].filter(t => t).map(t => t.value).join(" ");
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(name);
  }

  *validate(defs) {
    yield* this.extAttrs.validate(defs);
    /*
     * If a union is nullable, its subunions cannot include a dictionary
     * If not, subunions may include dictionaries if each union is not nullable
     */
    const typedef = !this.union && defs.unique.get(this.idlType);
    const target =
      this.union ? this :
      (typedef && typedef.type === "typedef") ? typedef.idlType :
      undefined;
    if (target && this.nullable) {
      // do not allow any dictionary
      const { reference } = Object(_validators_helpers_js__WEBPACK_IMPORTED_MODULE_4__["idlTypeIncludesDictionary"])(target, defs) || {};
      if (reference) {
        const targetToken = (this.union ? reference : this).tokens.base;
        const message = `Nullable union cannot include a dictionary type`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_3__["validationError"])(targetToken, this, "no-nullable-union-dict", message);
      }
    } else {
      // allow some dictionary
      for (const subtype of this.subtype) {
        yield* subtype.validate(defs);
      }
    }
  }
}


/***/ }),
/* 6 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Base", function() { return Base; });
// @ts-check

class Base {
  /**
   * @param {object} initializer
   * @param {Base["source"]} initializer.source
   * @param {Base["tokens"]} initializer.tokens
   */
  constructor({ source, tokens }) {
    Object.defineProperties(this, {
      source: { value: source },
      tokens: { value: tokens, writable: true },
      parent: { value: null, writable: true },
      this: { value: this } // useful when escaping from proxy
    });
  }

  toJSON() {
    const json = { type: undefined, name: undefined, inheritance: undefined };
    let proto = this;
    while (proto !== Object.prototype) {
      const descMap = Object.getOwnPropertyDescriptors(proto);
      for (const [key, value] of Object.entries(descMap)) {
        if (value.enumerable || value.get) {
          // @ts-ignore - allow indexing here
          json[key] = this[key];
        }
      }
      proto = Object.getPrototypeOf(proto);
    }
    return json;
  }
}


/***/ }),
/* 7 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "idlTypeIncludesDictionary", function() { return idlTypeIncludesDictionary; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "dictionaryIncludesRequiredField", function() { return dictionaryIncludesRequiredField; });
// @ts-check

/**
 * @typedef {import("../productions/dictionary.js").Dictionary} Dictionary
 *
 * @param {*} idlType
 * @param {import("../validator.js").Definitions} defs
 * @param {object} [options]
 * @param {boolean} [options.useNullableInner] use when the input idlType is nullable and you want to use its inner type
 * @return {{ reference: *, dictionary: Dictionary }} the type reference that ultimately includes dictionary.
 */
function idlTypeIncludesDictionary(idlType, defs, { useNullableInner } = {}) {
  if (!idlType.union) {
    const def = defs.unique.get(idlType.idlType);
    if (!def) {
      return;
    }
    if (def.type === "typedef") {
      const { typedefIncludesDictionary } = defs.cache;
      if (typedefIncludesDictionary.has(def)) {
        // Note that this also halts when it met indeterminate state
        // to prevent infinite recursion
        return typedefIncludesDictionary.get(def);
      }
      defs.cache.typedefIncludesDictionary.set(def, undefined); // indeterminate state
      const result = idlTypeIncludesDictionary(def.idlType, defs);
      defs.cache.typedefIncludesDictionary.set(def, result);
      if (result) {
        return {
          reference: idlType,
          dictionary: result.dictionary
        };
      }
    }
    if (def.type === "dictionary" && (useNullableInner || !idlType.nullable)) {
      return {
        reference: idlType,
        dictionary: def
      };
    }
  }
  for (const subtype of idlType.subtype) {
    const result = idlTypeIncludesDictionary(subtype, defs);
    if (result) {
      if (subtype.union) {
        return result;
      }
      return {
        reference: subtype,
        dictionary: result.dictionary
      };
    }
  }
}

/**
 * @param {*} dict dictionary type
 * @param {import("../validator.js").Definitions} defs
 * @return {boolean}
 */
function dictionaryIncludesRequiredField(dict, defs) {
  if (defs.cache.dictionaryIncludesRequiredField.has(dict)) {
    return defs.cache.dictionaryIncludesRequiredField.get(dict);
  }
  defs.cache.dictionaryIncludesRequiredField.set(dict, undefined); // indeterminate
  if (dict.inheritance) {
    const superdict = defs.unique.get(dict.inheritance);
    if (!superdict) {
      return true;
    }
    if (dictionaryIncludesRequiredField(superdict, defs)) {
      return true;
    }
  }
  const result = dict.members.some(field => field.required);
  defs.cache.dictionaryIncludesRequiredField.set(dict, result);
  return result;
}


/***/ }),
/* 8 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SimpleExtendedAttribute", function() { return SimpleExtendedAttribute; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ExtendedAttributes", function() { return ExtendedAttributes; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _array_base_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(9);
/* harmony import */ var _token_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(10);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(4);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(3);






/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {string} tokenName
 */
function tokens(tokeniser, tokenName) {
  return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["list"])(tokeniser, {
    parser: _token_js__WEBPACK_IMPORTED_MODULE_2__["Token"].parser(tokeniser, tokenName),
    listName: tokenName + " list"
  });
}

const extAttrValueSyntax = ["identifier", "decimal", "integer", "string"];

const shouldBeLegacyPrefixed = [
  "NoInterfaceObject",
  "LenientSetter",
  "LenientThis",
  "TreatNonObjectAsNull",
  "Unforgeable",
];

const renamedLegacies = new Map([
  ...shouldBeLegacyPrefixed.map(name => [name, `Legacy${name}`]),
  ["NamedConstructor", "LegacyFactoryFunction"],
  ["OverrideBuiltins", "LegacyOverrideBuiltIns"],
  ["TreatNullAs", "LegacyNullToEmptyString"],
]);

/**
 * This will allow a set of extended attribute values to be parsed.
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function extAttrListItems(tokeniser) {
  for (const syntax of extAttrValueSyntax) {
    const toks = tokens(tokeniser, syntax);
    if (toks.length) {
      return toks;
    }
  }
  tokeniser.error(`Expected identifiers, strings, decimals, or integers but none found`);
}


class ExtendedAttributeParameters extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const tokens = { assign: tokeniser.consume("=") };
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["autoParenter"])(new ExtendedAttributeParameters({ source: tokeniser.source, tokens }));
    if (tokens.assign) {
      tokens.secondaryName = tokeniser.consume(...extAttrValueSyntax);
    }
    tokens.open = tokeniser.consume("(");
    if (tokens.open) {
      ret.list = ret.rhsIsList ?
        // [Exposed=(Window,Worker)]
        extAttrListItems(tokeniser) :
        // [LegacyFactoryFunction=Audio(DOMString src)] or [Constructor(DOMString str)]
        Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["argument_list"])(tokeniser);
      tokens.close = tokeniser.consume(")") || tokeniser.error("Unexpected token in extended attribute argument list");
    } else if (ret.hasRhs && !tokens.secondaryName) {
      tokeniser.error("No right hand side to extended attribute assignment");
    }
    return ret.this;
  }

  get rhsIsList() {
    return this.tokens.assign && !this.tokens.secondaryName;
  }

  get rhsType() {
    if (this.rhsIsList) {
      return this.list[0].tokens.value.type + "-list";
    }
    if (this.tokens.secondaryName) {
      return this.tokens.secondaryName.type;
    }
    return null;
  }
}

class SimpleExtendedAttribute extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const name = tokeniser.consume("identifier");
    if (name) {
      return new SimpleExtendedAttribute({
        source: tokeniser.source,
        tokens: { name },
        params: ExtendedAttributeParameters.parse(tokeniser)
      });
    }
  }

  constructor({ source, tokens, params }) {
    super({ source, tokens });
    params.parent = this;
    Object.defineProperty(this, "params", { value: params });
  }

  get type() {
    return "extended-attribute";
  }
  get name() {
    return this.tokens.name.value;
  }
  get rhs() {
    const { rhsType: type, tokens, list } = this.params;
    if (!type) {
      return null;
    }
    const value = this.params.rhsIsList ? list : Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["unescape"])(tokens.secondaryName.value);
    return { type, value };
  }
  get arguments() {
    const { rhsIsList, list } = this.params;
    if (!list || rhsIsList) {
      return [];
    }
    return list;
  }

  *validate(defs) {
    const { name } = this;
    if (name === "LegacyNoInterfaceObject") {
      const message = `\`[LegacyNoInterfaceObject]\` extended attribute is an \
undesirable feature that may be removed from Web IDL in the future. Refer to the \
[relevant upstream PR](https://github.com/heycam/webidl/pull/609) for more \
information.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_4__["validationError"])(this.tokens.name, this, "no-nointerfaceobject", message, { level: "warning" });
    } else if (renamedLegacies.has(name)) {
      const message = `\`[${name}]\` extended attribute is a legacy feature \
that is now renamed to \`[${renamedLegacies.get(name)}]\`. Refer to the \
[relevant upstream PR](https://github.com/heycam/webidl/pull/870) for more \
information.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_4__["validationError"])(this.tokens.name, this, "renamed-legacy", message, {
        level: "warning",
        autofix: renameLegacyExtendedAttribute(this)
      });
    }
    for (const arg of this.arguments) {
      yield* arg.validate(defs);
    }
  }
}

/**
 * @param {SimpleExtendedAttribute} extAttr
 */
function renameLegacyExtendedAttribute(extAttr) {
  return () => {
    const { name } = extAttr;
    extAttr.tokens.name.value = renamedLegacies.get(name);
    if (name === "TreatNullAs") {
      extAttr.params.tokens = {};
    }
  };
}

// Note: we parse something simpler than the official syntax. It's all that ever
// seems to be used
class ExtendedAttributes extends _array_base_js__WEBPACK_IMPORTED_MODULE_1__["ArrayBase"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const tokens = {};
    tokens.open = tokeniser.consume("[");
    if (!tokens.open) return new ExtendedAttributes({});
    const ret = new ExtendedAttributes({ source: tokeniser.source, tokens });
    ret.push(...Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["list"])(tokeniser, {
      parser: SimpleExtendedAttribute.parse,
      listName: "extended attribute"
    }));
    tokens.close = tokeniser.consume("]") || tokeniser.error("Unexpected closing token of extended attribute");
    if (!ret.length) {
      tokeniser.error("Found an empty extended attribute");
    }
    if (tokeniser.probe("[")) {
      tokeniser.error("Illegal double extended attribute lists, consider merging them");
    }
    return ret;
  }

  *validate(defs) {
    for (const extAttr of this) {
      yield* extAttr.validate(defs);
    }
  }
}


/***/ }),
/* 9 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ArrayBase", function() { return ArrayBase; });
// @ts-check

class ArrayBase extends Array {
  constructor({ source, tokens }) {
    super();
    Object.defineProperties(this, {
      source: { value: source },
      tokens: { value: tokens },
      parent: { value: null, writable: true }
    });
  }
}


/***/ }),
/* 10 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Token", function() { return Token; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
// @ts-check




class Token extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   * @param {string} type
   */
  static parser(tokeniser, type) {
    return () => {
      const value = tokeniser.consume(type);
      if (value) {
        return new Token({ source: tokeniser.source, tokens: { value } });
      }
    };
  }

  get value() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.value.value);
  }
}


/***/ }),
/* 11 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Argument", function() { return Argument; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _default_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(12);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(4);
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(2);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(3);
/* harmony import */ var _validators_helpers_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(7);
// @ts-check









class Argument extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const start_position = tokeniser.position;
    /** @type {Base["tokens"]} */
    const tokens = {};
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["autoParenter"])(new Argument({ source: tokeniser.source, tokens }));
    ret.extAttrs = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__["ExtendedAttributes"].parse(tokeniser);
    tokens.optional = tokeniser.consume("optional");
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["type_with_extended_attributes"])(tokeniser, "argument-type");
    if (!ret.idlType) {
      return tokeniser.unconsume(start_position);
    }
    if (!tokens.optional) {
      tokens.variadic = tokeniser.consume("...");
    }
    tokens.name = tokeniser.consume("identifier", ..._tokeniser_js__WEBPACK_IMPORTED_MODULE_4__["argumentNameKeywords"]);
    if (!tokens.name) {
      return tokeniser.unconsume(start_position);
    }
    ret.default = tokens.optional ? _default_js__WEBPACK_IMPORTED_MODULE_1__["Default"].parse(tokeniser) : null;
    return ret.this;
  }

  get type() {
    return "argument";
  }
  get optional() {
    return !!this.tokens.optional;
  }
  get variadic() {
    return !!this.tokens.variadic;
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["unescape"])(this.tokens.name.value);
  }

  /**
   * @param {import("../validator.js").Definitions} defs
   */
  *validate(defs) {
    yield* this.idlType.validate(defs);
    const result = Object(_validators_helpers_js__WEBPACK_IMPORTED_MODULE_6__["idlTypeIncludesDictionary"])(this.idlType, defs, { useNullableInner: true });
    if (result) {
      if (this.idlType.nullable) {
        const message = `Dictionary arguments cannot be nullable.`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_5__["validationError"])(this.tokens.name, this, "no-nullable-dict-arg", message);
      } else if (!this.optional) {
        if (this.parent && !Object(_validators_helpers_js__WEBPACK_IMPORTED_MODULE_6__["dictionaryIncludesRequiredField"])(result.dictionary, defs) && isLastRequiredArgument(this)) {
          const message = `Dictionary argument must be optional if it has no required fields`;
          yield Object(_error_js__WEBPACK_IMPORTED_MODULE_5__["validationError"])(this.tokens.name, this, "dict-arg-optional", message, {
            autofix: autofixDictionaryArgumentOptionality(this)
          });
        }
      } else if (!this.default) {
        const message = `Optional dictionary arguments must have a default value of \`{}\`.`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_5__["validationError"])(this.tokens.name, this, "dict-arg-default", message, {
          autofix: autofixOptionalDictionaryDefaultValue(this)
        });
      }
    }
  }
}

/**
 * @param {Argument} arg
 */
function isLastRequiredArgument(arg) {
  const list = arg.parent.arguments || arg.parent.list;
  const index = list.indexOf(arg);
  const requiredExists = list.slice(index + 1).some(a => !a.optional);
  return !requiredExists;
}

/**
 * @param {Argument} arg
 */
function autofixDictionaryArgumentOptionality(arg) {
  return () => {
    const firstToken = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_3__["getFirstToken"])(arg.idlType);
    arg.tokens.optional = { type: "optional", value: "optional", trivia: firstToken.trivia };
    firstToken.trivia = " ";
    autofixOptionalDictionaryDefaultValue(arg)();
  };
}

/**
 * @param {Argument} arg
 */
function autofixOptionalDictionaryDefaultValue(arg) {
  return () => {
    arg.default = _default_js__WEBPACK_IMPORTED_MODULE_1__["Default"].parse(new _tokeniser_js__WEBPACK_IMPORTED_MODULE_4__["Tokeniser"](" = {}"));
  };
}


/***/ }),
/* 12 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Default", function() { return Default; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class Default extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const assign = tokeniser.consume("=");
    if (!assign) {
      return null;
    }
    const def = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_value"])(tokeniser) || tokeniser.consume("string", "null", "[", "{") || tokeniser.error("No value for default");
    const expression = [def];
    if (def.type === "[") {
      const close = tokeniser.consume("]") || tokeniser.error("Default sequence value must be empty");
      expression.push(close);
    } else if (def.type === "{") {
      const close = tokeniser.consume("}") || tokeniser.error("Default dictionary value must be empty");
      expression.push(close);
    }
    return new Default({ source: tokeniser.source, tokens: { assign }, expression });
  }

  constructor({ source, tokens, expression }) {
    super({ source, tokens });
    expression.parent = this;
    Object.defineProperty(this, "expression", { value: expression });
  }

  get type() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).type;
  }
  get value() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).value;
  }
  get negative() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).negative;
  }
}


/***/ }),
/* 13 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Operation", function() { return Operation; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(3);




class Operation extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @typedef {import("../tokeniser.js").Token} Token
   *
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   * @param {object} [options]
   * @param {Token} [options.special]
   * @param {Token} [options.regular]
   */
  static parse(tokeniser, { special, regular } = {}) {
    const tokens = { special };
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Operation({ source: tokeniser.source, tokens }));
    if (special && special.value === "stringifier") {
      tokens.termination = tokeniser.consume(";");
      if (tokens.termination) {
        ret.arguments = [];
        return ret;
      }
    }
    if (!special && !regular) {
      tokens.special = tokeniser.consume("getter", "setter", "deleter");
    }
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["return_type"])(tokeniser) || tokeniser.error("Missing return type");
    tokens.name = tokeniser.consume("identifier", "includes");
    tokens.open = tokeniser.consume("(") || tokeniser.error("Invalid operation");
    ret.arguments = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["argument_list"])(tokeniser);
    tokens.close = tokeniser.consume(")") || tokeniser.error("Unterminated operation");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated operation, expected `;`");
    return ret.this;
  }

  get type() {
    return "operation";
  }
  get name() {
    const { name } = this.tokens;
    if (!name) {
      return "";
    }
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(name.value);
  }
  get special() {
    if (!this.tokens.special) {
      return "";
    }
    return this.tokens.special.value;
  }

  *validate(defs) {
    if (!this.name && ["", "static"].includes(this.special)) {
      const message = `Regular or static operations must have both a return type and an identifier.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_2__["validationError"])(this.tokens.open, this, "incomplete-op", message);
    }
    if (this.idlType) {
      yield* this.idlType.validate(defs);
    }
    for (const argument of this.arguments) {
      yield* argument.validate(defs);
    }
  }
}


/***/ }),
/* 14 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Attribute", function() { return Attribute; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class Attribute extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   */
  static parse(tokeniser, { special, noInherit = false, readonly = false } = {}) {
    const start_position = tokeniser.position;
    const tokens = { special };
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Attribute({ source: tokeniser.source, tokens }));
    if (!special && !noInherit) {
      tokens.special = tokeniser.consume("inherit");
    }
    if (ret.special === "inherit" && tokeniser.probe("readonly")) {
      tokeniser.error("Inherited attributes cannot be read-only");
    }
    tokens.readonly = tokeniser.consume("readonly");
    if (readonly && !tokens.readonly && tokeniser.probe("attribute")) {
      tokeniser.error("Attributes must be readonly in this context");
    }
    tokens.base = tokeniser.consume("attribute");
    if (!tokens.base) {
      tokeniser.unconsume(start_position);
      return;
    }
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser, "attribute-type") || tokeniser.error("Attribute lacks a type");
    switch (ret.idlType.generic) {
      case "sequence":
      case "record": tokeniser.error(`Attributes cannot accept ${ret.idlType.generic} types`);
    }
    tokens.name = tokeniser.consume("identifier", "async", "required") || tokeniser.error("Attribute lacks a name");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated attribute, expected `;`");
    return ret.this;
  }

  get type() {
    return "attribute";
  }
  get special() {
    if (!this.tokens.special) {
      return "";
    }
    return this.tokens.special.value;
  }
  get readonly() {
    return !!this.tokens.readonly;
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.name.value);
  }

  *validate(defs) {
    yield* this.extAttrs.validate(defs);
    yield* this.idlType.validate(defs);
  }
}


/***/ }),
/* 15 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Enum", function() { return Enum; });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(4);
/* harmony import */ var _token_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(10);
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(6);




class EnumValue extends _token_js__WEBPACK_IMPORTED_MODULE_1__["Token"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const value = tokeniser.consume("string");
    if (value) {
      return new EnumValue({ source: tokeniser.source, tokens: { value } });
    }
  }

  get type() {
    return "enum-value";
  }
  get value() {
    return super.value.slice(1, -1);
  }
}

class Enum extends _base_js__WEBPACK_IMPORTED_MODULE_2__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    /** @type {Base["tokens"]} */
    const tokens = {};
    tokens.base = tokeniser.consume("enum");
    if (!tokens.base) {
      return;
    }
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("No name for enum");
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_0__["autoParenter"])(new Enum({ source: tokeniser.source, tokens }));
    tokeniser.current = ret.this;
    tokens.open = tokeniser.consume("{") || tokeniser.error("Bodyless enum");
    ret.values = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_0__["list"])(tokeniser, {
      parser: EnumValue.parse,
      allowDangler: true,
      listName: "enumeration"
    });
    if (tokeniser.probe("string")) {
      tokeniser.error("No comma between enum values");
    }
    tokens.close = tokeniser.consume("}") || tokeniser.error("Unexpected value in enum");
    if (!ret.values.length) {
      tokeniser.error("No value in enum");
    }
    tokens.termination = tokeniser.consume(";") || tokeniser.error("No semicolon after enum");
    return ret.this;
  }

  get type() {
    return "enum";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
  }
}


/***/ }),
/* 16 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Includes", function() { return Includes; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
// @ts-check




class Includes extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const target = tokeniser.consume("identifier");
    if (!target) {
      return;
    }
    const tokens = { target };
    tokens.includes = tokeniser.consume("includes");
    if (!tokens.includes) {
      tokeniser.unconsume(target.index);
      return;
    }
    tokens.mixin = tokeniser.consume("identifier") || tokeniser.error("Incomplete includes statement");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("No terminating ; for includes statement");
    return new Includes({ source: tokeniser.source, tokens });
  }

  get type() {
    return "includes";
  }
  get target() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.target.value);
  }
  get includes() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.mixin.value);
  }
}


/***/ }),
/* 17 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Typedef", function() { return Typedef; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class Typedef extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    /** @type {Base["tokens"]} */
    const tokens = {};
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Typedef({ source: tokeniser.source, tokens }));
    tokens.base = tokeniser.consume("typedef");
    if (!tokens.base) {
      return;
    }
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser, "typedef-type") || tokeniser.error("Typedef lacks a type");
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("Typedef lacks a name");
    tokeniser.current = ret.this;
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated typedef, expected `;`");
    return ret.this;
  }

  get type() {
    return "typedef";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.name.value);
  }

  *validate(defs) {
    yield* this.idlType.validate(defs);
  }
}


/***/ }),
/* 18 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CallbackFunction", function() { return CallbackFunction; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class CallbackFunction extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   */
  static parse(tokeniser, base) {
    const tokens = { base };
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new CallbackFunction({ source: tokeniser.source, tokens }));
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("Callback lacks a name");
    tokeniser.current = ret.this;
    tokens.assign = tokeniser.consume("=") || tokeniser.error("Callback lacks an assignment");
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["return_type"])(tokeniser) || tokeniser.error("Callback lacks a return type");
    tokens.open = tokeniser.consume("(") || tokeniser.error("Callback lacks parentheses for arguments");
    ret.arguments = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["argument_list"])(tokeniser);
    tokens.close = tokeniser.consume(")") || tokeniser.error("Unterminated callback");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated callback, expected `;`");
    return ret.this;
  }

  get type() {
    return "callback";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.name.value);
  }

  *validate(defs) {
    yield* this.extAttrs.validate(defs);
    yield* this.idlType.validate(defs);
  }
}


/***/ }),
/* 19 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Interface", function() { return Interface; });
/* harmony import */ var _container_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(20);
/* harmony import */ var _attribute_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(14);
/* harmony import */ var _operation_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var _constant_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(21);
/* harmony import */ var _iterable_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(22);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(4);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(3);
/* harmony import */ var _validators_interface_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(23);
/* harmony import */ var _constructor_js__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(24);
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(2);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(8);












/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function static_member(tokeniser) {
  const special = tokeniser.consume("static");
  if (!special) return;
  const member = _attribute_js__WEBPACK_IMPORTED_MODULE_1__["Attribute"].parse(tokeniser, { special }) ||
    _operation_js__WEBPACK_IMPORTED_MODULE_2__["Operation"].parse(tokeniser, { special }) ||
    tokeniser.error("No body in static member");
  return member;
}

class Interface extends _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser, base, { partial = null } = {}) {
    const tokens = { partial, base };
    return _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"].parse(tokeniser, new Interface({ source: tokeniser.source, tokens }), {
      type: "interface",
      inheritable: !partial,
      allowedMembers: [
        [_constant_js__WEBPACK_IMPORTED_MODULE_3__["Constant"].parse],
        [_constructor_js__WEBPACK_IMPORTED_MODULE_8__["Constructor"].parse],
        [static_member],
        [_helpers_js__WEBPACK_IMPORTED_MODULE_5__["stringifier"]],
        [_iterable_js__WEBPACK_IMPORTED_MODULE_4__["IterableLike"].parse],
        [_attribute_js__WEBPACK_IMPORTED_MODULE_1__["Attribute"].parse],
        [_operation_js__WEBPACK_IMPORTED_MODULE_2__["Operation"].parse]
      ]
    });
  }

  get type() {
    return "interface";
  }

  *validate(defs) {
    yield* this.extAttrs.validate(defs);
    if (
      !this.partial &&
      this.extAttrs.every(extAttr => extAttr.name !== "Exposed") &&
      this.extAttrs.every(extAttr => extAttr.name !== "LegacyNoInterfaceObject")
    ) {
      const message = `Interfaces must have \`[Exposed]\` extended attribute. \
To fix, add, for example, \`[Exposed=Window]\`. Please also consider carefully \
if your interface should also be exposed in a Worker scope. Refer to the \
[WebIDL spec section on Exposed](https://heycam.github.io/webidl/#Exposed) \
for more information.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_6__["validationError"])(this.tokens.name, this, "require-exposed", message, {
        autofix: Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["autofixAddExposedWindow"])(this)
      });
    }
    const oldConstructors = this.extAttrs.filter(extAttr => extAttr.name === "Constructor");
    for (const constructor of oldConstructors) {
      const message = `Constructors should now be represented as a \`constructor()\` operation on the interface \
instead of \`[Constructor]\` extended attribute. Refer to the \
[WebIDL spec section on constructor operations](https://heycam.github.io/webidl/#idl-constructors) \
for more information.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_6__["validationError"])(constructor.tokens.name, this, "constructor-member", message, {
        autofix: autofixConstructor(this, constructor)
      });
    }

    const isGlobal = this.extAttrs.some(extAttr => extAttr.name === "Global");
    if (isGlobal) {
      const factoryFunctions = this.extAttrs.filter(extAttr => extAttr.name === "LegacyFactoryFunction");
      for (const named of factoryFunctions) {
        const message = `Interfaces marked as \`[Global]\` cannot have factory functions.`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_6__["validationError"])(named.tokens.name, this, "no-constructible-global", message);
      }

      const constructors = this.members.filter(member => member.type === "constructor");
      for (const named of constructors) {
        const message = `Interfaces marked as \`[Global]\` cannot have constructors.`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_6__["validationError"])(named.tokens.base, this, "no-constructible-global", message);
      }
    }

    yield* super.validate(defs);
    if (!this.partial) {
      yield* Object(_validators_interface_js__WEBPACK_IMPORTED_MODULE_7__["checkInterfaceMemberDuplication"])(defs, this);
    }
  }
}

function autofixConstructor(interfaceDef, constructorExtAttr) {
  interfaceDef = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["autoParenter"])(interfaceDef);
  return () => {
    const indentation = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["getLastIndentation"])(interfaceDef.extAttrs.tokens.open.trivia);
    const memberIndent = interfaceDef.members.length ?
      Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["getLastIndentation"])(Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["getFirstToken"])(interfaceDef.members[0]).trivia) :
      Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["getMemberIndentation"])(indentation);
    const constructorOp = _constructor_js__WEBPACK_IMPORTED_MODULE_8__["Constructor"].parse(new _tokeniser_js__WEBPACK_IMPORTED_MODULE_9__["Tokeniser"](`\n${memberIndent}constructor();`));
    constructorOp.extAttrs = new _extended_attributes_js__WEBPACK_IMPORTED_MODULE_10__["ExtendedAttributes"]({});
    Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["autoParenter"])(constructorOp).arguments = constructorExtAttr.arguments;

    const existingIndex = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_5__["findLastIndex"])(interfaceDef.members, m => m.type === "constructor");
    interfaceDef.members.splice(existingIndex + 1, 0, constructorOp);

    const { close }  = interfaceDef.tokens;
    if (!close.trivia.includes("\n")) {
      close.trivia += `\n${indentation}`;
    }

    const { extAttrs } = interfaceDef;
    const index = extAttrs.indexOf(constructorExtAttr);
    const removed = extAttrs.splice(index, 1);
    if (!extAttrs.length) {
      extAttrs.tokens.open = extAttrs.tokens.close = undefined;
    } else if (extAttrs.length === index) {
      extAttrs[index - 1].tokens.separator = undefined;
    } else if (!extAttrs[index].tokens.name.trivia.trim()) {
      extAttrs[index].tokens.name.trivia = removed[0].tokens.name.trivia;
    }
  };
}


/***/ }),
/* 20 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Container", function() { return Container; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(4);




/**
 * @param {import("../tokeniser.js").Tokeniser} tokeniser
 */
function inheritance(tokeniser) {
  const colon = tokeniser.consume(":");
  if (!colon) {
    return {};
  }
  const inheritance = tokeniser.consume("identifier") || tokeniser.error("Inheritance lacks a type");
  return { colon, inheritance };
}

class Container extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
    /**
     * @template T
     * @param {import("../tokeniser.js").Tokeniser} tokeniser
     * @param {T} instance
     * @param {*} args
     */
    static parse(tokeniser, instance, { type, inheritable, allowedMembers }) {
      const { tokens } = instance;
      tokens.name = tokeniser.consume("identifier") || tokeniser.error(`Missing name in ${instance.type}`);
      tokeniser.current = instance;
      instance = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["autoParenter"])(instance);
      if (inheritable) {
        Object.assign(tokens, inheritance(tokeniser));
      }
      tokens.open = tokeniser.consume("{") || tokeniser.error(`Bodyless ${type}`);
      instance.members = [];
      while (true) {
        tokens.close = tokeniser.consume("}");
        if (tokens.close) {
          tokens.termination = tokeniser.consume(";") || tokeniser.error(`Missing semicolon after ${type}`);
          return instance.this;
        }
        const ea = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_1__["ExtendedAttributes"].parse(tokeniser);
        let mem;
        for (const [parser, ...args] of allowedMembers) {
          mem = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["autoParenter"])(parser(tokeniser, ...args));
          if (mem) {
            break;
          }
        }
        if (!mem) {
          tokeniser.error("Unknown member");
        }
        mem.extAttrs = ea;
        instance.members.push(mem.this);
      }
    }

    get partial() {
      return !!this.tokens.partial;
    }
    get name() {
      return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["unescape"])(this.tokens.name.value);
    }
    get inheritance() {
      if (!this.tokens.inheritance) {
        return null;
      }
      return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["unescape"])(this.tokens.inheritance.value);
    }

    *validate(defs) {
      for (const member of this.members) {
        if (member.validate) {
          yield* member.validate(defs);
        }
      }
    }
  }


/***/ }),
/* 21 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Constant", function() { return Constant; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _type_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(5);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(4);




class Constant extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    /** @type {Base["tokens"]} */
    const tokens = {};
    tokens.base = tokeniser.consume("const");
    if (!tokens.base) {
      return;
    }
    let idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["primitive_type"])(tokeniser);
    if (!idlType) {
      const base = tokeniser.consume("identifier") || tokeniser.error("Const lacks a type");
      idlType = new _type_js__WEBPACK_IMPORTED_MODULE_1__["Type"]({ source: tokeniser.source, tokens: { base } });
    }
    if (tokeniser.probe("?")) {
      tokeniser.error("Unexpected nullable constant type");
    }
    idlType.type = "const-type";
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("Const lacks a name");
    tokens.assign = tokeniser.consume("=") || tokeniser.error("Const lacks value assignment");
    tokens.value = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["const_value"])(tokeniser) || tokeniser.error("Const lacks a value");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated const, expected `;`");
    const ret = new Constant({ source: tokeniser.source, tokens });
    Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["autoParenter"])(ret).idlType = idlType;
    return ret;
  }

  get type() {
    return "const";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["unescape"])(this.tokens.name.value);
  }
  get value() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_2__["const_data"])(this.tokens.value);
  }
}


/***/ }),
/* 22 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "IterableLike", function() { return IterableLike; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class IterableLike extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const start_position = tokeniser.position;
    const tokens = {};
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new IterableLike({ source: tokeniser.source, tokens }));
    tokens.readonly = tokeniser.consume("readonly");
    if (!tokens.readonly) {
      tokens.async = tokeniser.consume("async");
    }
    tokens.base =
      tokens.readonly ? tokeniser.consume("maplike", "setlike") :
      tokens.async ? tokeniser.consume("iterable") :
      tokeniser.consume("iterable", "maplike", "setlike");
    if (!tokens.base) {
      tokeniser.unconsume(start_position);
      return;
    }

    const { type } = ret;
    const secondTypeRequired = type === "maplike";
    const secondTypeAllowed = secondTypeRequired || type === "iterable";
    const argumentAllowed = ret.async && type === "iterable";

    tokens.open = tokeniser.consume("<") || tokeniser.error(`Missing less-than sign \`<\` in ${type} declaration`);
    const first = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser) || tokeniser.error(`Missing a type argument in ${type} declaration`);
    ret.idlType = [first];
    ret.arguments = [];

    if (secondTypeAllowed) {
      first.tokens.separator = tokeniser.consume(",");
      if (first.tokens.separator) {
        ret.idlType.push(Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser));
      }
      else if (secondTypeRequired) {
        tokeniser.error(`Missing second type argument in ${type} declaration`);
      }
    }

    tokens.close = tokeniser.consume(">") || tokeniser.error(`Missing greater-than sign \`>\` in ${type} declaration`);

    if (tokeniser.probe("(")) {
      if (argumentAllowed) {
        tokens.argsOpen = tokeniser.consume("(");
        ret.arguments.push(...Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["argument_list"])(tokeniser));
        tokens.argsClose = tokeniser.consume(")") || tokeniser.error("Unterminated async iterable argument list");
      } else {
        tokeniser.error(`Arguments are only allowed for \`async iterable\``);
      }
    }

    tokens.termination = tokeniser.consume(";") || tokeniser.error(`Missing semicolon after ${type} declaration`);

    return ret.this;
  }

  get type() {
    return this.tokens.base.value;
  }
  get readonly() {
    return !!this.tokens.readonly;
  }
  get async() {
    return !!this.tokens.async;
  }

  *validate(defs) {
    for (const type of this.idlType) {
      yield* type.validate(defs);
    }
    for (const argument of this.arguments) {
      yield* argument.validate(defs);
    }
  }
}


/***/ }),
/* 23 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "checkInterfaceMemberDuplication", function() { return checkInterfaceMemberDuplication; });
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(3);
// @ts-check



function* checkInterfaceMemberDuplication(defs, i) {
  const opNames = new Set(getOperations(i).map(op => op.name));
  const partials = defs.partials.get(i.name) || [];
  const mixins = defs.mixinMap.get(i.name) || [];
  for (const ext of [...partials, ...mixins]) {
    const additions = getOperations(ext);
    yield* forEachExtension(additions, opNames, ext, i);
    for (const addition of additions) {
      opNames.add(addition.name);
    }
  }

  function* forEachExtension(additions, existings, ext, base) {
    for (const addition of additions) {
      const { name } = addition;
      if (name && existings.has(name)) {
        const message = `The operation "${name}" has already been defined for the base interface "${base.name}" either in itself or in a mixin`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["validationError"])(addition.tokens.name, ext, "no-cross-overload", message);
      }
    }
  }

  function getOperations(i) {
    return i.members
      .filter(({type}) => type === "operation");
  }
}


/***/ }),
/* 24 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Constructor", function() { return Constructor; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);



class Constructor extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const base = tokeniser.consume("constructor");
    if (!base) {
      return;
    }
    /** @type {Base["tokens"]} */
    const tokens = { base };
    tokens.open = tokeniser.consume("(") || tokeniser.error("No argument list in constructor");
    const args = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["argument_list"])(tokeniser);
    tokens.close = tokeniser.consume(")") || tokeniser.error("Unterminated constructor");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("No semicolon after constructor");
    const ret = new Constructor({ source: tokeniser.source, tokens });
    Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(ret).arguments = args;
    return ret;
  }

  get type() {
    return "constructor";
  }

  *validate(defs) {
    if (this.idlType) {
      yield* this.idlType.validate(defs);
    }
    for (const argument of this.arguments) {
      yield* argument.validate(defs);
    }
  }
}


/***/ }),
/* 25 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Mixin", function() { return Mixin; });
/* harmony import */ var _container_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(20);
/* harmony import */ var _constant_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(21);
/* harmony import */ var _attribute_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(14);
/* harmony import */ var _operation_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(13);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(4);






class Mixin extends _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"] {
  /**
   * @typedef {import("../tokeniser.js").Token} Token
   *
   * @param {import("../tokeniser.js").Tokeniser} tokeniser
   * @param {Token} base
   * @param {object} [options]
   * @param {Token} [options.partial]
   */
  static parse(tokeniser, base, { partial } = {}) {
    const tokens = { partial, base };
    tokens.mixin = tokeniser.consume("mixin");
    if (!tokens.mixin) {
      return;
    }
    return _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"].parse(tokeniser, new Mixin({ source: tokeniser.source, tokens }), {
      type: "interface mixin",
      allowedMembers: [
        [_constant_js__WEBPACK_IMPORTED_MODULE_1__["Constant"].parse],
        [_helpers_js__WEBPACK_IMPORTED_MODULE_4__["stringifier"]],
        [_attribute_js__WEBPACK_IMPORTED_MODULE_2__["Attribute"].parse, { noInherit: true }],
        [_operation_js__WEBPACK_IMPORTED_MODULE_3__["Operation"].parse, { regular: true }]
      ]
    });
  }

  get type() {
    return "interface mixin";
  }
}


/***/ }),
/* 26 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Dictionary", function() { return Dictionary; });
/* harmony import */ var _container_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(20);
/* harmony import */ var _field_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(27);
// @ts-check




class Dictionary extends _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   * @param {object} [options]
   * @param {import("../tokeniser.js").Token} [options.partial]
   */
  static parse(tokeniser, { partial } = {}) {
    const tokens = { partial };
    tokens.base = tokeniser.consume("dictionary");
    if (!tokens.base) {
      return;
    }
    return _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"].parse(tokeniser, new Dictionary({ source: tokeniser.source, tokens }), {
      type: "dictionary",
      inheritable: !partial,
      allowedMembers: [
        [_field_js__WEBPACK_IMPORTED_MODULE_1__["Field"].parse],
      ]
    });
  }

  get type() {
    return "dictionary";
  }
}


/***/ }),
/* 27 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Field", function() { return Field; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
/* harmony import */ var _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var _default_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(12);





class Field extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    /** @type {Base["tokens"]} */
    const tokens = {};
    const ret = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["autoParenter"])(new Field({ source: tokeniser.source, tokens }));
    ret.extAttrs = _extended_attributes_js__WEBPACK_IMPORTED_MODULE_2__["ExtendedAttributes"].parse(tokeniser);
    tokens.required = tokeniser.consume("required");
    ret.idlType = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["type_with_extended_attributes"])(tokeniser, "dictionary-type") || tokeniser.error("Dictionary member lacks a type");
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("Dictionary member lacks a name");
    ret.default = _default_js__WEBPACK_IMPORTED_MODULE_3__["Default"].parse(tokeniser);
    if (tokens.required && ret.default) tokeniser.error("Required member must not have a default");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("Unterminated dictionary member, expected `;`");
    return ret.this;
  }

  get type() {
    return "field";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.name.value);
  }
  get required() {
    return !!this.tokens.required;
  }

  *validate(defs) {
    yield* this.idlType.validate(defs);
  }
}


/***/ }),
/* 28 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Namespace", function() { return Namespace; });
/* harmony import */ var _container_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(20);
/* harmony import */ var _attribute_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(14);
/* harmony import */ var _operation_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(3);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(4);






class Namespace extends _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   * @param {object} [options]
   * @param {import("../tokeniser.js").Token} [options.partial]
   */
  static parse(tokeniser, { partial } = {}) {
    const tokens = { partial };
    tokens.base = tokeniser.consume("namespace");
    if (!tokens.base) {
      return;
    }
    return _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"].parse(tokeniser, new Namespace({ source: tokeniser.source, tokens }), {
      type: "namespace",
      allowedMembers: [
        [_attribute_js__WEBPACK_IMPORTED_MODULE_1__["Attribute"].parse, { noInherit: true, readonly: true }],
        [_operation_js__WEBPACK_IMPORTED_MODULE_2__["Operation"].parse, { regular: true }]
      ]
    });
  }

  get type() {
    return "namespace";
  }

  *validate(defs) {
    if (!this.partial && this.extAttrs.every(extAttr => extAttr.name !== "Exposed")) {
      const message = `Namespaces must have [Exposed] extended attribute. \
To fix, add, for example, [Exposed=Window]. Please also consider carefully \
if your namespace should also be exposed in a Worker scope. Refer to the \
[WebIDL spec section on Exposed](https://heycam.github.io/webidl/#Exposed) \
for more information.`;
      yield Object(_error_js__WEBPACK_IMPORTED_MODULE_3__["validationError"])(this.tokens.name, this, "require-exposed", message, {
        autofix: Object(_helpers_js__WEBPACK_IMPORTED_MODULE_4__["autofixAddExposedWindow"])(this)
      });
    }
    yield* super.validate(defs);
  }
}


/***/ }),
/* 29 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CallbackInterface", function() { return CallbackInterface; });
/* harmony import */ var _container_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(20);
/* harmony import */ var _operation_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(13);
/* harmony import */ var _constant_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(21);
// @ts-check





class CallbackInterface extends _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser, callback, { partial = null } = {}) {
    const tokens = { callback };
    tokens.base = tokeniser.consume("interface");
    if (!tokens.base) {
      return;
    }
    return _container_js__WEBPACK_IMPORTED_MODULE_0__["Container"].parse(tokeniser, new CallbackInterface({ source: tokeniser.source, tokens }), {
      type: "callback interface",
      inheritable: !partial,
      allowedMembers: [
        [_constant_js__WEBPACK_IMPORTED_MODULE_2__["Constant"].parse],
        [_operation_js__WEBPACK_IMPORTED_MODULE_1__["Operation"].parse, { regular: true }]
      ]
    });
  }

  get type() {
    return "callback interface";
  }
}


/***/ }),
/* 30 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "write", function() { return write; });


function noop(arg) {
  return arg;
}

const templates = {
  wrap: items => items.join(""),
  trivia: noop,
  name: noop,
  reference: noop,
  type: noop,
  generic: noop,
  nameless: noop,
  inheritance: noop,
  definition: noop,
  extendedAttribute: noop,
  extendedAttributeReference: noop
};

function write(ast, { templates: ts = templates } = {}) {
  ts = Object.assign({}, templates, ts);

  function reference(raw, { unescaped, context }) {
    if (!unescaped) {
      unescaped = raw.startsWith("_") ? raw.slice(1) : raw;
    }
    return ts.reference(raw, unescaped, context);
  }

  function token(t, wrapper = noop, ...args) {
    if (!t) {
      return "";
    }
    const value = wrapper(t.value, ...args);
    return ts.wrap([ts.trivia(t.trivia), value]);
  }

  function reference_token(t, context) {
    return token(t, reference, { context });
  }

  function name_token(t, arg) {
    return token(t, ts.name, arg);
  }

  function type_body(it) {
    if (it.union || it.generic) {
      return ts.wrap([
        token(it.tokens.base, ts.generic),
        token(it.tokens.open),
        ...it.subtype.map(type),
        token(it.tokens.close)
      ]);
    }
    const firstToken = it.tokens.prefix || it.tokens.base;
    const prefix = it.tokens.prefix ? [
      it.tokens.prefix.value,
      ts.trivia(it.tokens.base.trivia)
    ] : [];
    const ref = reference(ts.wrap([
      ...prefix,
      it.tokens.base.value,
      token(it.tokens.postfix)
    ]), { unescaped: it.idlType, context: it });
    return ts.wrap([ts.trivia(firstToken.trivia), ref]);
  }
  function type(it) {
    return ts.wrap([
      extended_attributes(it.extAttrs),
      type_body(it),
      token(it.tokens.nullable),
      token(it.tokens.separator)
    ]);
  }
  function default_(def) {
    if (!def) {
      return "";
    }
    return ts.wrap([
      token(def.tokens.assign),
      ...def.expression.map(t => token(t))
    ]);
  }
  function argument(arg) {
    return ts.wrap([
      extended_attributes(arg.extAttrs),
      token(arg.tokens.optional),
      ts.type(type(arg.idlType)),
      token(arg.tokens.variadic),
      name_token(arg.tokens.name, { data: arg }),
      default_(arg.default),
      token(arg.tokens.separator)
    ]);
  }
  function extended_attribute_listitem(str) {
    return ts.wrap([
      token(str.tokens.value),
      token(str.tokens.separator)
    ]);
  }
  function identifier(id, context) {
    return ts.wrap([
      reference_token(id.tokens.value, context),
      token(id.tokens.separator)
    ]);
  }
  function make_ext_at(it) {
    const { rhsType } = it.params;
    return ts.wrap([
      ts.trivia(it.tokens.name.trivia),
      ts.extendedAttribute(ts.wrap([
        ts.extendedAttributeReference(it.name),
        token(it.params.tokens.assign),
        reference_token(it.params.tokens.secondaryName, it),
        token(it.params.tokens.open),
        ...!it.params.list ? [] :
          it.params.list.map(
            rhsType === "identifier-list" ? id => identifier(id, it) :
            rhsType && rhsType.endsWith("-list") ? extended_attribute_listitem :
            argument
          ),
        token(it.params.tokens.close)
      ])),
      token(it.tokens.separator)
    ]);
  }
  function extended_attributes(eats) {
    if (!eats.length) return "";
    return ts.wrap([
      token(eats.tokens.open),
      ...eats.map(make_ext_at),
      token(eats.tokens.close)
    ]);
  }

  function operation(it, parent) {
    const body = it.idlType ? [
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.open),
      ts.wrap(it.arguments.map(argument)),
      token(it.tokens.close),
    ] : [];
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      it.tokens.name ? token(it.tokens.special) : token(it.tokens.special, ts.nameless, { data: it, parent }),
      ...body,
      token(it.tokens.termination)
    ]), { data: it, parent });
  }

  function attribute(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.special),
      token(it.tokens.readonly),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }

  function constructor(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base, ts.nameless, { data: it, parent }),
      token(it.tokens.open),
      ts.wrap(it.arguments.map(argument)),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }

  function inheritance(inh) {
    if (!inh.tokens.inheritance) {
      return "";
    }
    return ts.wrap([
      token(inh.tokens.colon),
      ts.trivia(inh.tokens.inheritance.trivia),
      ts.inheritance(reference(inh.tokens.inheritance.value, { context: inh }))
    ]);
  }

  function container(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.callback),
      token(it.tokens.partial),
      token(it.tokens.base),
      token(it.tokens.mixin),
      name_token(it.tokens.name, { data: it }),
      inheritance(it),
      token(it.tokens.open),
      iterate(it.members, it),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it });
  }

  function field(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.required),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      default_(it.default),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function const_(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.assign),
      token(it.tokens.value),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function typedef(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function includes(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      reference_token(it.tokens.target, it),
      token(it.tokens.includes),
      reference_token(it.tokens.mixin, it),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function callback(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.assign),
      ts.type(type(it.idlType)),
      token(it.tokens.open),
      ...it.arguments.map(argument),
      token(it.tokens.close),
      token(it.tokens.termination),
    ]), { data: it });
  }
  function enum_(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.open),
      iterate(it.values, it),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function enum_value(v, parent) {
    return ts.wrap([
      ts.trivia(v.tokens.value.trivia),
      ts.definition(
        ts.wrap(['"', ts.name(v.value, { data: v, parent }), '"']),
        { data: v, parent }
      ),
      token(v.tokens.separator)
    ]);
  }
  function iterable_like(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.readonly),
      token(it.tokens.async),
      token(it.tokens.base, ts.generic),
      token(it.tokens.open),
      ts.wrap(it.idlType.map(type)),
      token(it.tokens.close),
      token(it.tokens.argsOpen),
      ts.wrap(it.arguments.map(argument)),
      token(it.tokens.argsClose),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function eof(it) {
    return ts.trivia(it.trivia);
  }

  const table = {
    interface: container,
    "interface mixin": container,
    namespace: container,
    operation,
    attribute,
    constructor,
    dictionary: container,
    field,
    const: const_,
    typedef,
    includes,
    callback,
    enum: enum_,
    "enum-value": enum_value,
    iterable: iterable_like,
    maplike: iterable_like,
    setlike: iterable_like,
    "callback interface": container,
    eof
  };
  function dispatch(it, parent) {
    const dispatcher = table[it.type];
    if (!dispatcher) {
      throw new Error(`Type "${it.type}" is unsupported`);
    }
    return table[it.type](it, parent);
  }
  function iterate(things, parent) {
    if (!things) return;
    const results = things.map(thing => dispatch(thing, parent));
    return ts.wrap(results);
  }
  return iterate(ast);
}


/***/ }),
/* 31 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "validate", function() { return validate; });
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(3);




function getMixinMap(all, unique) {
  const map = new Map();
  const includes = all.filter(def => def.type === "includes");
  for (const include of includes) {
    const mixin = unique.get(include.includes);
    if (!mixin) {
      continue;
    }
    const array = map.get(include.target);
    if (array) {
      array.push(mixin);
    } else {
      map.set(include.target, [mixin]);
    }
  }
  return map;
}

/**
 * @typedef {ReturnType<typeof groupDefinitions>} Definitions
 */
function groupDefinitions(all) {
  const unique = new Map();
  const duplicates = new Set();
  const partials = new Map();
  for (const def of all) {
    if (def.partial) {
      const array = partials.get(def.name);
      if (array) {
        array.push(def);
      } else {
        partials.set(def.name, [def]);
      }
      continue;
    }
    if (!def.name) {
      continue;
    }
    if (!unique.has(def.name)) {
      unique.set(def.name, def);
    } else {
      duplicates.add(def);
    }
  }
  return {
    all,
    unique,
    partials,
    duplicates,
    mixinMap: getMixinMap(all, unique),
    cache: {
      typedefIncludesDictionary: new WeakMap(),
      dictionaryIncludesRequiredField: new WeakMap()
    },
  };
}

function* checkDuplicatedNames({ unique, duplicates }) {
  for (const dup of duplicates) {
    const { name } = dup;
    const message = `The name "${name}" of type "${unique.get(name).type}" was already seen`;
    yield Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["validationError"])(dup.tokens.name, dup, "no-duplicate", message);
  }
}

function* validateIterable(ast) {
  const defs = groupDefinitions(ast);
  for (const def of defs.all) {
    if (def.validate) {
      yield* def.validate(defs);
    }
  }
  yield* checkDuplicatedNames(defs);
}

// Remove this once all of our support targets expose `.flat()` by default
function flatten(array) {
  if (array.flat) {
    return array.flat();
  }
  return [].concat(...array);
}

/**
 * @param {*} ast AST or array of ASTs
 */
function validate(ast) {
  return [...validateIterable(flatten(ast))];
}


/***/ })
/******/ ]);
});
//# sourceMappingURL=webidl2.js.map