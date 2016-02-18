var mapping = require('./_mapping'),
    mutateMap = mapping.mutate,
    placeholder = {};

/**
 * The base implementation of `convert` which accepts a `util` object of methods
 * required to perform conversions.
 *
 * @param {Object} util The util object.
 * @param {string} name The name of the function to wrap.
 * @param {Function} func The function to wrap.
 * @param {Object} [options] The options object.
 * @param {boolean} [options.cap=true] Specify capping iteratee arguments.
 * @param {boolean} [options.curry=true] Specify currying.
 * @param {boolean} [options.fixed=true] Specify fixed arity.
 * @param {boolean} [options.immutable=true] Specify immutable operations.
 * @param {boolean} [options.rearg=true] Specify rearranging arguments.
 * @returns {Function|Object} Returns the converted function or object.
 */
function baseConvert(util, name, func, options) {
  options || (options = {});

  if (typeof func != 'function') {
    func = name;
    name = undefined;
  }
  if (func == null) {
    throw new TypeError;
  }
  var config = {
    'cap': 'cap' in options ? options.cap : true,
    'curry': 'curry' in options ? options.curry : true,
    'fixed': 'fixed' in options ? options.fixed : true,
    'immutable': 'immutable' in options ? options.immutable : true,
    'rearg': 'rearg' in options ? options.rearg : true
  };

  var forceRearg = ('rearg' in options) && options.rearg;

  var isLib = name === undefined && typeof func.VERSION == 'string';

  var _ = isLib ? func : {
    'ary': util.ary,
    'cloneDeep': util.cloneDeep,
    'curry': util.curry,
    'forEach': util.forEach,
    'isFunction': util.isFunction,
    'iteratee': util.iteratee,
    'keys': util.keys,
    'rearg': util.rearg,
    'spread': util.spread
  };

  var ary = _.ary,
      cloneDeep = _.cloneDeep,
      curry = _.curry,
      each = _.forEach,
      isFunction = _.isFunction,
      keys = _.keys,
      rearg = _.rearg,
      spread = _.spread;

  var baseArity = function(func, n) {
    return n == 2
      ? function(a, b) { return func.apply(undefined, arguments); }
      : function(a) { return func.apply(undefined, arguments); };
  };

  var baseAry = function(func, n) {
    return n == 2
      ? function(a, b) { return func(a, b); }
      : function(a) { return func(a); };
  };

  var cloneArray = function(array) {
    var length = array ? array.length : 0,
        result = Array(length);

    while (length--) {
      result[length] = array[length];
    }
    return result;
  };

  var createCloner = function(func) {
    return function(object) {
      return func({}, object);
    };
  };

  var immutWrap = function(func, cloner) {
    return overArg(func, cloner, true);
  };

  var iterateeAry = function(func, n) {
    return overArg(func, function(func) {
      return baseAry(func, n);
    });
  };

  var iterateeRearg = function(func, indexes) {
    return overArg(func, function(func) {
      var n = indexes.length;
      return baseArity(rearg(baseAry(func, n), indexes), n);
    });
  };

  var overArg = function(func, iteratee, retArg) {
    return function() {
      var length = arguments.length,
          args = Array(length);

      while (length--) {
        args[length] = arguments[length];
      }
      args[0] = iteratee(args[0]);
      var result = func.apply(undefined, args);
      return retArg ? args[0] : result;
    };
  };

  var wrappers = {
    'iteratee': function(iteratee) {
      return function() {
        var func = arguments[0],
            arity = arguments[1];

        if (!config.cap) {
          return iteratee(func, arity);
        }
        arity = arity > 2 ? (arity - 2) : 1;
        func = iteratee(func);
        var length = func.length;
        return (length && length <= arity) ? func : baseAry(func, arity);
      };
    },
    'mixin': function(mixin) {
      return function(source) {
        var func = this;
        if (!isFunction(func)) {
          return mixin(func, Object(source));
        }
        var methods = [],
            methodNames = [];

        each(keys(source), function(key) {
          var value = source[key];
          if (isFunction(value)) {
            methodNames.push(key);
            methods.push(func.prototype[key]);
          }
        });

        mixin(func, Object(source));

        each(methodNames, function(methodName, index) {
          var method = methods[index];
          if (isFunction(method)) {
            func.prototype[methodName] = method;
          } else {
            delete func.prototype[methodName];
          }
        });
        return func;
      };
    },
    'runInContext': function(runInContext) {
      return function(context) {
        return baseConvert(util, runInContext(context), undefined, options);
      };
    }
  };

  var wrap = function(name, func) {
    name = mapping.aliasToReal[name] || name;
    var wrapper = wrappers[name];
    if (wrapper) {
      return wrapper(func);
    }
    var wrapped = func;
    if (config.immutable) {
      if (mutateMap.array[name]) {
        wrapped = immutWrap(func, cloneArray);
      }
      else if (mutateMap.object[name]) {
        wrapped = immutWrap(func, createCloner(func));
      }
      else if (mutateMap.set[name]) {
        wrapped = immutWrap(func, cloneDeep);
      }
    }
    var result;
    each(mapping.caps, function(cap) {
      each(mapping.aryMethod[cap], function(otherName) {
        if (name == otherName) {
          var aryN = !isLib && mapping.iterateeAry[name],
              reargIndexes = mapping.iterateeRearg[name],
              spreadStart = mapping.methodSpread[name];

          if (config.fixed) {
            result = spreadStart === undefined
              ? ary(wrapped, cap)
              : spread(wrapped, spreadStart);
          }
          if (config.rearg && cap > 1 && (forceRearg || !mapping.skipRearg[name])) {
            result = rearg(result, mapping.methodRearg[name] || mapping.aryRearg[cap]);
          }
          if (config.cap) {
            if (reargIndexes) {
              result = iterateeRearg(result, reargIndexes);
            } else if (aryN) {
              result = iterateeAry(result, aryN);
            }
          }
          if (config.curry && cap > 1) {
            result = curry(result, cap);
          }
          return false;
        }
      });
      return !result;
    });

    result || (result = func);
    if (mapping.placeholder[name]) {
      func.placeholder = result.placeholder = placeholder;
    }
    return result;
  };

  if (!isLib) {
    return wrap(name, func);
  }
  // Add placeholder.
  _.placeholder = placeholder;

  // Iterate over methods for the current ary cap.
  var pairs = [];
  each(mapping.caps, function(cap) {
    each(mapping.aryMethod[cap], function(key) {
      var func = _[mapping.rename[key] || key];
      if (func) {
        pairs.push([key, wrap(key, func)]);
      }
    });
  });

  // Assign to `_` leaving `_.prototype` unchanged to allow chaining.
  each(pairs, function(pair) {
    _[pair[0]] = pair[1];
  });

  // Wrap the lodash method and its aliases.
  each(keys(_), function(key) {
    each(mapping.realToAlias[key] || [], function(alias) {
      _[alias] = _[key];
    });
  });

  return _;
}

module.exports = baseConvert;
