function _classCallCheck(instance, Constructor) {
  if (!(instance instanceof Constructor)) {
    throw new TypeError("Cannot call a class as a function");
  }
}

function _defineProperties(target, props) {
  for (var i = 0; i < props.length; i++) {
    var descriptor = props[i];
    descriptor.enumerable = descriptor.enumerable || false;
    descriptor.configurable = true;
    if ("value" in descriptor) descriptor.writable = true;
    Object.defineProperty(target, descriptor.key, descriptor);
  }
}

function _createClass(Constructor, protoProps, staticProps) {
  if (protoProps) _defineProperties(Constructor.prototype, protoProps);
  if (staticProps) _defineProperties(Constructor, staticProps);
  return Constructor;
}

function _setPrototypeOf(o, p) {
  _setPrototypeOf = Object.setPrototypeOf || function _setPrototypeOf(o, p) {
    o.__proto__ = p;
    return o;
  };

  return _setPrototypeOf(o, p);
}

function _isNativeReflectConstruct() {
  if (typeof Reflect === "undefined" || !Reflect.construct) return false;
  if (Reflect.construct.sham) return false;
  if (typeof Proxy === "function") return true;

  try {
    Date.prototype.toString.call(Reflect.construct(Date, [], function () {}));
    return true;
  } catch (e) {
    return false;
  }
}

function _construct(Parent, args, Class) {
  if (_isNativeReflectConstruct()) {
    _construct = Reflect.construct;
  } else {
    _construct = function _construct(Parent, args, Class) {
      var a = [null];
      a.push.apply(a, args);
      var Constructor = Function.bind.apply(Parent, a);
      var instance = new Constructor();
      if (Class) _setPrototypeOf(instance, Class.prototype);
      return instance;
    };
  }

  return _construct.apply(null, arguments);
}

/* eslint-disable node/no-unsupported-features/es-syntax */

/**
 * @param {RegExp} regex
 * @param {string} newFlags
 * @param {Integer} [newLastIndex=regex.lastIndex]
 * @returns {RegExp}
 */
function mixinRegex(regex, newFlags) {
  var newLastIndex = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : regex.lastIndex;
  newFlags = newFlags || '';
  regex = new RegExp(regex.source, (newFlags.includes('g') ? 'g' : regex.global ? 'g' : '') + (newFlags.includes('i') ? 'i' : regex.ignoreCase ? 'i' : '') + (newFlags.includes('m') ? 'm' : regex.multiline ? 'm' : '') + (newFlags.includes('u') ? 'u' : regex.unicode ? 'u' : '') + (newFlags.includes('y') ? 'y' : regex.sticky ? 'y' : '') + (newFlags.includes('s') ? 's' : regex.dotAll ? 's' : ''));
  regex.lastIndex = newLastIndex;
  return regex;
}

var RegExtras = /*#__PURE__*/function () {
  function RegExtras(regex, flags, newLastIndex) {
    _classCallCheck(this, RegExtras);

    this.regex = mixinRegex(typeof regex === 'string' ? new RegExp(regex) : mixinRegex(regex), flags || '', newLastIndex);
  }

  _createClass(RegExtras, [{
    key: "forEach",
    value: function forEach(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          n0,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        cb.apply(thisObj, matches.concat(i++, n0));
      }

      return this;
    }
  }, {
    key: "some",
    value: function some(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          ret,
          n0,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        ret = cb.apply(thisObj, matches.concat(i++, n0));

        if (ret) {
          return true;
        }
      }

      return false;
    }
  }, {
    key: "every",
    value: function every(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          ret,
          n0,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        ret = cb.apply(thisObj, matches.concat(i++, n0));

        if (!ret) {
          return false;
        }
      }

      return true;
    }
  }, {
    key: "map",
    value: function map(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var ret = [];
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          n0,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        ret.push(cb.apply(thisObj, matches.concat(i++, n0)));
      }

      return ret;
    }
  }, {
    key: "filter",
    value: function filter(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var matches,
          n0,
          i = 0;
      var ret = [],
          regex = mixinRegex(this.regex, 'g');

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        matches = matches.concat(i++, n0);

        if (cb.apply(thisObj, matches)) {
          ret.push(n0[0]);
        }
      }

      return ret;
    }
  }, {
    key: "reduce",
    value: function reduce(str, cb, prev) {
      var thisObj = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : null;
      var matches,
          n0,
          i = 0;
      var regex = mixinRegex(this.regex, 'g');

      if (!prev) {
        if ((matches = regex.exec(str)) !== null) {
          n0 = matches.splice(0, 1);
          prev = cb.apply(thisObj, [''].concat(matches.concat(i++, n0)));
        }
      }

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        prev = cb.apply(thisObj, [prev].concat(matches.concat(i++, n0)));
      }

      return prev;
    }
  }, {
    key: "reduceRight",
    value: function reduceRight(str, cb, prevOrig, thisObjOrig) {
      var matches,
          n0,
          i,
          thisObj = thisObjOrig,
          prev = prevOrig;
      var matchesContainer = [],
          regex = mixinRegex(this.regex, 'g');
      thisObj = thisObj || null;

      while ((matches = regex.exec(str)) !== null) {
        matchesContainer.push(matches);
      }

      i = matchesContainer.length;

      if (!i) {
        if (arguments.length < 3) {
          throw new TypeError('reduce of empty matches array with no initial value');
        }

        return prev;
      }

      if (!prev) {
        matches = matchesContainer.splice(-1)[0];
        n0 = matches.splice(0, 1);
        prev = cb.apply(thisObj, [''].concat(matches.concat(i--, n0)));
      }

      matchesContainer.reduceRight(function (container, mtches) {
        n0 = mtches.splice(0, 1);
        prev = cb.apply(thisObj, [prev].concat(mtches.concat(i--, n0)));
        return container;
      }, matchesContainer);
      return prev;
    }
  }, {
    key: "find",
    value: function find(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var matches,
          ret,
          n0,
          i = 0;
      var regex = mixinRegex(this.regex, 'g');

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        ret = cb.apply(thisObj, matches.concat(i++, n0));

        if (ret) {
          return n0[0];
        }
      }

      return false;
    }
  }, {
    key: "findIndex",
    value: function findIndex(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        var n0 = matches.splice(0, 1);
        var ret = cb.apply(thisObj, matches.concat(i++, n0));

        if (ret) {
          return i - 1;
        }
      }

      return -1;
    }
  }, {
    key: "findExec",
    value: function findExec(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var regex = mixinRegex(this.regex, 'g');
      var matches,
          i = 0;

      while ((matches = regex.exec(str)) !== null) {
        var n0 = matches.splice(0, 1);
        var ret = cb.apply(thisObj, matches.concat(i++, n0));

        if (ret) {
          return matches;
        }
      }

      return false;
    }
  }, {
    key: "filterExec",
    value: function filterExec(str, cb) {
      var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
      var matches,
          n0,
          i = 0;
      var ret = [],
          regex = mixinRegex(this.regex, 'g');

      while ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        matches.push(i++, n0[0]);

        if (cb.apply(thisObj, matches)) {
          ret.push(matches);
        }
      }

      return ret;
    }
  }]);

  return RegExtras;
}();

var _RegExtras = RegExtras;

RegExtras = function RegExtras() {
  for (var _len = arguments.length, args = new Array(_len), _key = 0; _key < _len; _key++) {
    args[_key] = arguments[_key];
  }

  // eslint-disable-line no-class-assign
  return _construct(_RegExtras, args);
};

RegExtras.prototype = _RegExtras.prototype;
RegExtras.mixinRegex = mixinRegex;

export { RegExtras, mixinRegex };
