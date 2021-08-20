"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

function _gensync() {
  const data = require("gensync");

  _gensync = function () {
    return data;
  };

  return data;
}

var _async = require("../gensync-utils/async");

var _util = require("./util");

var context = require("../index");

var _plugin = require("./plugin");

var _item = require("./item");

var _configChain = require("./config-chain");

function _traverse() {
  const data = require("@babel/traverse");

  _traverse = function () {
    return data;
  };

  return data;
}

var _caching = require("./caching");

var _options = require("./validation/options");

var _plugins = require("./validation/plugins");

var _configApi = require("./helpers/config-api");

var _partial = require("./partial");

var Context = require("./cache-contexts");

var _default = _gensync()(function* loadFullConfig(inputOpts) {
  var _opts$assumptions;

  const result = yield* (0, _partial.default)(inputOpts);

  if (!result) {
    return null;
  }

  const {
    options,
    context,
    fileHandling
  } = result;

  if (fileHandling === "ignored") {
    return null;
  }

  const optionDefaults = {};
  const {
    plugins,
    presets
  } = options;

  if (!plugins || !presets) {
    throw new Error("Assertion failure - plugins and presets exist");
  }

  const presetContext = Object.assign({}, context, {
    targets: options.targets
  });

  const toDescriptor = item => {
    const desc = (0, _item.getItemDescriptor)(item);

    if (!desc) {
      throw new Error("Assertion failure - must be config item");
    }

    return desc;
  };

  const presetsDescriptors = presets.map(toDescriptor);
  const initialPluginsDescriptors = plugins.map(toDescriptor);
  const pluginDescriptorsByPass = [[]];
  const passes = [];
  const ignored = yield* enhanceError(context, function* recursePresetDescriptors(rawPresets, pluginDescriptorsPass) {
    const presets = [];

    for (let i = 0; i < rawPresets.length; i++) {
      const descriptor = rawPresets[i];

      if (descriptor.options !== false) {
        try {
          if (descriptor.ownPass) {
            presets.push({
              preset: yield* loadPresetDescriptor(descriptor, presetContext),
              pass: []
            });
          } else {
            presets.unshift({
              preset: yield* loadPresetDescriptor(descriptor, presetContext),
              pass: pluginDescriptorsPass
            });
          }
        } catch (e) {
          if (e.code === "BABEL_UNKNOWN_OPTION") {
            (0, _options.checkNoUnwrappedItemOptionPairs)(rawPresets, i, "preset", e);
          }

          throw e;
        }
      }
    }

    if (presets.length > 0) {
      pluginDescriptorsByPass.splice(1, 0, ...presets.map(o => o.pass).filter(p => p !== pluginDescriptorsPass));

      for (const {
        preset,
        pass
      } of presets) {
        if (!preset) return true;
        pass.push(...preset.plugins);
        const ignored = yield* recursePresetDescriptors(preset.presets, pass);
        if (ignored) return true;
        preset.options.forEach(opts => {
          (0, _util.mergeOptions)(optionDefaults, opts);
        });
      }
    }
  })(presetsDescriptors, pluginDescriptorsByPass[0]);
  if (ignored) return null;
  const opts = optionDefaults;
  (0, _util.mergeOptions)(opts, options);
  const pluginContext = Object.assign({}, presetContext, {
    assumptions: (_opts$assumptions = opts.assumptions) != null ? _opts$assumptions : {}
  });
  yield* enhanceError(context, function* loadPluginDescriptors() {
    pluginDescriptorsByPass[0].unshift(...initialPluginsDescriptors);

    for (const descs of pluginDescriptorsByPass) {
      const pass = [];
      passes.push(pass);

      for (let i = 0; i < descs.length; i++) {
        const descriptor = descs[i];

        if (descriptor.options !== false) {
          try {
            pass.push(yield* loadPluginDescriptor(descriptor, pluginContext));
          } catch (e) {
            if (e.code === "BABEL_UNKNOWN_PLUGIN_PROPERTY") {
              (0, _options.checkNoUnwrappedItemOptionPairs)(descs, i, "plugin", e);
            }

            throw e;
          }
        }
      }
    }
  })();
  opts.plugins = passes[0];
  opts.presets = passes.slice(1).filter(plugins => plugins.length > 0).map(plugins => ({
    plugins
  }));
  opts.passPerPreset = opts.presets.length > 0;
  return {
    options: opts,
    passes: passes
  };
});

exports.default = _default;

function enhanceError(context, fn) {
  return function* (arg1, arg2) {
    try {
      return yield* fn(arg1, arg2);
    } catch (e) {
      if (!/^\[BABEL\]/.test(e.message)) {
        e.message = `[BABEL] ${context.filename || "unknown"}: ${e.message}`;
      }

      throw e;
    }
  };
}

