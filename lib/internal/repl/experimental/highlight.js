'use strict';

const emphasize = require('internal/deps/emphasize/bundle');
const { inspect } = require('internal/util/inspect');

function makeStyled(color) {
  return (s) => `\x1b[${color[0]}m${s}\x1b[${color[1]}m`;
}

const sheet = {
  'comment': makeStyled(inspect.colors.grey),
  'quote': makeStyled(inspect.colors.grey),

  'keyword': makeStyled(inspect.colors.green),
  'addition': makeStyled(inspect.colors.green),

  'number': makeStyled(inspect.colors.yellow),
  'string': makeStyled(inspect.colors.green),
  'meta meta-string': makeStyled(inspect.colors.cyan),
  'literal': makeStyled(inspect.colors.cyan),
  'doctag': makeStyled(inspect.colors.cyan),
  'regexp': makeStyled(inspect.colors.cyan),

  'attribute': undefined,
  'attr': undefined,
  'variable': makeStyled(inspect.colors.yellow),
  'template-variable': makeStyled(inspect.colors.yellow),
  'class title': makeStyled(inspect.colors.yellow),
  'type': makeStyled(inspect.colors.yellow),

  'symbol': makeStyled(inspect.colors.magenta),
  'bullet': makeStyled(inspect.colors.magenta),
  'subst': makeStyled(inspect.colors.magenta),
  'meta': makeStyled(inspect.colors.magenta),
  'meta keyword': makeStyled(inspect.colors.magenta),
  'link': makeStyled(inspect.colors.magenta),

  'built_in': makeStyled(inspect.colors.cyan),
  'deletion': makeStyled(inspect.colors.red),

  'emphasis': makeStyled(inspect.colors.italic),
  'strong': makeStyled(inspect.colors.bold),
  'formula': makeStyled(inspect.colors.inverse),
};

const { highlight } = emphasize.createEmphasize({ javascript: emphasize.common.javascript });
module.exports = (s) => highlight('javascript', s, sheet).value;
