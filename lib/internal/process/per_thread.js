'use strict';

// This files contains process bootstrappers that can be
// run when setting up each thread, including the main
// thread and the worker threads.

const {
  ArrayIsArray,
  BigUint64Array,
  NumberMAX_SAFE_INTEGER,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetOwnPropertyDescriptors,
  RegExpPrototypeTest,
  Set,
  SetPrototype,
  SetPrototypeHas,
  StringPrototypeReplace,
} = primordials;

const {
  errnoException,
  codes: {
    ERR_ASSERTION,
    ERR_CPU_USAGE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_OPT_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_UNKNOWN_SIGNAL
  }
} = require('internal/errors');
const format = require('internal/util/inspect').format;
const constants = internalBinding('constants').os.signals;

function assert(x, msg) {
  if (!x) throw new ERR_ASSERTION(msg || 'assertion error');
}

// The execution of this function itself should not cause any side effects.
function wrapProcessMethods(binding) {
  const {
    hrtime: _hrtime,
    hrtimeBigInt: _hrtimeBigInt,
    cpuUsage: _cpuUsage,
    memoryUsage: _memoryUsage,
    resourceUsage: _resourceUsage
  } = binding;

  function _rawDebug(...args) {
    binding._rawDebug(format.apply(null, args));
  }

  // Create the argument array that will be passed to the native function.
  const cpuValues = new Float64Array(2);

  // Replace the native function with the JS version that calls the native
  // function.
  function cpuUsage(prevValue) {
    // If a previous value was passed in, ensure it has the correct shape.
    if (prevValue) {
      if (!previousValueIsValid(prevValue.user)) {
        if (typeof prevValue !== 'object')
          throw new ERR_INVALID_ARG_TYPE('prevValue', 'object', prevValue);

        if (typeof prevValue.user !== 'number') {
          throw new ERR_INVALID_ARG_TYPE('prevValue.user',
                                         'number', prevValue.user);
        }
        throw new ERR_INVALID_OPT_VALUE.RangeError('prevValue.user',
                                                   prevValue.user);
      }

      if (!previousValueIsValid(prevValue.system)) {
        if (typeof prevValue.system !== 'number') {
          throw new ERR_INVALID_ARG_TYPE('prevValue.system',
                                         'number', prevValue.system);
        }
        throw new ERR_INVALID_OPT_VALUE.RangeError('prevValue.system',
                                                   prevValue.system);
      }
    }

    // Call the native function to get the current values.
    const errmsg = _cpuUsage(cpuValues);
    if (errmsg) {
      throw new ERR_CPU_USAGE(errmsg);
    }

    // If a previous value was passed in, return diff of current from previous.
    if (prevValue) {
      return {
        user: cpuValues[0] - prevValue.user,
        system: cpuValues[1] - prevValue.system
      };
    }

    // If no previous value passed in, return current value.
    return {
      user: cpuValues[0],
      system: cpuValues[1]
    };
  }

  // Ensure that a previously passed in value is valid. Currently, the native
  // implementation always returns numbers <= Number.MAX_SAFE_INTEGER.
  function previousValueIsValid(num) {
    return typeof num === 'number' &&
        num <= NumberMAX_SAFE_INTEGER &&
        num >= 0;
  }

  // The 3 entries filled in by the original process.hrtime contains
  // the upper/lower 32 bits of the second part of the value,
  // and the remaining nanoseconds of the value.
  const hrValues = new Uint32Array(3);

  function hrtime(time) {
    _hrtime(hrValues);

    if (time !== undefined) {
      if (!ArrayIsArray(time)) {
        throw new ERR_INVALID_ARG_TYPE('time', 'Array', time);
      }
      if (time.length !== 2) {
        throw new ERR_OUT_OF_RANGE('time', 2, time.length);
      }

      const sec = (hrValues[0] * 0x100000000 + hrValues[1]) - time[0];
      const nsec = hrValues[2] - time[1];
      const needsBorrow = nsec < 0;
      return [needsBorrow ? sec - 1 : sec, needsBorrow ? nsec + 1e9 : nsec];
    }

    return [
      hrValues[0] * 0x100000000 + hrValues[1],
      hrValues[2]
    ];
  }

  // Use a BigUint64Array in the closure because this is actually a bit
  // faster than simply returning a BigInt from C++ in V8 7.1.
  const hrBigintValues = new BigUint64Array(1);
  function hrtimeBigInt() {
    _hrtimeBigInt(hrBigintValues);
    return hrBigintValues[0];
  }

  const memValues = new Float64Array(5);
  function memoryUsage() {
    _memoryUsage(memValues);
    return {
      rss: memValues[0],
      heapTotal: memValues[1],
      heapUsed: memValues[2],
      external: memValues[3],
      arrayBuffers: memValues[4]
    };
  }

  function exit(code) {
    if (code || code === 0)
      process.exitCode = code;

    if (!process._exiting) {
      process._exiting = true;
      process.emit('exit', process.exitCode || 0);
    }
    // FIXME(joyeecheung): This is an undocumented API that gets monkey-patched
    // in the user land. Either document it, or deprecate it in favor of a
    // better public alternative.
    process.reallyExit(process.exitCode || 0);
  }

  function kill(pid, sig) {
    let err;

    // eslint-disable-next-line eqeqeq
    if (pid != (pid | 0)) {
      throw new ERR_INVALID_ARG_TYPE('pid', 'number', pid);
    }

    // Preserve null signal
    if (sig === (sig | 0)) {
      // XXX(joyeecheung): we have to use process._kill here because
      // it's monkey-patched by tests.
      err = process._kill(pid, sig);
    } else {
      sig = sig || 'SIGTERM';
      if (constants[sig]) {
        err = process._kill(pid, constants[sig]);
      } else {
        throw new ERR_UNKNOWN_SIGNAL(sig);
      }
    }

    if (err)
      throw errnoException(err, 'kill');

    return true;
  }

  const resourceValues = new Float64Array(16);
  function resourceUsage() {
    _resourceUsage(resourceValues);
    return {
      userCPUTime: resourceValues[0],
      systemCPUTime: resourceValues[1],
      maxRSS: resourceValues[2],
      sharedMemorySize: resourceValues[3],
      unsharedDataSize: resourceValues[4],
      unsharedStackSize: resourceValues[5],
      minorPageFault: resourceValues[6],
      majorPageFault: resourceValues[7],
      swappedOut: resourceValues[8],
      fsRead: resourceValues[9],
      fsWrite: resourceValues[10],
      ipcSent: resourceValues[11],
      ipcReceived: resourceValues[12],
      signalsCount: resourceValues[13],
      voluntaryContextSwitches: resourceValues[14],
      involuntaryContextSwitches: resourceValues[15]
    };
  }


  return {
    _rawDebug,
    hrtime,
    hrtimeBigInt,
    cpuUsage,
    resourceUsage,
    memoryUsage,
    kill,
    exit
  };
}

