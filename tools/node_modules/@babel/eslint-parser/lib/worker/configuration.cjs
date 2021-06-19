const babel = require("./babel-core.cjs");

function getParserPlugins(babelOptions) {
  var _babelOptions$parserO, _babelOptions$parserO2;

  const babelParserPlugins = (_babelOptions$parserO = (_babelOptions$parserO2 = babelOptions.parserOpts) == null ? void 0 : _babelOptions$parserO2.plugins) != null ? _babelOptions$parserO : [];
  const estreeOptions = {
    classFeatures: false
  };

  for (const plugin of babelParserPlugins) {
    if (Array.isArray(plugin) && plugin[0] === "estree") {
      Object.assign(estreeOptions, plugin[1]);
      break;
    }
  }

  return [["estree", estreeOptions], ...babelParserPlugins];
}

function normalizeParserOptions(options) {
  return Object.assign({
    sourceType: options.sourceType,
    filename: options.filePath
  }, options.babelOptions, {
    parserOpts: Object.assign({
      allowImportExportEverywhere: options.allowImportExportEverywhere,
      allowReturnOutsideFunction: true,
      allowSuperOutsideMethod: true
    }, options.babelOptions.parserOpts, {
      plugins: getParserPlugins(options.babelOptions),
      ranges: true,
      tokens: true
    }),
    caller: Object.assign({
      name: "@babel/eslint-parser"
    }, options.babelOptions.caller)
  });
}

function validateResolvedConfig(config, options) {
  if (config !== null) {
    if (options.requireConfigFile !== false) {
      if (!config.hasFilesystemConfig()) {
        let error = `No Babel config file detected for ${config.options.filename}. Either disable config file checking with requireConfigFile: false, or configure Babel so that it can find the config files.`;

        if (config.options.filename.includes("node_modules")) {
          error += `\nIf you have a .babelrc.js file or use package.json#babel, keep in mind that it's not used when parsing dependencies. If you want your config to be applied to your whole app, consider using babel.config.js or babel.config.json instead.`;
        }

        throw new Error(error);
      }
    }

    return config.options;
  }
}

function getDefaultParserOptions(options) {
  return Object.assign({
    plugins: []
  }, options, {
    babelrc: false,
    configFile: false,
    browserslistConfigFile: false,
    ignore: null,
    only: null
  });
}

module.exports = function normalizeBabelParseConfig(options) {
  const parseOptions = normalizeParserOptions(options);
  {
    const config = babel.loadPartialConfigSync(parseOptions);
    return validateConfigWithFallback(config);
  }

  function validateConfigWithFallback(inputConfig) {
    const result = validateResolvedConfig(inputConfig, options);

    if (result) {
      return result;
    } else {
      return getDefaultParserOptions(parseOptions);
    }
  }
};