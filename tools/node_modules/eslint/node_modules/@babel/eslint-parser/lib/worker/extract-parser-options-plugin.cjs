module.exports = function extractParserOptionsPlugin() {
  return {
    parserOverride(code, opts) {
      return opts;
    }

  };
};