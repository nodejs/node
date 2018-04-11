// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class GraphView extends View {
  constructor (d3, id, nodes, edges, broker) {
    super(id, broker);
    var graph = this;

    var svg = this.divElement.append("svg").attr('version','1.1').attr("width", "100%");
    graph.svg = svg;

    graph.nodes = nodes || [];
    graph.edges = edges || [];

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
      showTypes: false
    };

    var selectionHandler = {
      clear: function() {
        broker.clear(selectionHandler);
      },
      select: function(items, selected) {
        var locations = [];
        for (var d of items) {
          if (selected) {
            d.classList.add("selected");
          } else {
            d.classList.remove("selected");
          }
          var data = d.__data__;
          locations.push({ pos_start: data.pos, pos_end: data.pos + 1, node_id: data.id});
        }
        broker.select(selectionHandler, locations, selected);
      },
      selectionDifference: function(span1, inclusive1, span2, inclusive2) {
        // Should not be called
      },
      brokeredSelect: function(locations, selected) {
        var test = [].entries().next();
        var selection = graph.nodes
          .filter(function(n) {
            var pos = n.pos;
            for (var location of locations) {
              var start = location.pos_start;
              var end = location.pos_end;
              var id = location.node_id;
              if (end != undefined) {
                if (pos >= start && pos < end) {
                  return true;
                }
              } else if (start != undefined) {
                if (pos === start) {
                  return true;
                }
              } else {
                if (n.id === id) {
                  return true;
                }
              }
            }
            return false;
          });
        var newlySelected = new Set();
        selection.forEach(function(n) {
          newlySelected.add(n);
          if (!n.visible) {
            n.visible = true;
          }
        });
        graph.updateGraphVisibility();
        graph.visibleNodes.each(function(n) {
          if (newlySelected.has(n)) {
            graph.state.selection.select(this, selected);
          }
        });
        graph.updateGraphVisibility();
        graph.viewSelection();
      },
      brokeredClear: function() {
        graph.state.selection.clear();
      }
    };
    broker.addSelectionHandler(selectionHandler);

    graph.state.selection = new Selection(selectionHandler);

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
    graph.visibleEdges = this.graphElement.append("g").selectAll("g");
    graph.visibleNodes = this.graphElement.append("g").selectAll("g");

    graph.drag = d3.behavior.drag()
      .origin(function(d){
        return {x: d.x, y: d.y};
      })
      .on("drag", function(args){
        graph.state.justDragged = true;
        graph.dragmove.call(graph, args);
      })

    d3.select("#upload").on("click", partial(this.uploadAction, graph));
    d3.select("#layout").on("click", partial(this.layoutAction, graph));
    d3.select("#show-all").on("click", partial(this.showAllAction, graph));
    d3.select("#hide-dead").on("click", partial(this.hideDeadAction, graph));
    d3.select("#hide-unselected").on("click", partial(this.hideUnselectedAction, graph));
    d3.select("#hide-selected").on("click", partial(this.hideSelectedAction, graph));
    d3.select("#zoom-selection").on("click", partial(this.zoomSelectionAction, graph));
    d3.select("#toggle-types").on("click", partial(this.toggleTypesAction, graph));
    d3.select("#search-input").on("keydown", partial(this.searchInputAction, graph));

    // listen for key events
    d3.select(window).on("keydown", function(e){
      graph.svgKeyDown.call(graph);
    })
      .on("keyup", function(){
        graph.svgKeyUp.call(graph);
      });
    svg.on("mousedown", function(d){graph.svgMouseDown.call(graph, d);});
    svg.on("mouseup", function(d){graph.svgMouseUp.call(graph, d);});

    graph.dragSvg = d3.behavior.zoom()
      .on("zoom", function(){
        if (d3.event.sourceEvent.shiftKey){
          return false;
        } else{
          graph.zoomed.call(graph);
        }
        return true;
      })
      .on("zoomstart", function(){
        if (!d3.event.sourceEvent.shiftKey) d3.select('body').style("cursor", "move");
      })
      .on("zoomend", function(){
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
    nodes.forEach(function(element) {
      var edges = inEdges ? element.__data__.inputs : element.__data__.outputs;
      var edgeNumber = 0;
      edges.forEach(function(edge) {
        if (edgeFilter == undefined || edgeFilter(edge, edgeNumber)) {
          frontier.add(edge);
        }
        ++edgeNumber;
      });
    });
    return frontier;
  }

  getNodeFrontier(nodes, inEdges, edgeFilter) {
    let graph = this;
    var frontier = new Set();
    var newState = true;
    var edgeFrontier = graph.getEdgeFrontier(nodes, inEdges, edgeFilter);
    // Control key toggles edges rather than just turning them on
    if (d3.event.ctrlKey) {
      edgeFrontier.forEach(function(edge) {
        if (edge.visible) {
          newState = false;
        }
      });
    }
    edgeFrontier.forEach(function(edge) {
      edge.visible = newState;
      if (newState) {
        var node = inEdges ? edge.source : edge.target;
        node.visible = true;
        frontier.add(node);
      }
    });
    graph.updateGraphVisibility();
    if (newState) {
      return graph.visibleNodes.filter(function(n) {
        return frontier.has(n);
      });
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

  createGraph(data, initiallyVisibileIds) {
    var g = this;
    g.nodes = data.nodes;
    g.nodeMap = [];
    g.nodes.forEach(function(n, i){
      n.__proto__ = Node;
      n.visible = false;
      n.x = 0;
      n.y = 0;
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
    data.edges.forEach(function(e, i){
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
    g.nodes.forEach(function(n, i) {
      n.visible = isNodeInitiallyVisible(n);
      if (initiallyVisibileIds != undefined) {
        if (initiallyVisibileIds.has(n.id)) {
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
    graph.state.selection.selection.forEach(function(element) {
      var edgeNumber = 0;
      element.__data__.inputs.forEach(function(edge) {
        if (edge.source.visible && edge.target.visible) {
          edge.visible = true;
        }
      });
      element.__data__.outputs.forEach(function(edge) {
        if (edge.source.visible && edge.target.visible) {
          edge.visible = true;
        }
      });
    });
  }

  updateInputAndOutputBubbles() {
    var g = this;
    var s = g.visibleBubbles;
    s.classed("filledBubbleStyle", function(c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return edge.isVisible();
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 2;
      }
    }).classed("halfFilledBubbleStyle", function(c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return false;
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 1;
      }
    }).classed("bubbleStyle", function(c) {
      var components = this.id.split(',');
      if (components[0] == "ib") {
        var edge = g.nodeMap[components[3]].inputs[components[2]];
        return !edge.isVisible();
      } else {
        return g.nodeMap[components[1]].areAnyOutputsVisible() == 0;
      }
    });
    s.each(function(c) {
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
    var graph = this;
    if (s.size != 0) {
      this.visibleNodes.each(function(n) {
        if (s.has(this.__data__.id)) {
          graph.state.selection.select(this, true);
        }
      });
    }
  }

  detachSelection() {
    var selection = this.state.selection.detachSelection();
    var s = new Set();
    for (var i of selection) {
      s.add(i.__data__.id);
    };
    return s;
  }

  pathMouseDown(path, d) {
    d3.event.stopPropagation();
    this.state.selection.clear();
    this.state.selection.add(path);
  };

  nodeMouseDown(node, d) {
    d3.event.stopPropagation();
    this.state.mouseDownNode = d;
  }

  nodeMouseUp(d3node, d) {
    var graph = this,
    state = graph.state,
    consts = graph.consts;

    var mouseDownNode = state.mouseDownNode;

    if (!mouseDownNode) return;

    if (state.justDragged) {
      // dragged, not clicked
      redetermineGraphBoundingBox(graph);
      state.justDragged = false;
    } else{
      // clicked, not dragged
      var extend = d3.event.shiftKey;
      var selection = graph.state.selection;
      if (!extend) {
        selection.clear();
      }
      selection.select(d3node[0][0], true);
    }
  }

  selectSourcePositions(start, end, selected) {
    var graph = this;
    var map = [];
    var sel = graph.nodes.filter(function(n) {
      var pos = (n.pos === undefined)
        ? -1
        : n.getFunctionRelativeSourcePosition(graph);
      if (pos >= start && pos < end) {
        map[n.id] = true;
        n.visible = true;
      }
    });
    graph.updateGraphVisibility();
    graph.visibleNodes.filter(function(n) { return map[n.id]; })
      .each(function(n) {
        var selection = graph.state.selection;
        selection.select(d3.select(this), selected);
      });
  }

  selectAllNodes(inEdges, filter) {
    var graph = this;
    if (!d3.event.shiftKey) {
      graph.state.selection.clear();
    }
    graph.state.selection.select(graph.visibleNodes[0], true);
    graph.updateGraphVisibility();
  }

  uploadAction(graph) {
    document.getElementById("hidden-file-upload").click();
  }

  layoutAction(graph) {
    graph.updateGraphVisibility();
    graph.layoutGraph();
    graph.updateGraphVisibility();
    graph.viewWholeGraph();
  }

  showAllAction(graph) {
    graph.nodes.filter(function(n) { n.visible = true; })
    graph.edges.filter(function(e) { e.visible = true; })
    graph.updateGraphVisibility();
    graph.viewWholeGraph();
  }

  hideDeadAction(graph) {
    graph.nodes.filter(function(n) { if (!n.isLive()) n.visible = false; })
    graph.updateGraphVisibility();
  }

  hideUnselectedAction(graph) {
    var unselected = graph.visibleNodes.filter(function(n) {
      return !this.classList.contains("selected");
    });
    unselected.each(function(n) {
      n.visible = false;
    });
    graph.updateGraphVisibility();
  }

  hideSelectedAction(graph) {
    var selected = graph.visibleNodes.filter(function(n) {
      return this.classList.contains("selected");
    });
    selected.each(function(n) {
      n.visible = false;
    });
    graph.state.selection.clear();
    graph.updateGraphVisibility();
  }

  zoomSelectionAction(graph) {
    graph.viewSelection();
  }

  toggleTypesAction(graph) {
    graph.toggleTypes();
  }

  searchInputAction(graph) {
    if (d3.event.keyCode == 13) {
      graph.state.selection.clear();
      var query = this.value;
      window.sessionStorage.setItem("lastSearch", query);

      var reg = new RegExp(query);
      var filterFunction = function(n) {
        return (reg.exec(n.getDisplayLabel()) != null ||
                (graph.state.showTypes && reg.exec(n.getDisplayType())) ||
                reg.exec(n.opcode) != null);
      };
      if (d3.event.ctrlKey) {
        graph.nodes.forEach(function(n, i) {
          if (filterFunction(n)) {
            n.visible = true;
          }
        });
        graph.updateGraphVisibility();
      }
      var selected = graph.visibleNodes.each(function(n) {
        if (filterFunction(n)) {
          graph.state.selection.select(this, true);
        }
      });
      graph.connectVisibleSelectedNodes();
      graph.updateGraphVisibility();
      this.blur();
      graph.viewSelection();
    }
    d3.event.stopPropagation();
  }

  svgMouseDown() {
    this.state.graphMouseDown = true;
  }

  svgMouseUp() {
    var graph = this,
    state = graph.state;
    if (state.justScaleTransGraph) {
      // Dragged
      state.justScaleTransGraph = false;
    } else {
      // Clicked
      if (state.mouseDownNode == null) {
        graph.state.selection.clear();
      }
    }
    state.mouseDownNode = null;
    state.graphMouseDown = false;
  }

  svgKeyDown() {
    var state = this.state;
    var graph = this;

    // Don't handle key press repetition
    if(state.lastKeyDown !== -1) return;

    var showSelectionFrontierNodes = function(inEdges, filter, select) {
      var frontier = graph.getNodeFrontier(state.selection.selection, inEdges, filter);
      if (frontier != undefined) {
        if (select) {
          if (!d3.event.shiftKey) {
            state.selection.clear();
          }
          state.selection.select(frontier[0], true);
        }
        graph.updateGraphVisibility();
      }
      allowRepetition = false;
    }

    var allowRepetition = true;
    var eventHandled = true; // unless the below switch defaults
    switch(d3.event.keyCode) {
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
      showSelectionFrontierNodes(true,
          (edge, index) => { return edge.type == 'control'; },
          false);
      break;
    case 69:
      // 'e'
      showSelectionFrontierNodes(true,
          (edge, index) => { return edge.type == 'effect'; },
          false);
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

  layoutEdges() {
    var graph = this;
    graph.maxGraphX = graph.maxGraphNodeX;
    this.visibleEdges.attr("d", function(edge){
      return edge.generatePath(graph);
    });
  }

  layoutGraph() {
    layoutNodeGraph(this);
  }

  // call to propagate changes to graph
  updateGraphVisibility() {

    var graph = this,
    state = graph.state;

    var filteredEdges = graph.edges.filter(function(e) { return e.isVisible(); });
    var visibleEdges = graph.visibleEdges.data(filteredEdges, function(edge) {
      return edge.stringID();
    });

    // add new paths
    visibleEdges.enter()
      .append('path')
      .style('marker-end','url(#end-arrow)')
      .classed('hidden', function(e) {
        return !e.isVisible();
      })
      .attr("id", function(edge){ return "e," + edge.stringID(); })
      .on("mousedown", function(d){
        graph.pathMouseDown.call(graph, d3.select(this), d);
      })
      .attr("adjacentToHover", "false");

    // Set the correct styles on all of the paths
    visibleEdges.classed('value', function(e) {
      return e.type == 'value' || e.type == 'context';
    }).classed('control', function(e) {
      return e.type == 'control';
    }).classed('effect', function(e) {
      return e.type == 'effect';
    }).classed('frame-state', function(e) {
      return e.type == 'frame-state';
    }).attr('stroke-dasharray', function(e) {
      if (e.type == 'frame-state') return "10,10";
      return (e.type == 'effect') ? "5,5" : "";
    });

    // remove old links
    visibleEdges.exit().remove();

    graph.visibleEdges = visibleEdges;

    // update existing nodes
    var filteredNodes = graph.nodes.filter(function(n) { return n.visible; });
    graph.visibleNodes = graph.visibleNodes.data(filteredNodes, function(d) {
      return d.id;
    });
    graph.visibleNodes.attr("transform", function(n){
      return "translate(" + n.x + "," + n.y + ")";
    }).select('rect').
      attr(HEIGHT, function(d) { return graph.getNodeHeight(d); });

    // add new nodes
    var newGs = graph.visibleNodes.enter()
      .append("g");

    newGs.classed("turbonode", function(n) { return true; })
      .classed("control", function(n) { return n.isControl(); })
      .classed("live", function(n) { return n.isLive(); })
      .classed("dead", function(n) { return !n.isLive(); })
      .classed("javascript", function(n) { return n.isJavaScript(); })
      .classed("input", function(n) { return n.isInput(); })
      .classed("simplified", function(n) { return n.isSimplified(); })
      .classed("machine", function(n) { return n.isMachine(); })
      .attr("transform", function(d){ return "translate(" + d.x + "," + d.y + ")";})
      .on("mousedown", function(d){
        graph.nodeMouseDown.call(graph, d3.select(this), d);
      })
      .on("mouseup", function(d){
        graph.nodeMouseUp.call(graph, d3.select(this), d);
      })
      .on('mouseover', function(d){
        var nodeSelection = d3.select(this);
        let node = graph.nodeMap[d.id];
        let adjInputEdges = graph.visibleEdges.filter(e => { return e.target === node; });
        let adjOutputEdges = graph.visibleEdges.filter(e => { return e.source === node; });
        adjInputEdges.attr('relToHover', "input");
        adjOutputEdges.attr('relToHover', "output");
        let adjInputNodes = adjInputEdges.data().map(e => e.source);
        graph.visibleNodes.data(adjInputNodes, function(d) {
          return d.id;
        }).attr('relToHover', "input");
        let adjOutputNodes = adjOutputEdges.data().map(e => e.target);
        graph.visibleNodes.data(adjOutputNodes, function(d) {
          return d.id;
        }).attr('relToHover', "output");
        graph.updateGraphVisibility();
      })
      .on('mouseout', function(d){
        var nodeSelection = d3.select(this);
        let node = graph.nodeMap[d.id];
        let adjEdges = graph.visibleEdges.filter(e => { return e.target === node  || e.source === node; });
        adjEdges.attr('relToHover', "none");
        let adjNodes = adjEdges.data().map(e => e.target).concat(adjEdges.data().map(e => e.source));
        let nodes = graph.visibleNodes.data(adjNodes, function(d) {
          return d.id;
        }).attr('relToHover', "none");
        graph.updateGraphVisibility();
      })
      .call(graph.drag);

    newGs.append("rect")
      .attr("rx", 10)
      .attr("ry", 10)
      .attr(WIDTH, function(d) {
        return d.getTotalNodeWidth();
      })
      .attr(HEIGHT, function(d) {
        return graph.getNodeHeight(d);
      })

    function appendInputAndOutputBubbles(g, d) {
      for (var i = 0; i < d.inputs.length; ++i) {
        var x = d.getInputX(i);
        var y = -DEFAULT_NODE_BUBBLE_RADIUS;
        var s = g.append('circle')
          .classed("filledBubbleStyle", function(c) {
            return d.inputs[i].isVisible();
          } )
          .classed("bubbleStyle", function(c) {
            return !d.inputs[i].isVisible();
          } )
          .attr("id", "ib," + d.inputs[i].stringID())
          .attr("r", DEFAULT_NODE_BUBBLE_RADIUS)
          .attr("transform", function(d) {
            return "translate(" + x + "," + y + ")";
          })
          .on("mousedown", function(d){
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
          .classed("filledBubbleStyle", function(c) {
            return d.areAnyOutputsVisible() == 2;
          } )
          .classed("halFilledBubbleStyle", function(c) {
            return d.areAnyOutputsVisible() == 1;
          } )
          .classed("bubbleStyle", function(c) {
            return d.areAnyOutputsVisible() == 0;
          } )
          .attr("id", "ob," + d.id)
          .attr("r", DEFAULT_NODE_BUBBLE_RADIUS)
          .attr("transform", function(d) {
            return "translate(" + x + "," + y + ")";
          })
          .on("mousedown", function(d) {
            d.setOutputVisibility(d.areAnyOutputsVisible() == 0);
            d3.event.stopPropagation();
            graph.updateGraphVisibility();
          });
      }
    }

    newGs.each(function(d){
      appendInputAndOutputBubbles(d3.select(this), d);
    });

    newGs.each(function(d){
      d3.select(this).append("text")
        .classed("label", true)
        .attr("text-anchor","right")
        .attr("dx", 5)
        .attr("dy", 5)
        .append('tspan')
        .text(function(l) {
          return d.getDisplayLabel();
        })
        .append("title")
        .text(function(l) {
          return d.getTitle();
        })
      if (d.type != undefined) {
        d3.select(this).append("text")
          .classed("label", true)
          .classed("type", true)
          .attr("text-anchor","right")
          .attr("dx", 5)
          .attr("dy", d.labelbbox.height + 5)
          .append('tspan')
          .text(function(l) {
            return d.getDisplayType();
          })
          .append("title")
          .text(function(l) {
            return d.getType();
          })
      }
    });

    graph.visibleNodes.select('.type').each(function (d) {
      this.setAttribute('visibility', graph.state.showTypes ? 'visible' : 'hidden');
    });

    // remove old nodes
    graph.visibleNodes.exit().remove();

    graph.visibleBubbles = d3.selectAll('circle');

    graph.updateInputAndOutputBubbles();

    graph.layoutEdges();

    graph.svg.style.height = '100%';
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
    var graphNode = this.graphElement[0][0];
    var translate = this.getVisibleTranslation(translate, scale);
    if (transition) {
      graphNode.classList.add('visible-transition');
      clearTimeout(graph.transitionTimout);
      graph.transitionTimout = setTimeout(function(){
        graphNode.classList.remove('visible-transition');
      }, 1000);
    }
    var translateString = "translate(" + translate[0] + "px," + translate[1] + "px) scale(" + scale + ")";
    graphNode.style.transform = translateString;
    graph.dragSvg.translate(translate);
    graph.dragSvg.scale(scale);
  }

  zoomed(){
    this.state.justScaleTransGraph = true;
    var scale =  this.dragSvg.scale();
    this.translateClipped(d3.event.translate, scale);
  }


  getSvgViewDimensions() {
    var canvasWidth = this.parentNode.clientWidth;
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
    this.svg.attr("height", document.documentElement.clientHeight + "px");
    this.translateClipped(this.dragSvg.translate(), this.dragSvg.scale());
  }

  toggleTypes() {
    var graph = this;
    graph.state.showTypes = !graph.state.showTypes;
    var element = document.getElementById('toggle-types');
    if (graph.state.showTypes) {
      element.classList.add('button-input-toggled');
    } else {
      element.classList.remove('button-input-toggled');
    }
    graph.updateGraphVisibility();
  }

  viewSelection() {
    var graph = this;
    var minX, maxX, minY, maxY;
    var hasSelection = false;
    graph.visibleNodes.each(function(n) {
      if (this.classList.contains("selected")) {
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
    var translation = [-minX*scale, -minY*scale];
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
