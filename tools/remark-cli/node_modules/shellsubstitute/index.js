module.exports = function (s, vars) {
  return s.replace(/(\\*)(\$([_a-z0-9]+)|\${([_a-z0-9]+)})/ig, function (_, escape, varExpression, variable, bracedVariable) {
    if (!(escape.length % 2)) {
      return escape.substring(Math.ceil(escape.length / 2)) + (vars[variable || bracedVariable] || '');
    } else {
      return escape.substring(1) + varExpression;
    }
  });
};
