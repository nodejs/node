// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class EmptyView extends View {
  constructor(id, broker) {
    super(id, broker);
    this.svg = this.divElement.append("svg").attr('version','1.1').attr("width", "100%");
  }

  initializeContent(data, rememberedSelection) {
    this.svg.attr("height", document.documentElement.clientHeight + "px");
  }

  deleteContent() {
  }
}
