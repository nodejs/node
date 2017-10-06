// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class DisassemblyView extends TextView {
  constructor(id, broker) {
    super(id, broker, null, false);

    let view = this;
    let ADDRESS_STYLE = {
      css: 'tag',
      location: function(text) {
        ADDRESS_STYLE.last_address = text;
        return undefined;
      }
    };
    let ADDRESS_LINK_STYLE = {
      css: 'tag',
      link: function(text) {
        view.select(function(location) { return location.address == text; }, true, true);
      }
    };
    let UNCLASSIFIED_STYLE = {
      css: 'com'
    };
    let NUMBER_STYLE = {
      css: 'lit'
    };
    let COMMENT_STYLE = {
      css: 'com'
    };
    let POSITION_STYLE = {
      css: 'com',
      location: function(text) {
        view.pos_start = Number(text);
      }
    };
    let OPCODE_STYLE = {
      css: 'kwd',
      location: function(text) {
        if (BLOCK_HEADER_STYLE.block_id != undefined) {
          return {
            address: ADDRESS_STYLE.last_address,
            block_id: BLOCK_HEADER_STYLE.block_id
          };
        } else {
          return {
            address: ADDRESS_STYLE.last_address
          };
        }
      }
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
    const SOURCE_POSITION_HEADER_STYLE = {
      css: 'com',
      location: function(text) {
        let matches = /(\d+):(\d+)/.exec(text);
        if (!matches) return undefined;
        let li = Number(matches[1]);
        if (view.pos_lines === null) return undefined;
        let pos = view.pos_lines[li-1] + Number(matches[2]);
        return {
          pos_start: pos,
          pos_end: pos + 1
        };
      },
    };
    view.SOURCE_POSITION_HEADER_REGEX = /^(\s*-- .+:)(\d+:\d+)( --)/;
    let patterns = [
      [
        [/^0x[0-9a-f]{8,16}/, ADDRESS_STYLE, 1],
        [view.SOURCE_POSITION_HEADER_REGEX, SOURCE_POSITION_HEADER_STYLE, -1],
        [/^\s+-- B\d+ start.*/, BLOCK_HEADER_STYLE, -1],
        [/^.*/, UNCLASSIFIED_STYLE, -1]
      ],
      [
        [/^\s+[0-9a-f]+\s+[0-9a-f]+\s+/, NUMBER_STYLE, 2],
        [/^.*/, null, -1]
      ],
      [
        [/^\S+\s+/, OPCODE_STYLE, 3],
        [/^\S+$/, OPCODE_STYLE, -1],
        [/^.*/, null, -1]
      ],
      [
        [/^\s+/, null],
        [/^[^\(;]+$/, null, -1],
        [/^[^\(;]+/, null],
        [/^\(/, null, 4],
        [/^;/, COMMENT_STYLE, 5]
      ],
      [
        [/^0x[0-9a-f]{8,16}/, ADDRESS_LINK_STYLE],
        [/^[^\)]/, null],
        [/^\)$/, null, -1],
        [/^\)/, null, 3]
      ],
      [
        [/^; debug\: position /, COMMENT_STYLE, 6],
        [/^.+$/, COMMENT_STYLE, -1]
      ],
      [
        [/^\d+$/, POSITION_STYLE, -1],
      ]
    ];
    view.setPatterns(patterns);
  }

  lineLocation(li) {
    let view = this;
    let result = undefined;
    for (let i = 0; i < li.children.length; ++i) {
      let fragment = li.children[i];
      let location = fragment.location;
      if (location != null) {
        if (location.block_id != undefined) {
          if (result === undefined) result = {};
          result.block_id = location.block_id;
        }
        if (location.address != undefined) {
          if (result === undefined) result = {};
          result.address = location.address;
        }
        if (location.pos_start != undefined && location.pos_end != undefined) {
          if (result === undefined) result = {};
          result.pos_start = location.pos_start;
          result.pos_end = location.pos_end;
        }
        else if (view.pos_start != -1) {
          if (result === undefined) result = {};
          result.pos_start = view.pos_start;
          result.pos_end = result.pos_start + 1;
        }
      }
    }
    return result;
  }

  initializeContent(data, rememberedSelection) {
    this.data = data;
    super.initializeContent(data, rememberedSelection);
  }

  initializeCode(sourceText, sourcePosition) {
    let view = this;
    view.pos_start = -1;
    view.addr_event_counts = null;
    view.total_event_counts = null;
    view.max_event_counts = null;
    view.pos_lines = new Array();
    // Comment lines for line 0 include sourcePosition already, only need to
    // add sourcePosition for lines > 0.
    view.pos_lines[0] = sourcePosition;
    if (sourceText != "") {
      let base = sourcePosition;
      let current = 0;
      let source_lines = sourceText.split("\n");
      for (let i = 1; i < source_lines.length; i++) {
        // Add 1 for newline character that is split off.
        current += source_lines[i-1].length + 1;
        view.pos_lines[i] = base + current;
      }
    }
  }

  initializePerfProfile(eventCounts) {
    let view = this;
    if (eventCounts !== undefined) {
      view.addr_event_counts = eventCounts;

      view.total_event_counts = {};
      view.max_event_counts = {};
      for (let ev_name in view.addr_event_counts) {
        let keys = Object.keys(view.addr_event_counts[ev_name]);
        let values = keys.map(key => view.addr_event_counts[ev_name][key]);
        view.total_event_counts[ev_name] = values.reduce((a, b) => a + b);
        view.max_event_counts[ev_name] = values.reduce((a, b) => Math.max(a, b));
      }
    }
    else {
      view.addr_event_counts = null;
      view.total_event_counts = null;
      view.max_event_counts = null;
    }
  }

  // Shorten decimals and remove trailing zeroes for readability.
  humanize(num) {
    return num.toFixed(3).replace(/\.?0+$/, "") + "%";
  }

  // Interpolate between the given start and end values by a fraction of val/max.
  interpolate(val, max, start, end) {
    return start + (end - start) * (val / max);
  }

  processLine(line) {
    let view = this;
    let func = function(match, p1, p2, p3) {
      let nums = p2.split(":");
      let li = Number(nums[0]);
      let pos = Number(nums[1]);
      if(li === 0)
        pos -= view.pos_lines[0];
      li++;
      return p1 + li + ":" + pos + p3;
    };
    line = line.replace(view.SOURCE_POSITION_HEADER_REGEX, func);
    let fragments = super.processLine(line);

    // Add profiling data per instruction if available.
    if (view.total_event_counts) {
      let matches = /^(0x[0-9a-fA-F]+)\s+\d+\s+[0-9a-fA-F]+/.exec(line);
      if (matches) {
        let newFragments = [];
        for (let event in view.addr_event_counts) {
          let count = view.addr_event_counts[event][matches[1]];
          let str = " ";
          let css_cls = "prof";
          if(count !== undefined) {
            let perc = count / view.total_event_counts[event] * 100;

            let col = { r: 255, g: 255, b: 255 };
            for (let i = 0; i < PROF_COLS.length; i++) {
              if (perc === PROF_COLS[i].perc) {
                col = PROF_COLS[i].col;
                break;
              }
              else if (perc > PROF_COLS[i].perc && perc < PROF_COLS[i + 1].perc) {
                let col1 = PROF_COLS[i].col;
                let col2 = PROF_COLS[i + 1].col;

                let val = perc - PROF_COLS[i].perc;
                let max = PROF_COLS[i + 1].perc - PROF_COLS[i].perc;

                col.r = Math.round(view.interpolate(val, max, col1.r, col2.r));
                col.g = Math.round(view.interpolate(val, max, col1.g, col2.g));
                col.b = Math.round(view.interpolate(val, max, col1.b, col2.b));
                break;
              }
            }

            str = UNICODE_BLOCK;

            let fragment = view.createFragment(str, css_cls);
            fragment.title = event + ": " + view.humanize(perc) + " (" + count + ")";
            fragment.style.color = "rgb(" + col.r + ", " + col.g + ", " + col.b + ")";

            newFragments.push(fragment);
          }
          else
            newFragments.push(view.createFragment(str, css_cls));

        }
        fragments = newFragments.concat(fragments);
      }
    }
    return fragments;
  }
}
