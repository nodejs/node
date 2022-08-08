"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.explode = explode;
exports.merge = merge;
exports.verify = verify;

var virtualTypes = require("./path/lib/virtual-types");

var _t = require("@babel/types");

const {
  DEPRECATED_KEYS,
  FLIPPED_ALIAS_KEYS,
  TYPES
} = _t;

function isVirtualType(type) {
  return type in virtualTypes;
}

function explode(visitor) {
  if (visitor._exploded) return visitor;
  visitor._exploded = true;

  for (const nodeType of Object.keys(visitor)) {
    if (shouldIgnoreKey(nodeType)) continue;
    const parts = nodeType.split("|");
    if (parts.length === 1) continue;
    const fns = visitor[nodeType];
    delete visitor[nodeType];

    for (const part of parts) {
      visitor[part] = fns;
    }
  }

  verify(visitor);
  delete visitor.__esModule;
  ensureEntranceObjects(visitor);
  ensureCallbackArrays(visitor);

  for (const nodeType of Object.keys(visitor)) {
    if (shouldIgnoreKey(nodeType)) continue;
    if (!isVirtualType(nodeType)) continue;
    const fns = visitor[nodeType];

    for (const type of Object.keys(fns)) {
      fns[type] = wrapCheck(nodeType, fns[type]);
    }

    delete visitor[nodeType];
    const types = virtualTypes[nodeType];

    if (types !== null) {
      for (const type of types) {
        if (visitor[type]) {
          mergePair(visitor[type], fns);
        } else {
          visitor[type] = fns;
        }
      }
    } else {
      mergePair(visitor, fns);
    }
  }

  for (const nodeType of Object.keys(visitor)) {
    if (shouldIgnoreKey(nodeType)) continue;
    const fns = visitor[nodeType];
    let aliases = FLIPPED_ALIAS_KEYS[nodeType];
    const deprecatedKey = DEPRECATED_KEYS[nodeType];

    if (deprecatedKey) {
      console.trace(`Visitor defined for ${nodeType} but it has been renamed to ${deprecatedKey}`);
      aliases = [deprecatedKey];
    }

    if (!aliases) continue;
    delete visitor[nodeType];

    for (const alias of aliases) {
      const existing = visitor[alias];

      if (existing) {
        mergePair(existing, fns);
      } else {
        visitor[alias] = Object.assign({}, fns);
      }
    }
  }

  for (const nodeType of Object.keys(visitor)) {
    if (shouldIgnoreKey(nodeType)) continue;
    ensureCallbackArrays(visitor[nodeType]);
  }

  return visitor;
}

function verify(visitor) {
  if (visitor._verified) return;

  if (typeof visitor === "function") {
    throw new Error("You passed `traverse()` a function when it expected a visitor object, " + "are you sure you didn't mean `{ enter: Function }`?");
  }

  for (const nodeType of Object.keys(visitor)) {
    if (nodeType === "enter" || nodeType === "exit") {
      validateVisitorMethods(nodeType, visitor[nodeType]);
    }

    if (shouldIgnoreKey(nodeType)) continue;

    if (TYPES.indexOf(nodeType) < 0) {
      throw new Error(`You gave us a visitor for the node type ${nodeType} but it's not a valid type`);
    }

    const visitors = visitor[nodeType];

    if (typeof visitors === "object") {
      for (const visitorKey of Object.keys(visitors)) {
        if (visitorKey === "enter" || visitorKey === "exit") {
          validateVisitorMethods(`${nodeType}.${visitorKey}`, visitors[visitorKey]);
        } else {
          throw new Error("You passed `traverse()` a visitor object with the property " + `${nodeType} that has the invalid property ${visitorKey}`);
        }
      }
    }
  }

  visitor._verified = true;
}

function validateVisitorMethods(path, val) {
  const fns = [].concat(val);

  for (const fn of fns) {
    if (typeof fn !== "function") {
      throw new TypeError(`Non-function found defined in ${path} with type ${typeof fn}`);
    }
  }
}

function merge(visitors, states = [], wrapper) {
  const rootVisitor = {};

  for (let i = 0; i < visitors.length; i++) {
    const visitor = visitors[i];
    const state = states[i];
    explode(visitor);

    for (const type of Object.keys(visitor)) {
      let visitorType = visitor[type];

      if (state || wrapper) {
        visitorType = wrapWithStateOrWrapper(visitorType, state, wrapper);
      }

      const nodeVisitor = rootVisitor[type] = rootVisitor[type] || {};
      mergePair(nodeVisitor, visitorType);
    }
  }

  return rootVisitor;
}

function wrapWithStateOrWrapper(oldVisitor, state, wrapper) {
  const newVisitor = {};

  for (const key of Object.keys(oldVisitor)) {
    let fns = oldVisitor[key];
    if (!Array.isArray(fns)) continue;
    fns = fns.map(function (fn) {
      let newFn = fn;

      if (state) {
        newFn = function (path) {
          return fn.call(state, path, state);
        };
      }

      if (wrapper) {
        newFn = wrapper(state.key, key, newFn);
      }

      if (newFn !== fn) {
        newFn.toString = () => fn.toString();
      }

      return newFn;
    });
    newVisitor[key] = fns;
  }

  return newVisitor;
}

function ensureEntranceObjects(obj) {
  for (const key of Object.keys(obj)) {
    if (shouldIgnoreKey(key)) continue;
    const fns = obj[key];

    if (typeof fns === "function") {
      obj[key] = {
        enter: fns
      };
    }
  }
}

function ensureCallbackArrays(obj) {
  if (obj.enter && !Array.isArray(obj.enter)) obj.enter = [obj.enter];
  if (obj.exit && !Array.isArray(obj.exit)) obj.exit = [obj.exit];
}

function wrapCheck(nodeType, fn) {
  const newFn = function (path) {
    if (path[`is${nodeType}`]()) {
      return fn.apply(this, arguments);
    }
  };

  newFn.toString = () => fn.toString();

  return newFn;
}

function shouldIgnoreKey(key) {
  if (key[0] === "_") return true;
  if (key === "enter" || key === "exit" || key === "shouldSkip") return true;

  if (key === "denylist" || key === "noScope" || key === "skipKeys" || key === "blacklist") {
    return true;
  }

  return false;
}

function mergePair(dest, src) {
  for (const key of Object.keys(src)) {
    dest[key] = [].concat(dest[key] || [], src[key]);
  }
}