// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class View {
  constructor(id, broker) {
    this.divElement = d3.select("#" + id);
    this.divNode = this.divElement[0][0];
    this.parentNode = this.divNode.parentNode;
  }

  isScrollable() {
    return false;
  }

  show(data, rememberedSelection) {
    this.parentNode.appendChild(this.divElement[0][0]);
    this.initializeContent(data, rememberedSelection);
    this.resizeToParent();
    this.divElement.attr(VISIBILITY, 'visible');
  }

  resizeToParent() {
    var view = this;
    var documentElement = document.documentElement;
    var y;
    if (this.parentNode.clientHeight)
      y = Math.max(this.parentNode.clientHeight, documentElement.clientHeight);
    else
      y = documentElement.clientHeight;
    this.parentNode.style.height = y + 'px';
  }

  hide() {
    this.divElement.attr(VISIBILITY, 'hidden');
    this.deleteContent();
    this.parentNode.removeChild(this.divNode);
  }

  detachSelection() {
    return null;
  }
}
