// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class ScheduleView extends TextView {
  constructor(id, broker) {
    super(id, broker, null, false);
    let view = this;
    let BLOCK_STYLE = {
      css: 'tag'
    };
    const BLOCK_HEADER_STYLE = {
      css: 'com',
      block_id: -1,
      location: function(text) {
        let matches = /\d+/.exec(text);
        if (!matches) return undefined;
        BLOCK_HEADER_STYLE.block_id = Number(matches[0]);
        return {
          block_id: BLOCK_HEADER_STYLE.block_id
        };
      },
    };
    const BLOCK_LINK_STYLE = {
      css: 'tag',
      link: function(text) {
        let id = Number(text.substr(1));
        view.select(function(location) { return location.block_id == id; }, true, true);
      }
    };
    const ID_STYLE = {
      css: 'tag',
      location: function(text) {
        let matches = /\d+/.exec(text);
        return {
          node_id: Number(matches[0]),
          block_id: BLOCK_HEADER_STYLE.block_id
        };
      },
    };
    const ID_LINK_STYLE = {
      css: 'tag',
      link: function(text) {
        let id = Number(text);
        view.select(function(location) { return location.node_id == id; }, true, true);
      }
    };
    const NODE_STYLE = { css: 'kwd' };
    const GOTO_STYLE = { css: 'kwd',
      goto_id: -2,
      location: function(text) {
        return {
          node_id: GOTO_STYLE.goto_id--,
          block_id: BLOCK_HEADER_STYLE.block_id
        };
      }
    }
    const ARROW_STYLE = { css: 'kwd' };
    let patterns = [
      [
        [/^--- BLOCK B\d+/, BLOCK_HEADER_STYLE, 1],
        [/^\s+\d+: /, ID_STYLE, 2],
        [/^\s+Goto/, GOTO_STYLE, 6],
        [/^.*/, null, -1]
      ],
      [
        [/^ +/, null],
        [/^\(deferred\)/, BLOCK_HEADER_STYLE],
        [/^B\d+/, BLOCK_LINK_STYLE],
        [/^<-/, ARROW_STYLE],
        [/^->/, ARROW_STYLE],
        [/^,/, null],
        [/^---/, BLOCK_HEADER_STYLE, -1]
      ],
      // Parse opcode including []
      [
        [/^[A-Za-z0-9_]+(\[.*\])?$/, NODE_STYLE, -1],
        [/^[A-Za-z0-9_]+(\[(\[.*?\]|.)*?\])?/, NODE_STYLE, 3]
      ],
      // Parse optional parameters
      [
        [/^ /, null, 4],
        [/^\(/, null],
        [/^\d+/, ID_LINK_STYLE],
        [/^, /, null],
        [/^\)$/, null, -1],
        [/^\)/, null, 4],
      ],
      [
        [/^ -> /, ARROW_STYLE, 5],
        [/^.*/, null, -1]
      ],
      [
        [/^B\d+$/, BLOCK_LINK_STYLE, -1],
        [/^B\d+/, BLOCK_LINK_STYLE],
        [/^, /, null]
      ],
      [
        [/^ -> /, ARROW_STYLE],
        [/^B\d+$/, BLOCK_LINK_STYLE, -1]
      ]
    ];
    this.setPatterns(patterns);
  }

  initializeContent(data, rememberedSelection) {
    super.initializeContent(data, rememberedSelection);
    var graph = this;
    var locations = [];
    for (var id of rememberedSelection) {
      locations.push({ node_id : id });
    }
    this.selectLocations(locations, true, true);
  }

  detachSelection() {
    var selection = this.selection.detachSelection();
    var s = new Set();
    for (var i of selection) {
      if (i.location.node_id != undefined && i.location.node_id > 0) {
        s.add(i.location.node_id);
      }
    };
    return s;
  }
}
