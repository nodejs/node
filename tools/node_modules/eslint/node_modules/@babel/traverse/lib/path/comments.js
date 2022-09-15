"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addComment = addComment;
exports.addComments = addComments;
exports.shareCommentsWithSiblings = shareCommentsWithSiblings;

var _t = require("@babel/types");

const {
  addComment: _addComment,
  addComments: _addComments
} = _t;

function shareCommentsWithSiblings() {
  if (typeof this.key === "string") return;
  const node = this.node;
  if (!node) return;
  const trailing = node.trailingComments;
  const leading = node.leadingComments;
  if (!trailing && !leading) return;
  const prev = this.getSibling(this.key - 1);
  const next = this.getSibling(this.key + 1);
  const hasPrev = Boolean(prev.node);
  const hasNext = Boolean(next.node);

  if (hasPrev && !hasNext) {
    prev.addComments("trailing", trailing);
  } else if (hasNext && !hasPrev) {
    next.addComments("leading", leading);
  }
}

function addComment(type, content, line) {
  _addComment(this.node, type, content, line);
}

function addComments(type, comments) {
  _addComments(this.node, type, comments);
}

//# sourceMappingURL=comments.js.map
