// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class CodeView extends View {
  constructor(divID, PR, sourceText, sourcePosition, broker) {
    super(divID, broker, null, false);
    let view = this;
    view.PR = PR;
    view.mouseDown = false;
    view.broker = broker;
    view.allSpans = [];

    var selectionHandler = {
      clear: function() { broker.clear(selectionHandler); },
      select: function(items, selected) {
        var handler = this;
        var broker = view.broker;
        for (let span of items) {
          if (selected) {
            span.classList.add("selected");
          } else {
            span.classList.remove("selected");
          }
        }
        var locations = [];
        for (var span of items) {
          locations.push({pos_start: span.start, pos_end: span.end});
        }
        broker.clear(selectionHandler);
        broker.select(selectionHandler, locations, selected);
      },
      selectionDifference: function(span1, inclusive1, span2, inclusive2) {
        var pos1 = span1.start;
        var pos2 = span2.start;
        var result = [];
        var lineListDiv = view.divNode.firstChild.firstChild.childNodes;
        for (var i = 0; i < lineListDiv.length; i++) {
          var currentLineElement = lineListDiv[i];
          var spans = currentLineElement.childNodes;
          for (var j = 0; j < spans.length; ++j) {
            var currentSpan = spans[j];
            if (currentSpan.start > pos1 ||
                (inclusive1 && currentSpan.start == pos1)) {
              if (currentSpan.start < pos2 ||
                  (inclusive2 && currentSpan.start == pos2)) {
                result.push(currentSpan);
              }
            }
          }
        }
        return result;
      },
      brokeredSelect: function(locations, selected) {
        let firstSelect = view.selection.isEmpty();
        for (let location of locations) {
          let start = location.pos_start;
          let end = location.pos_end;
          if (start && end) {
            let lower = 0;
            let upper = view.allSpans.length;
            if (upper > 0) {
              while ((upper - lower) > 1) {
                var middle = Math.floor((upper + lower) / 2);
                var lineStart = view.allSpans[middle].start;
                if (lineStart < start) {
                  lower = middle;
                } else if (lineStart > start) {
                  upper = middle;
                } else {
                  lower = middle;
                  break;
                }
              }
              var currentSpan = view.allSpans[lower];
              var currentLineElement = currentSpan.parentNode;
              if ((currentSpan.start <= start && start < currentSpan.end) ||
                  (currentSpan.start <= end && end < currentSpan.end)) {
                if (firstSelect) {
                  makeContainerPosVisible(
                      view.divNode, currentLineElement.offsetTop);
                  firstSelect = false;
                }
                view.selection.select(currentSpan, selected);
              }
            }
          }
        }
      },
      brokeredClear: function() { view.selection.clear(); },
    };
    view.selection = new Selection(selectionHandler);
    broker.addSelectionHandler(selectionHandler);

    view.handleSpanMouseDown = function(e) {
      e.stopPropagation();
      if (!e.shiftKey) {
        view.selection.clear();
      }
      view.selection.select(this, true);
      view.mouseDown = true;
    }

    view.handleSpanMouseMove = function(e) {
      if (view.mouseDown) {
        view.selection.extendTo(this);
      }
    }

    view.handleCodeMouseDown = function(e) { view.selection.clear(); }

    document.addEventListener('mouseup', function(e) {
      view.mouseDown = false;
    }, false);

    view.initializeCode(sourceText, sourcePosition);
  }

  initializeContent(data, rememberedSelection) { this.data = data; }

  initializeCode(sourceText, sourcePosition) {
    var view = this;
    var codePre = document.createElement("pre");
    codePre.classList.add("prettyprint");
    view.divNode.innerHTML = "";
    view.divNode.appendChild(codePre);
    if (sourceText != "") {
      codePre.classList.add("linenums");
      codePre.textContent = sourceText;
      try {
        // Wrap in try to work when offline.
        view.PR.prettyPrint();
      } catch (e) {
      }

      view.divNode.onmousedown = this.handleCodeMouseDown;

      var base = sourcePosition;
      var current = 0;
      var lineListDiv = view.divNode.firstChild.firstChild.childNodes;
      for (let i = 0; i < lineListDiv.length; i++) {
        var currentLineElement = lineListDiv[i];
        currentLineElement.id = "li" + i;
        var pos = base + current;
        currentLineElement.pos = pos;
        var spans = currentLineElement.childNodes;
        for (let j = 0; j < spans.length; ++j) {
          var currentSpan = spans[j];
          if (currentSpan.nodeType == 1) {
            currentSpan.start = pos;
            currentSpan.end = pos + currentSpan.textContent.length;
            currentSpan.onmousedown = this.handleSpanMouseDown;
            currentSpan.onmousemove = this.handleSpanMouseMove;
            view.allSpans.push(currentSpan);
          }
          current += currentSpan.textContent.length;
          pos = base + current;
        }
        while ((current < sourceText.length) &&
               (sourceText[current] == '\n' || sourceText[current] == '\r')) {
          ++current;
        }
      }
    }

    view.resizeToParent();
  }

  deleteContent() {}
}