const makeDescriptorLoader = apiFactory => (0, _caching.makeWeakCache)(function* ({
  value,
  options,
  dirname,
  alias
}, cache) {
  if (options === false) throw new Error("Assertion failure");
  options = options || {};
  let item = value;

  if (typeof value === "function") {
    const factory = (0, _async.maybeAsync)(value, `You appear to be using an async plugin/preset, but Babel has been called synchronously`);
    const api = Object.assign({}, context, apiFactory(cache));

    try {
      item = yield* factory(api, options, dirname);
    } catch (e) {
      if (alias) {
        e.message += ` (While processing: ${JSON.stringify(alias)})`;
      }

      throw e;
    }
  }

  if (!item || typeof item !== "object") {
    throw new Error("Plugin/Preset did not return an object.");
  }

  if ((0, _async.isThenable)(item)) {
    yield* [];
    throw new Error(`You appear to be using a promise as a plugin, ` + `which your current version of Babel does not support. ` + `If you're using a published plugin, ` + `you may need to upgrade your @babel/core version. ` + `As an alternative, you can prefix the promise with "await". ` + `(While processing: ${JSON.stringify(alias)})`);
  }

  return {
    value: item,
    options,
    dirname,
    alias
  };
});

const pluginDescriptorLoader = makeDescriptorLoader(_configApi.makePluginAPI);
const presetDescriptorLoader = makeDescriptorLoader(_configApi.makePresetAPI);

function* loadPluginDescriptor(descriptor, context) {
  if (descriptor.value instanceof _plugin.default) {
    if (descriptor.options) {
      throw new Error("Passed options to an existing Plugin instance will not work.");
    }

    return descriptor.value;
  }

  return yield* instantiatePlugin(yield* pluginDescriptorLoader(descriptor, context), context);
}

const instantiatePlugin = (0, _caching.makeWeakCache)(function* ({
  value,
  options,
  dirname,
  alias
}, cache) {
  const pluginObj = (0, _plugins.validatePluginObject)(value);
  const plugin = Object.assign({}, pluginObj);

  if (plugin.visitor) {
    plugin.visitor = _traverse().default.explode(Object.assign({}, plugin.visitor));
  }

  if (plugin.inherits) {
    const inheritsDescriptor = {
      name: undefined,
      alias: `${alias}$inherits`,
      value: plugin.inherits,
      options,
      dirname
    };
    const inherits = yield* (0, _async.forwardAsync)(loadPluginDescriptor, run => {
      return cache.invalidate(data => run(inheritsDescriptor, data));
    });
    plugin.pre = chain(inherits.pre, plugin.pre);
    plugin.post = chain(inherits.post, plugin.post);
    plugin.manipulateOptions = chain(inherits.manipulateOptions, plugin.manipulateOptions);
    plugin.visitor = _traverse().default.visitors.merge([inherits.visitor || {}, plugin.visitor || {}]);
  }

  return new _plugin.default(plugin, options, alias);
});

const validateIfOptionNeedsFilename = (options, descriptor) => {
  if (options.test || options.include || options.exclude) {
    const formattedPresetName = descriptor.name ? `"${descriptor.name}"` : "/* your preset */";
    throw new Error([`Preset ${formattedPresetName} requires a filename to be set when babel is called directly,`, `\`\`\``, `babel.transform(code, { filename: 'file.ts', presets: [${formattedPresetName}] });`, `\`\`\``, `See https://babeljs.io/docs/en/options#filename for more information.`].join("\n"));
  }
};

const validatePreset = (preset, context, descriptor) => {
  if (!context.filename) {
    const {
      options
    } = preset;
    validateIfOptionNeedsFilename(options, descriptor);

    if (options.overrides) {
      options.overrides.forEach(overrideOptions => validateIfOptionNeedsFilename(overrideOptions, descriptor));
    }
  }
};

function* loadPresetDescriptor(descriptor, context) {
  const preset = instantiatePreset(yield* presetDescriptorLoader(descriptor, context));
  validatePreset(preset, context, descriptor);
  return yield* (0, _configChain.buildPresetChain)(preset, context);
}

const instantiatePreset = (0, _caching.makeWeakCacheSync)(({
  value,
  dirname,
  alias
}) => {
  return {
    options: (0, _options.validate)("preset", value),
    alias,
    dirname
  };
});

function chain(a, b) {
  const fns = [a, b].filter(Boolean);
  if (fns.length <= 1) return fns[0];
  return function (...args) {
    for (const fn of fns) {
      fn.apply(this, args);
    }
  };
}