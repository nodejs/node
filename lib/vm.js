var binding = process.binding('evals');

exports.Script = binding.Script;
exports.createScript = function(code, ctx, name) {
  return new exports.Script(code, ctx, name);
};

exports.createContext = binding.Script.createContext;
exports.runInContext = binding.Script.runInContext;
exports.runInThisContext = binding.Script.runInThisContext;
exports.runInNewContext = binding.Script.runInNewContext;
