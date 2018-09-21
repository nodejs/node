"use strict";

module.exports = function(tokens, tt) {
  var startingToken = 0;
  var currentToken = 0;
  var numBraces = 0; // track use of {}
  var numBackQuotes = 0; // track number of nested templates

  function isBackQuote(token) {
    return tokens[token].type === tt.backQuote;
  }

  function isTemplateStarter(token) {
    return (
      isBackQuote(token) ||
      // only can be a template starter when in a template already
      (tokens[token].type === tt.braceR && numBackQuotes > 0)
    );
  }

  function isTemplateEnder(token) {
    return isBackQuote(token) || tokens[token].type === tt.dollarBraceL;
  }

  // append the values between start and end
  function createTemplateValue(start, end) {
    var value = "";
    while (start <= end) {
      if (tokens[start].value) {
        value += tokens[start].value;
      } else if (tokens[start].type !== tt.template) {
        value += tokens[start].type.label;
      }
      start++;
    }
    return value;
  }

  // create Template token
  function replaceWithTemplateType(start, end) {
    var templateToken = {
      type: "Template",
      value: createTemplateValue(start, end),
      start: tokens[start].start,
      end: tokens[end].end,
      loc: {
        start: tokens[start].loc.start,
        end: tokens[end].loc.end,
      },
    };

    // put new token in place of old tokens
    tokens.splice(start, end - start + 1, templateToken);
  }

  function trackNumBraces(token) {
    if (tokens[token].type === tt.braceL) {
      numBraces++;
    } else if (tokens[token].type === tt.braceR) {
      numBraces--;
    }
  }

  while (startingToken < tokens.length) {
    // template start: check if ` or }
    if (isTemplateStarter(startingToken) && numBraces === 0) {
      if (isBackQuote(startingToken)) {
        numBackQuotes++;
      }

      currentToken = startingToken + 1;

      // check if token after template start is "template"
      if (
        currentToken >= tokens.length - 1 ||
        tokens[currentToken].type !== tt.template
      ) {
        break;
      }

      // template end: find ` or ${
      while (!isTemplateEnder(currentToken)) {
        if (currentToken >= tokens.length - 1) {
          break;
        }
        currentToken++;
      }

      if (isBackQuote(currentToken)) {
        numBackQuotes--;
      }
      // template start and end found: create new token
      replaceWithTemplateType(startingToken, currentToken);
    } else if (numBackQuotes > 0) {
      trackNumBraces(startingToken);
    }
    startingToken++;
  }
};
