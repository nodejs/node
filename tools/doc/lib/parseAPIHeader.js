'use strict';
module.exports = function parseAPIHeader(text) {
  text = text.replace(
    /(.*:)\s(\d)([\s\S]*)/,
    '<pre class="api_stability api_stability_$2">$1 $2$3</pre>'
  );
  return text;
};
