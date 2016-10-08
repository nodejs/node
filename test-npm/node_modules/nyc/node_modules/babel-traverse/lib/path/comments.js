"use strict";

exports.__esModule = true;
exports.shareCommentsWithSiblings = shareCommentsWithSiblings;
exports.addComment = addComment;
exports.addComments = addComments;
// This file contains methods responsible for dealing with comments.

/**
 * Share comments amongst siblings.
 */

function shareCommentsWithSiblings() {
  var node = this.node;
  if (!node) return;

  var trailing = node.trailingComments;
  var leading = node.leadingComments;
  if (!trailing && !leading) return;

  var prev = this.getSibling(this.key - 1);
  var next = this.getSibling(this.key + 1);

  if (!prev.node) prev = next;
  if (!next.node) next = prev;

  prev.addComments("trailing", leading);
  next.addComments("leading", trailing);
}

function addComment(type, content, line) {
  this.addComments(type, [{
    type: line ? "CommentLine" : "CommentBlock",
    value: content
  }]);
}

/**
 * Give node `comments` of the specified `type`.
 */

function addComments(type, comments) {
  if (!comments) return;

  var node = this.node;
  if (!node) return;

  var key = type + "Comments";

  if (node[key]) {
    node[key] = node[key].concat(comments);
  } else {
    node[key] = comments;
  }
}