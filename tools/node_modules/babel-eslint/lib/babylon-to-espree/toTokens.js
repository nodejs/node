"use strict";

var convertTemplateType = require("./convertTemplateType");
var toToken = require("./toToken");

module.exports = function(tokens, tt, code) {
  // transform tokens to type "Template"
  convertTemplateType(tokens, tt);

  var transformedTokens = [];
  for (var i = 0; i < tokens.length; i++) {
    var token = tokens[i];
    if (token.type !== "CommentLine" && token.type !== "CommentBlock") {
      transformedTokens.push(toToken(token, tt, code));
    }
  }

  return transformedTokens;
};
