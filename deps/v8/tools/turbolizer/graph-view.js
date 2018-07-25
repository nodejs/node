// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function nodeToStringKey(n) {
  return "" + n.id;
}

class GraphView extends View {
  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "graph");
    return pane;
  }

  constructor(d3, id, broker, showPhaseByName) {
    super(id, broker);
    var graph = this;
    this.showPhaseByName = showPhaseByName

    var svg = this.divElement.append("svg").attr('version', '1.1')
      .attr("width", "100%")
      .attr("height", "100%");
    svg.on("mousedown", function (d) { graph.svgMouseDown.call(graph, d); });
    svg.on("mouseup", function (d) { graph.svgMouseUp.call(graph, d); });
    graph.svg = svg;

    graph.nodes = [];
    graph.edges = [];

    graph.minGraphX = 0;
    graph.maxGraphX = 1;
    graph.minGraphY = 0;
    graph.maxGraphY = 1;

    graph.state = {
      selection: null,
      mouseDownNode: null,
      justDragged: false,
      justScaleTransGraph: false,
      lastKeyDown: -1,
      showTypes: false,
      hideDead: false
    };

    this.selectionHandler = {
      clear: function () {
        graph.state.selection.clear();
        broker.broadcastClear(this);
        graph.updateGraphVisibility();
      },
      select: function (nodes, selected) {
        let locations = [];
        for (const node of nodes) {
          if (node.sourcePosition) {
            locations.push(node.sourcePosition);
          }
        }
        graph.state.selection.select(nodes, selected);
        broker.broadcastSourcePositionSelect(this, locations, selected);
        graph.updateGraphVisibility();
      },
      brokeredNodeSelect: function (locations, selected) {
        let selection = graph.nodes
          .filter(function (n) {
            return locations.has(nodeToStringKey(n))
              && (!graph.state.hideDead || n.isLive());
          });
        graph.state.selection.select(selection, selected);
        // Update edge visibility based on selection.
        graph.nodes.forEach((n) => {
          if (graph.state.selection.isSelected(n)) n.visible = true;
        });
        graph.edges.forEach(function (e) {
          e.visible = e.visible ||
            (graph.state.selection.isSelected(e.source) && graph.state.selection.isSelected(e.target));
        });
        graph.updateGraphVisibility();
      },
      brokeredClear: function () {
        graph.state.selection.clear();
        graph.updateGraphVisibility();
      }
    };
    broker.addNodeHandler(this.selectionHandler);

    graph.state.selection = new Selection(nodeToStringKey);

    var defs = svg.append('svg:defs');
    defs.append('svg:marker')
      .attr('id', 'end-arrow')
      .attr('viewBox', '0 -4 8 8')
      .attr('refX', 2)
      .attr('markerWidth', 2.5)
      .attr('markerHeight', 2.5)
      .attr('orient', 'auto')
      .append('svg:path')
      .attr('d', 'M0,-4L8,0L0,4');

    this.graphElement = svg.append("g");
    graph.visibleEdges = this.graphElement.append("g");
    graph.visibleNodes = this.graphElement.append("g");

    graph.drag = d3.behavior.drag()
      .origin(function (d) {
        return { x: d.x, y: d.y };
      })
      .on("drag", function (args) {
        graph.state.justDragged = true;
        graph.dragmove.call(graph, args);
      })

    d3.select("#layout").on("click", partial(this.layoutAction, graph));
    d3.select("#show-all").on("click", partial(this.showAllAction, graph));
    d3.select("#toggle-hide-dead").on("click", partial(this.toggleHideDead, graph));
    d3.select("#hide-unselected").on("click", partial(this.hideUnselectedAction, graph));
    d3.select("#hide-selected").on("click", partial(this.hideSelectedAction, graph));
    d3.select("#zoom-selection").on("click", partial(this.zoomSelectionAction, graph));
    d3.select("#toggle-types").on("click", partial(this.toggleTypesAction, graph));

    // listen for key events
    d3.select(window).on("keydown", function (e) {
      graph.svgKeyDown.call(graph);
    })
      .on("keyup", function () {
        graph.svgKeyUp.call(graph);
      });

    graph.dragSvg = d3.behavior.zoom()
      .on("zoom", function () {
        if (d3.event.sourceEvent.shiftKey) {
          return false;
        } else {
          graph.zoomed.call(graph);
        }
        return true;
      })
      .on("zoomstart", function () {
        if (!d3.event.sourceEvent.shiftKey) d3.select('body').style("cursor", "move");
      })
      .on("zoomend", function () {
        d3.select('body').style("cursor", "auto");
      });

    svg.call(graph.dragSvg).on("dblclick.zoom", null);
  }

  static get selectedClass() {
    return "selected";
  }
  static get rectClass() {
    return "nodeStyle";
  }
  static get activeEditId() {
    return "active-editing";
  }
  static get nodeRadius() {
    return 50;
  }

  getNodeHeight(d) {
    if (this.state.showTypes) {
      return d.normalheight + d.labelbbox.height;
    } else {
      return d.normalheight;
    }
  }

  getEdgeFrontier(nodes, inEdges, edgeFilter) {
    let frontier = new Set();
    for (const n of nodes) {
      var edges = inEdges ? n.inputs : n.outputs;
      var edgeNumber = 0;
      edges.forEach(function (edge) {
        if (edgeFilter == undefined || edgeFilter(edge, edgeNumber)) {
          frontier.add(edge);
        }
        ++edgeNumber;
      });
    }
    return frontier;
  }

  getNodeFrontier(nodes, inEdges, edgeFilter) {
    let graph = this;
    var frontier = new Set();
    var newState = true;
    var edgeFrontier = graph.getEdgeFrontier(nodes, inEdges, edgeFilter);
    // Control key toggles edges rather than just turning them on
    if (d3.event.ctrlKey) {
      edgeFrontier.forEach(function (edge) {
        if (edge.visible) {
          newState = false;
        }
      });
    }
    edgeFrontier.forEach(function (edge) {
      edge.visible = newState;
      if (newState) {
        var node = inEdges ? edge.source : edge.target;
        node.visible = true;
        frontier.add(node);
      }
    });
    graph.updateGraphVisibility();
    if (newState) {
      return frontier;
    } else {
      return undefined;
    }
  }

  dragmove(d) {
    var graph = this;
    d.x += d3.event.dx;
    d.y += d3.event.dy;
    graph.updateGraphVisibility();
  }

  initializeContent(data, rememberedSelection) {
    this.createGraph(data, rememberedSelection);
    if (rememberedSelection != null) {
      this.attachSelection(rememberedSelection);
      this.connectVisibleSelectedNodes();
      this.viewSelection();
    }
    this.updateGraphVisibility();
    this.fitGraphViewToWindow();
  }

  deleteContent() {
    if (this.visibleNodes) {
      this.nodes = [];
      this.edges = [];
      this.nodeMap = [];
      this.updateGraphVisibility();
    }
  };

  measureText(text) {
    var textMeasure = document.getElementById('text-measure');
    textMeasure.textContent = text;
    return {
      width: textMeasure.getBBox().width,
      height: textMeasure.getBBox().height,
    };
  }

  createGraph(data, rememberedSelection) {
    var g = this;
    g.nodes = data.nodes;
    g.nodeMap = [];
    g.nodes.forEach(function (n, i) {
      n.__proto__ = Node;
      n.visible = false;
      n.x = 0;
      n.y = 0;
      if (typeof n.pos === "number") {
        // Backwards compatibility.
        n.sourcePosition = { scriptOffset: n.pos, inliningId: -1 };
      }
      n.rank = MAX_RANK_SENTINEL;
      n.inputs = [];
      n.outputs = [];
      n.rpo = -1;
      n.outputApproach = MINIMUM_NODE_OUTPUT_APPROACH;
      n.cfg = n.control;
      g.nodeMap[n.id] = n;
      n.displayLabel = n.getDisplayLabel();
      n.labelbbox = g.measureText(n.displayLabel);
      n.typebbox = g.measureText(n.getDisplayType());
      var innerwidth = Math.max(n.labelbbox.width, n.typebbox.width);
      n.width = Math.alignUp(innerwidth + NODE_INPUT_WIDTH * 2,
        NODE_INPUT_WIDTH);
      var innerheight = Math.max(n.labelbbox.height, n.typebbox.height);
      n.normalheight = innerheight + 20;
    });
    g.edges = [];
    data.edges.forEach(function (e, i) {
      var t = g.nodeMap[e.target];
      var s = g.nodeMap[e.source];
      var newEdge = new Edge(t, e.index, s, e.type);
      t.inputs.push(newEdge);
      s.outputs.push(newEdge);
      g.edges.push(newEdge);
      if (e.type == 'control') {
        s.cfg = true;
      }
    });
    g.nodes.forEach(function (n, i) {
      n.visible = isNodeInitiallyVisible(n) && (!g.state.hideDead || n.isLive());
      if (rememberedSelection != undefined) {
        if (rememberedSelection.has(nodeToStringKey(n))) {
          n.visible = true;
        }
      }
    });
    g.fitGraphViewToWindow();
    g.updateGraphVisibility();
    g.layoutGraph();
    g.updateGraphVisibility();
    g.viewWholeGraph();
  }

  connectVisibleSelectedNodes() {
    var graph = this;
    for (const n of graph.state.selection) {
      n.inputs.forEach(function (edge) {
        if (edge.source.visible && edge.target.visible) {
          edge.visible = true;
        }
      });
      n.outputs.forEach(function (edge) {
        if (edge.source.visible && edge.target.visible) {
          edge.visible = true;
        }
      });
    }
  }

  updateInputAndOutputBubbles() {
    var g = this;
    var s = g.visibleBubbles;
    s.classed("filledBubbleStyle", function (c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return edge.isVisible();
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 2;
      }
    }).classed("halfFilledBubbleStyle", function (c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return false;
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 1;
      }
    }).classed("bubbleStyle", function (c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return !edge.isVisible();
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 0;
      }
    });
    s.each(function (c) {
      var components = this.id.split(',');
      if (components[0] == "ob") {
        var from = g.nodeMap[components[1]];
        var x = from.getOutputX();
        var y = g.getNodeHeight(from) + DEFAULT_NODE_BUBBLE_RADIUS;
        var transform = "translate(" + x + "," + y + ")";
        this.setAttribute('transform', transform);
      }
    });
  }

  attachSelection(s) {
    const graph = this;
    if (!(s instanceof Set)) return;
    graph.selectionHandler.clear();
    const selected = graph.nodes.filter((n) =>
      s.has(graph.state.selection.stringKey(n)) && (!graph.state.hideDead || n.isLive()));
    graph.selectionHandler.select(selected, true);
  }

  detachSelection() {
    return this.state.selection.detachSelection();
  }

  pathMouseUp(path, d) {
    d3.event.stopPropagation();
    const edge = path.datum();
    if (!d3.event.shiftKey) {
      this.selectionHandler.clear();
    }
    this.selectionHandler.select([edge.source, edge.target], true);
    return false;
  };

  nodeMouseDown(node, d) {
    d3.event.stopPropagation();
    this.state.mouseDownNode = d;
  }

  nodeMouseUp(d3node, d) {
    let graph = this;
    let state = graph.state;

    if (!state.mouseDownNode) return;

    if (state.justDragged) {
      // dragged, not clicked
      redetermineGraphBoundingBox(graph);
      state.justDragged = false;
    } else {
      // clicked, not dragged
      var extend = d3.event.shiftKey;
      if (!extend) {
        graph.selectionHandler.clear();
      }
      graph.selectionHandler.select([d3node.datum()], undefined);
    }
  }

  selectAllNodes(inEdges, filter) {
    var graph = this;
    if (!d3.event.shiftKey) {
      graph.state.selection.clear();
    }
    const allVisibleNodes = graph.nodes.filter((n) => n.visible);
    graph.state.selection.select(allVisibleNodes, true);
    graph.updateGraphVisibility();
  }

  layoutAction(graph) {
    graph.updateGraphVisibility();
    graph.layoutGraph();
    graph.updateGraphVisibility();
    graph.viewWholeGraph();
  }

  showAllAction(graph) {
    graph.nodes.forEach(function (n) {
      n.visible = !graph.state.hideDead || n.isLive();
    });
    graph.edges.forEach(function (e) {
      e.visible = !graph.state.hideDead || (e.source.isLive() && e.target.isLive());
    });
    graph.updateGraphVisibility();
    graph.viewWholeGraph();
  }

  toggleHideDead(graph) {
    graph.state.hideDead = !graph.state.hideDead;
    if (graph.state.hideDead) graph.hideDead();
    var element = document.getElementById('toggle-hide-dead');
    element.classList.toggle('button-input-toggled', graph.state.hideDead);
  }

  hideDead() {
    const graph = this;
    graph.nodes.filter(function (n) {
      if (!n.isLive()) {
        n.visible = false;
        graph.state.selection.select([n], false);
      }
    })
    graph.updateGraphVisibility();
  }

  hideUnselectedAction(graph) {
    graph.nodes.forEach(function (n) {
      if (!graph.state.selection.isSelected(n)) {
        n.visible = false;
      }
    });
    graph.updateGraphVisibility();
  }

  hideSelectedAction(graph) {
    graph.nodes.forEach(function (n) {
      if (graph.state.selection.isSelected(n)) {
        n.visible = false;
      }
    });
    graph.selectionHandler.clear();
  }

  zoomSelectionAction(graph) {
    graph.viewSelection();
  }

  toggleTypesAction(graph) {
    graph.toggleTypes();
  }

  searchInputAction(graph, searchBar) {
    if (d3.event.keyCode == 13) {
      graph.selectionHandler.clear();
      var query = searchBar.value;
      window.sessionStorage.setItem("lastSearch", query);
      if (query.length == 0) return;

      var reg = new RegExp(query);
      var filterFunction = function (n) {
        return (reg.exec(n.getDisplayLabel()) != null ||
          (graph.state.showTypes && reg.exec(n.getDisplayType())) ||
          (reg.exec(n.getTitle())) ||
          reg.exec(n.opcode) != null);
      };

      const selection = graph.nodes.filter(
        function (n, i) {
          if ((d3.event.ctrlKey || n.visible) && filterFunction(n)) {
            if (d3.event.ctrlKey) n.visible = true;
            return true;
          }
          return false;
        });

      graph.selectionHandler.select(selection, true);
      graph.connectVisibleSelectedNodes();
      graph.updateGraphVisibility();
      searchBar.blur();
      graph.viewSelection();
    }
    d3.event.stopPropagation();
  }

  svgMouseDown() {
    this.state.graphMouseDown = true;
  }

  svgMouseUp() {
    const graph = this;
    const state = graph.state;
    if (state.justScaleTransGraph) {
      // Dragged
      state.justScaleTransGraph = false;
    } else {
      // Clicked
      if (state.mouseDownNode == null && !d3.event.shiftKey) {
        graph.selectionHandler.clear();
      }
    }
    state.mouseDownNode = null;
    state.graphMouseDown = false;
  }

  svgKeyDown() {
    var state = this.state;
    var graph = this;

    // Don't handle key press repetition
    if (state.lastKeyDown !== -1) return;

    var showSelectionFrontierNodes = function (inEdges, filter, select) {
      var frontier = graph.getNodeFrontier(state.selection, inEdges, filter);
      if (frontier != undefined && frontier.size) {
        if (select) {
          if (!d3.event.shiftKey) {
            state.selection.clear();
          }
          state.selection.select(frontier, true);
        }
        graph.updateGraphVisibility();
      }
      allowRepetition = false;
    }

    var allowRepetition = true;
    var eventHandled = true; // unless the below switch defaults
    switch (d3.event.keyCode) {
      case 49:
      case 50:
      case 51:
      case 52:
      case 53:
      case 54:
      case 55:
      case 56:
      case 57:
        // '1'-'9'
        showSelectionFrontierNodes(true,
          (edge, index) => { return index == (d3.event.keyCode - 49); },
          false);
        break;
      case 97:
      case 98:
      case 99:
      case 100:
      case 101:
      case 102:
      case 103:
      case 104:
      case 105:
        // 'numpad 1'-'numpad 9'
        showSelectionFrontierNodes(true,
          (edge, index) => { return index == (d3.event.keyCode - 97); },
          false);
        break;
      case 67:
        // 'c'
        showSelectionFrontierNodes(d3.event.altKey,
          (edge, index) => { return edge.type == 'control'; },
          true);
        break;
      case 69:
        // 'e'
        showSelectionFrontierNodes(d3.event.altKey,
            (edge, index) => { return edge.type == 'effect'; },
            true);
        break;
      case 79:
        // 'o'
        showSelectionFrontierNodes(false, undefined, false);
        break;
      case 73:
        // 'i'
        showSelectionFrontierNodes(true, undefined, false);
        break;
      case 65:
        // 'a'
        graph.selectAllNodes();
        allowRepetition = false;
        break;
      case 38:
      case 40: {
        showSelectionFrontierNodes(d3.event.keyCode == 38, undefined, true);
        break;
      }
      case 82:
        // 'r'
        if (!d3.event.ctrlKey) {
          this.layoutAction(this);
        } else {
          eventHandled = false;
        }
        break;
      case 83:
        // 's'
        graph.selectOrigins();
        break;
      case 191:
        // '/'
        document.getElementById("search-input").focus();
        document.getElementById("search-input").select();
        break;
      default:
        eventHandled = false;
        break;
    }
    if (eventHandled) {
      d3.event.preventDefault();
    }
    if (!allowRepetition) {
      state.lastKeyDown = d3.event.keyCode;
    }
  }

  svgKeyUp() {
    this.state.lastKeyDown = -1
  };

  layoutGraph() {
    layoutNodeGraph(this);
  }

  selectOrigins() {
    const state = this.state;
    const origins = [];
    let phase = null;
    for (const n of state.selection) {
      if (n.origin) {
        const node = this.nodeMap[n.origin.nodeId];
        origins.push(node);
        phase = n.origin.phase;
      }
    }
    if (origins.length) {
      state.selection.clear();
      state.selection.select(origins, true);
      if (phase) {
        this.showPhaseByName(phase);
      }
    }
  }

  // call to propagate changes to graph
  updateGraphVisibility() {
    let graph = this;
    let state = graph.state;

    var filteredEdges = graph.edges.filter(function (e) {
      return e.isVisible();
    });
    const selEdges = graph.visibleEdges.selectAll("path").data(filteredEdges, function (edge) {
      return edge.stringID();
    });

    // remove old links
    selEdges.exit().remove();

    // add new paths
    selEdges.enter()
      .append('path')
      .style('marker-end', 'url(#end-arrow)')
      .classed('hidden', function (e) {
        return !e.isVisible();
      })
      .attr("id", function (edge) { return "e," + edge.stringID(); })
      .on("mouseup", function (d) {
        graph.pathMouseUp.call(graph, d3.select(this), d);
      })
      .attr("adjacentToHover", "false");

    // Set the correct styles on all of the paths
    selEdges.classed('value', function (e) {
      return e.type == 'value' || e.type == 'context';
    }).classed('control', function (e) {
      return e.type == 'control';
    }).classed('effect', function (e) {
      return e.type == 'effect';
    }).classed('frame-state', function (e) {
      return e.type == 'frame-state';
    }).attr('stroke-dasharray', function (e) {
      if (e.type == 'frame-state') return "10,10";
      return (e.type == 'effect') ? "5,5" : "";
    });

    // select existing nodes
    var filteredNodes = graph.nodes.filter(function (n) {
      return n.visible;
    });
    let selNodes = graph.visibleNodes.selectAll("g").data(filteredNodes, function (d) {
      return d.id;
    });

    // remove old nodes
    selNodes.exit().remove();

    // add new nodes
    var newGs = selNodes.enter()
      .append("g");

    newGs.classed("turbonode", function (n) { return true; })
      .classed("control", function (n) { return n.isControl(); })
      .classed("live", function (n) { return n.isLive(); })
      .classed("dead", function (n) { return !n.isLive(); })
      .classed("javascript", function (n) { return n.isJavaScript(); })
      .classed("input", function (n) { return n.isInput(); })
      .classed("simplified", function (n) { return n.isSimplified(); })
      .classed("machine", function (n) { return n.isMachine(); })
      .on("mousedown", function (d) {
        graph.nodeMouseDown.call(graph, d3.select(this), d);
      })
      .on("mouseup", function (d) {
        graph.nodeMouseUp.call(graph, d3.select(this), d);
      })
      .on('mouseover', function (d) {
        var nodeSelection = d3.select(this);
        let node = graph.nodeMap[d.id];
        let visibleEdges = graph.visibleEdges.selectAll('path');
        let adjInputEdges = visibleEdges.filter(e => { return e.target === node; });
        let adjOutputEdges = visibleEdges.filter(e => { return e.source === node; });
        adjInputEdges.attr('relToHover', "input");
        adjOutputEdges.attr('relToHover', "output");
        let adjInputNodes = adjInputEdges.data().map(e => e.source);
        let visibleNodes = graph.visibleNodes.selectAll("g");
        visibleNodes.data(adjInputNodes, function (d) {
          return d.id;
        }).attr('relToHover', "input");
        let adjOutputNodes = adjOutputEdges.data().map(e => e.target);
        visibleNodes.data(adjOutputNodes, function (d) {
          return d.id;
        }).attr('relToHover', "output");
        graph.updateGraphVisibility();
      })
      .on('mouseout', function (d) {
        var nodeSelection = d3.select(this);
        let node = graph.nodeMap[d.id];
        let visibleEdges = graph.visibleEdges.selectAll('path');
        let adjEdges = visibleEdges.filter(e => { return e.target === node || e.source === node; });
        adjEdges.attr('relToHover', "none");
        let adjNodes = adjEdges.data().map(e => e.target).concat(adjEdges.data().map(e => e.source));
        let visibleNodes = graph.visibleNodes.selectAll("g");
        let nodes = visibleNodes.data(adjNodes, function (d) {
          return d.id;
        }).attr('relToHover', "none");
        graph.updateGraphVisibility();
      })
      .call(graph.drag)

    newGs.append("rect")
      .attr("rx", 10)
      .attr("ry", 10)
      .attr(WIDTH, function (d) {
        return d.getTotalNodeWidth();
      })
      .attr(HEIGHT, function (d) {
        return graph.getNodeHeight(d);
      })

    function appendInputAndOutputBubbles(g, d) {
      for (var i = 0; i < d.inputs.length; ++i) {
        var x = d.getInputX(i);
        var y = -DEFAULT_NODE_BUBBLE_RADIUS;
        var s = g.append('circle')
          .classed("filledBubbleStyle", function (c) {
            return d.inputs[i].isVisible();
          })
          .classed("bubbleStyle", function (c) {
            return !d.inputs[i].isVisible();
          })
          .attr("id", "ib," + d.inputs[i].stringID())
          .attr("r", DEFAULT_NODE_BUBBLE_RADIUS)
          .attr("transform", function (d) {
            return "translate(" + x + "," + y + ")";
          })
          .on("mousedown", function (d) {
            var components = this.id.split(',');
            var node = graph.nodeMap[components[3]];
            var edge = node.inputs[components[2]];
            var visible = !edge.isVisible();
            node.setInputVisibility(components[2], visible);
            d3.event.stopPropagation();
            graph.updateGraphVisibility();
          });
      }
      if (d.outputs.length != 0) {
        var x = d.getOutputX();
        var y = graph.getNodeHeight(d) + DEFAULT_NODE_BUBBLE_RADIUS;
        var s = g.append('circle')
          .classed("filledBubbleStyle", function (c) {
            return d.areAnyOutputsVisible() == 2;
          })
          .classed("halFilledBubbleStyle", function (c) {
            return d.areAnyOutputsVisible() == 1;
          })
          .classed("bubbleStyle", function (c) {
            return d.areAnyOutputsVisible() == 0;
          })
          .attr("id", "ob," + d.id)
          .attr("r", DEFAULT_NODE_BUBBLE_RADIUS)
          .attr("transform", function (d) {
            return "translate(" + x + "," + y + ")";
          })
          .on("mousedown", function (d) {
            d.setOutputVisibility(d.areAnyOutputsVisible() == 0);
            d3.event.stopPropagation();
            graph.updateGraphVisibility();
          });
      }
    }

    newGs.each(function (d) {
      appendInputAndOutputBubbles(d3.select(this), d);
    });

    newGs.each(function (d) {
      d3.select(this).append("text")
        .classed("label", true)
        .attr("text-anchor", "right")
        .attr("dx", 5)
        .attr("dy", 5)
        .append('tspan')
        .text(function (l) {
          return d.getDisplayLabel();
        })
        .append("title")
        .text(function (l) {
          return d.getTitle();
        })
      if (d.type != undefined) {
        d3.select(this).append("text")
          .classed("label", true)
          .classed("type", true)
          .attr("text-anchor", "right")
          .attr("dx", 5)
          .attr("dy", d.labelbbox.height + 5)
          .append('tspan')
          .text(function (l) {
            return d.getDisplayType();
          })
          .append("title")
          .text(function (l) {
            return d.getType();
          })
      }
    });

    selNodes.select('.type').each(function (d) {
      this.setAttribute('visibility', graph.state.showTypes ? 'visible' : 'hidden');
    });

    selNodes
      .classed("selected", function (n) {
        if (state.selection.isSelected(n)) return true;
        return false;
      })
      .attr("transform", function (d) { return "translate(" + d.x + "," + d.y + ")"; })
      .select('rect')
      .attr(HEIGHT, function (d) { return graph.getNodeHeight(d); });

    graph.visibleBubbles = d3.selectAll('circle');

    graph.updateInputAndOutputBubbles();

    graph.maxGraphX = graph.maxGraphNodeX;
    selEdges.attr("d", function (edge) {
      return edge.generatePath(graph);
    });

    graph.svg.style.height = '100%';
    redetermineGraphBoundingBox(this);
  }

  getVisibleTranslation(translate, scale) {
    var graph = this;
    var height = (graph.maxGraphY - graph.minGraphY + 2 * GRAPH_MARGIN) * scale;
    var width = (graph.maxGraphX - graph.minGraphX + 2 * GRAPH_MARGIN) * scale;

    var dimensions = this.getSvgViewDimensions();

    var baseY = translate[1];
    var minY = (graph.minGraphY - GRAPH_MARGIN) * scale;
    var maxY = (graph.maxGraphY + GRAPH_MARGIN) * scale;

    var adjustY = 0;
    var adjustYCandidate = 0;
    if ((maxY + baseY) < dimensions[1]) {
      adjustYCandidate = dimensions[1] - (maxY + baseY);
      if ((minY + baseY + adjustYCandidate) > 0) {
        adjustY = (dimensions[1] / 2) - (maxY - (height / 2)) - baseY;
      } else {
        adjustY = adjustYCandidate;
      }
    } else if (-baseY < minY) {
      adjustYCandidate = -(baseY + minY);
      if ((maxY + baseY + adjustYCandidate) < dimensions[1]) {
        adjustY = (dimensions[1] / 2) - (maxY - (height / 2)) - baseY;
      } else {
        adjustY = adjustYCandidate;
      }
    }
    translate[1] += adjustY;

    var baseX = translate[0];
    var minX = (graph.minGraphX - GRAPH_MARGIN) * scale;
    var maxX = (graph.maxGraphX + GRAPH_MARGIN) * scale;

    var adjustX = 0;
    var adjustXCandidate = 0;
    if ((maxX + baseX) < dimensions[0]) {
      adjustXCandidate = dimensions[0] - (maxX + baseX);
      if ((minX + baseX + adjustXCandidate) > 0) {
        adjustX = (dimensions[0] / 2) - (maxX - (width / 2)) - baseX;
      } else {
        adjustX = adjustXCandidate;
      }
    } else if (-baseX < minX) {
      adjustXCandidate = -(baseX + minX);
      if ((maxX + baseX + adjustXCandidate) < dimensions[0]) {
        adjustX = (dimensions[0] / 2) - (maxX - (width / 2)) - baseX;
      } else {
        adjustX = adjustXCandidate;
      }
    }
    translate[0] += adjustX;
    return translate;
  }

  translateClipped(translate, scale, transition) {
    var graph = this;
    var graphNode = this.graphElement.node();
    var translate = this.getVisibleTranslation(translate, scale);
    if (transition) {
      graphNode.classList.add('visible-transition');
      clearTimeout(graph.transitionTimout);
      graph.transitionTimout = setTimeout(function () {
        graphNode.classList.remove('visible-transition');
      }, 1000);
    }
    var translateString = "translate(" + translate[0] + "px," + translate[1] + "px) scale(" + scale + ")";
    graphNode.style.transform = translateString;
    graph.dragSvg.translate(translate);
    graph.dragSvg.scale(scale);
  }

  zoomed() {
    this.state.justScaleTransGraph = true;
    var scale = this.dragSvg.scale();
    this.translateClipped(d3.event.translate, scale);
  }


  getSvgViewDimensions() {
    var canvasWidth = this.container.clientWidth;
    var documentElement = document.documentElement;
    var canvasHeight = documentElement.clientHeight;
    return [canvasWidth, canvasHeight];
  }


  minScale() {
    var graph = this;
    var dimensions = this.getSvgViewDimensions();
    var width = graph.maxGraphX - graph.minGraphX;
    var height = graph.maxGraphY - graph.minGraphY;
    var minScale = dimensions[0] / (width + GRAPH_MARGIN * 2);
    var minScaleYCandidate = dimensions[1] / (height + GRAPH_MARGIN * 2);
    if (minScaleYCandidate < minScale) {
      minScale = minScaleYCandidate;
    }
    this.dragSvg.scaleExtent([minScale, 1.5]);
    return minScale;
  }

  fitGraphViewToWindow() {
    this.translateClipped(this.dragSvg.translate(), this.dragSvg.scale());
  }

  toggleTypes() {
    var graph = this;
    graph.state.showTypes = !graph.state.showTypes;
    var element = document.getElementById('toggle-types');
    element.classList.toggle('button-input-toggled', graph.state.showTypes);
    graph.updateGraphVisibility();
  }

  viewSelection() {
    var graph = this;
    var minX, maxX, minY, maxY;
    var hasSelection = false;
    graph.visibleNodes.selectAll("g").each(function (n) {
      if (graph.state.selection.isSelected(n)) {
        hasSelection = true;
        minX = minX ? Math.min(minX, n.x) : n.x;
        maxX = maxX ? Math.max(maxX, n.x + n.getTotalNodeWidth()) :
          n.x + n.getTotalNodeWidth();
        minY = minY ? Math.min(minY, n.y) : n.y;
        maxY = maxY ? Math.max(maxY, n.y + graph.getNodeHeight(n)) :
          n.y + graph.getNodeHeight(n);
      }
    });
    if (hasSelection) {
      graph.viewGraphRegion(minX - NODE_INPUT_WIDTH, minY - 60,
        maxX + NODE_INPUT_WIDTH, maxY + 60,
        true);
    }
  }

  viewGraphRegion(minX, minY, maxX, maxY, transition) {
    var graph = this;
    var dimensions = this.getSvgViewDimensions();
    var width = maxX - minX;
    var height = maxY - minY;
    var scale = Math.min(dimensions[0] / width, dimensions[1] / height);
    scale = Math.min(1.5, scale);
    scale = Math.max(graph.minScale(), scale);
    var translation = [-minX * scale, -minY * scale];
    translation = graph.getVisibleTranslation(translation, scale);
    graph.translateClipped(translation, scale, transition);
  }

  viewWholeGraph() {
    var graph = this;
    var minScale = graph.minScale();
    var translation = [0, 0];
    translation = graph.getVisibleTranslation(translation, minScale);
    graph.translateClipped(translation, minScale);
  }
}
