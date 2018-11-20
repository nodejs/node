"use strict";

exports.__esModule = true;
exports.default = addComments;

function addComments(node, type, comments) {
  if (!comments || !node) return node;
  var key = type + "Comments";

  if (node[key]) {
    if (type === "leading") {
      node[key] = comments.concat(node[key]);
    } else {
      node[key] = node[key].concat(comments);
    }
  } else {
    node[key] = comments;
  }

  return node;
}