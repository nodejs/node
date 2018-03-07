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
    this.divElement.attr(VISIBILITY, 'visible');
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