const replaceUnderscoresRegex = /_/g;
const leadingDashesRegex = /^--?/;
const trailingValuesRegex = /=.*$/;

// This builds the initial process.allowedNodeEnvironmentFlags
// from data in the config binding.
function buildAllowedFlags() {
  const {
    envSettings: { kAllowedInEnvironment }
  } = internalBinding('options');
  const { options, aliases } = require('internal/options');

  const allowedNodeEnvironmentFlags = [];
  for (const [name, info] of options) {
    if (info.envVarSettings === kAllowedInEnvironment) {
      allowedNodeEnvironmentFlags.push(name);
    }
  }

  for (const [ from, expansion ] of aliases) {
    let isAccepted = true;
    for (const to of expansion) {
      if (!to.startsWith('-') || to === '--') continue;
      const recursiveExpansion = aliases.get(to);
      if (recursiveExpansion) {
        if (recursiveExpansion[0] === to)
          recursiveExpansion.splice(0, 1);
        expansion.push(...recursiveExpansion);
        continue;
      }
      isAccepted = options.get(to).envVarSettings === kAllowedInEnvironment;
      if (!isAccepted) break;
    }
    if (isAccepted) {
      let canonical = from;
      if (canonical.endsWith('='))
        canonical = canonical.substr(0, canonical.length - 1);
      if (canonical.endsWith(' <arg>'))
        canonical = canonical.substr(0, canonical.length - 4);
      allowedNodeEnvironmentFlags.push(canonical);
    }
  }

  const trimLeadingDashes =
    (flag) => StringPrototypeReplace(flag, leadingDashesRegex, '');

  // Save these for comparison against flags provided to
  // process.allowedNodeEnvironmentFlags.has() which lack leading dashes.
  // Avoid interference w/ user code by flattening `Set.prototype` into
  // each object.
  const nodeFlags = ObjectDefineProperties(
    new Set(allowedNodeEnvironmentFlags.map(trimLeadingDashes)),
    ObjectGetOwnPropertyDescriptors(SetPrototype)
  );

  class NodeEnvironmentFlagsSet extends Set {
    constructor(...args) {
      super(...args);

      // The super constructor consumes `add`, but
      // disallow any future adds.
      ObjectDefineProperty(this, 'add', {
        value: () => this
      });
    }

    delete() {
      // No-op, `Set` API compatible
      return false;
    }

    clear() {
      // No-op
    }

    has(key) {
      // This will return `true` based on various possible
      // permutations of a flag, including present/missing leading
      // dash(es) and/or underscores-for-dashes.
      // Strips any values after `=`, inclusive.
      // TODO(addaleax): It might be more flexible to run the option parser
      // on a dummy option set and see whether it rejects the argument or
      // not.
      if (typeof key === 'string') {
        key = StringPrototypeReplace(key, replaceUnderscoresRegex, '-');
        if (RegExpPrototypeTest(leadingDashesRegex, key)) {
          key = StringPrototypeReplace(key, trailingValuesRegex, '');
          return SetPrototypeHas(this, key);
        }
        return SetPrototypeHas(nodeFlags, key);
      }
      return false;
    }
  }

  ObjectFreeze(NodeEnvironmentFlagsSet.prototype.constructor);
  ObjectFreeze(NodeEnvironmentFlagsSet.prototype);

  return ObjectFreeze(new NodeEnvironmentFlagsSet(
    allowedNodeEnvironmentFlags
  ));
}

// Lazy load internal/trace_events_async_hooks only if the async_hooks
// trace event category is enabled.
let traceEventsAsyncHook;
// Dynamically enable/disable the traceEventsAsyncHook
function toggleTraceCategoryState(asyncHooksEnabled) {
  if (asyncHooksEnabled) {
    if (!traceEventsAsyncHook) {
      traceEventsAsyncHook =
        require('internal/trace_events_async_hooks').createHook();
    }
    traceEventsAsyncHook.enable();
  } else if (traceEventsAsyncHook) {
    traceEventsAsyncHook.disable();
  }
}

module.exports = {
  toggleTraceCategoryState,
  assert,
  buildAllowedFlags,
  wrapProcessMethods
};
