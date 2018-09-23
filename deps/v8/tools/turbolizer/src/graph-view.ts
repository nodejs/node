// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as d3 from "d3"
import {layoutNodeGraph} from "./graph-layout.js"
import {MAX_RANK_SENTINEL} from "./constants.js"
import {GNode, nodeToStr, isNodeInitiallyVisible} from "./node.js"
import {NODE_INPUT_WIDTH, MINIMUM_NODE_OUTPUT_APPROACH} from "./node.js"
import {DEFAULT_NODE_BUBBLE_RADIUS} from "./node.js"
import {Edge, edgeToStr} from "./edge.js"
import {View, PhaseView} from "./view.js"
import {MySelection} from "./selection.js"
import {partial, alignUp} from "./util.js"

function nodeToStringKey(n) {
  return "" + n.id;
}

interface GraphState {
  showTypes: boolean;
  selection: MySelection;
  mouseDownNode: any;
  justDragged: boolean,
  justScaleTransGraph: boolean,
  lastKeyDown: number,
  hideDead: boolean
}

export class GraphView extends View implements PhaseView {
  divElement: d3.Selection<any, any, any, any>;
  svg: d3.Selection<any, any, any, any>;
  showPhaseByName: (string) => void;
  state: GraphState;
  nodes: Array<GNode>;
  edges: Array<any>;
  selectionHandler: NodeSelectionHandler;
  graphElement: d3.Selection<any, any, any, any>;
  visibleNodes: d3.Selection<any, GNode, any, any>;
  visibleEdges: d3.Selection<any, Edge, any, any>;
  minGraphX: number;
  maxGraphX: number;
  minGraphY: number;
  maxGraphY: number;
  width: number;
  height: number;
  maxGraphNodeX: number;
  drag: d3.DragBehavior<any, GNode, GNode>;
  panZoom: d3.ZoomBehavior<SVGElement, any>;
  nodeMap: Array<any>;
  visibleBubbles: d3.Selection<any, any, any, any>;
  transitionTimout: number;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "graph");
    return pane;
  }

  constructor(id, broker, showPhaseByName: (string) => void) {
    super(id);
    var graph = this;
    this.showPhaseByName = showPhaseByName;
    this.divElement = d3.select(this.divNode);
    const svg = this.divElement.append("svg").attr('version', '1.1')
      .attr("width", "100%")
      .attr("height", "100%");
    svg.on("click", function (d) {
      graph.selectionHandler.clear();
    });
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
          if (node.origin && node.origin.bytecodePosition) {
            locations.push({ bytecodePosition: node.origin.bytecodePosition });
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

    graph.state.selection = new MySelection(nodeToStringKey);

    const defs = svg.append('svg:defs');
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

    graph.drag = d3.drag<any, GNode, GNode>()
      .on("drag", function (d) {
        d.x += d3.event.dx;
        d.y += d3.event.dy;
        graph.updateGraphVisibility();
      });


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
    }).on("keyup", function () {
      graph.svgKeyUp.call(graph);
    });

    function zoomed() {
      if (d3.event.shiftKey) return false;
      graph.graphElement.attr("transform", d3.event.transform);
    }

    const zoomSvg = d3.zoom<SVGElement, any>()
      .scaleExtent([0.2, 40])
      .on("zoom", zoomed)
      .on("start", function () {
        if (d3.event.shiftKey) return;
        d3.select('body').style("cursor", "move");
      })
      .on("end", function () {
        d3.select('body').style("cursor", "auto");
      });

    svg.call(zoomSvg).on("dblclick.zoom", null);

    graph.panZoom = zoomSvg;

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

  getNodeHeight(d): number {
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

  initializeContent(data, rememberedSelection) {
    this.createGraph(data, rememberedSelection);
    if (rememberedSelection != null) {
      this.attachSelection(rememberedSelection);
      this.connectVisibleSelectedNodes();
      this.viewSelection();
    } else {
      this.viewWholeGraph();
    }
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
    const textMeasure = document.getElementById('text-measure') as SVGTSpanElement;
    textMeasure.textContent = text;
    return {
      width: textMeasure.getBBox().width,
      height: textMeasure.getBBox().height,
    };
  }

  createGraph(data, rememberedSelection) {
    var g = this;
    g.nodes = [];
    g.nodeMap = [];
    data.nodes.forEach(function (n, i) {
      n.__proto__ = GNode.prototype;
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
      n.width = alignUp(innerwidth + NODE_INPUT_WIDTH * 2,
        NODE_INPUT_WIDTH);
      var innerheight = Math.max(n.labelbbox.height, n.typebbox.height);
      n.normalheight = innerheight + 20;
      g.nodes.push(n);
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

  selectAllNodes() {
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

  searchInputAction(searchBar, e: KeyboardEvent) {
    const graph = this;
    if (e.keyCode == 13) {
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
          if ((e.ctrlKey || n.visible) && filterFunction(n)) {
            if (e.ctrlKey) n.visible = true;
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
    e.stopPropagation();
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
    const selEdges = graph.visibleEdges.selectAll<SVGPathElement, Edge>("path").data(filteredEdges, edgeToStr);

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
      .on("click", function (edge) {
        d3.event.stopPropagation();
        if (!d3.event.shiftKey) {
          graph.selectionHandler.clear();
        }
        graph.selectionHandler.select([edge.source, edge.target], true);
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
    const filteredNodes = graph.nodes.filter(n => n.visible);
    const allNodes = graph.visibleNodes.selectAll<SVGGElement, GNode>("g");
    const selNodes = allNodes.data(filteredNodes, nodeToStr);

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
      .on('mouseenter', function (node) {
        const visibleEdges = graph.visibleEdges.selectAll<SVGPathElement, Edge>('path');
        const adjInputEdges = visibleEdges.filter(e => { return e.target === node; });
        const adjOutputEdges = visibleEdges.filter(e => { return e.source === node; });
        adjInputEdges.attr('relToHover', "input");
        adjOutputEdges.attr('relToHover', "output");
        const adjInputNodes = adjInputEdges.data().map(e => e.source);
        const visibleNodes = graph.visibleNodes.selectAll<SVGGElement, GNode>("g");
        const input = visibleNodes.data<GNode>(adjInputNodes, nodeToStr)
          .attr('relToHover', "input");
        const adjOutputNodes = adjOutputEdges.data().map(e => e.target);
        const output = visibleNodes.data<GNode>(adjOutputNodes, nodeToStr)
          .attr('relToHover', "output");
        graph.updateGraphVisibility();
      })
      .on('mouseleave', function (node) {
        const visibleEdges = graph.visibleEdges.selectAll<SVGPathElement, Edge>('path');
        const adjEdges = visibleEdges.filter(e => { return e.target === node || e.source === node; });
        adjEdges.attr('relToHover', "none");
        const adjNodes = adjEdges.data().map(e => e.target).concat(adjEdges.data().map(e => e.source));
        const visibleNodes = graph.visibleNodes.selectAll<SVGPathElement, GNode>("g");
        const nodes = visibleNodes.data(adjNodes, nodeToStr)
          .attr('relToHover', "none");
        graph.updateGraphVisibility();
      })
      .on("click", (d) => {
        if (!d3.event.shiftKey) graph.selectionHandler.clear();
        graph.selectionHandler.select([d], undefined);
        d3.event.stopPropagation();
      })
      .call(graph.drag)

    newGs.append("rect")
      .attr("rx", 10)
      .attr("ry", 10)
      .attr('width', function (d) {
        return d.getTotalNodeWidth();
      })
      .attr('height', function (d) {
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
          .on("click", function (d) {
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
          .on("click", function (d) {
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

    const newAndOldNodes = newGs.merge(selNodes);

    newAndOldNodes.select<SVGTextElement>('.type').each(function (d) {
      this.setAttribute('visibility', graph.state.showTypes ? 'visible' : 'hidden');
    });

    newAndOldNodes
      .classed("selected", function (n) {
        if (state.selection.isSelected(n)) return true;
        return false;
      })
      .attr("transform", function (d) { return "translate(" + d.x + "," + d.y + ")"; })
      .select('rect')
      .attr('height', function (d) { return graph.getNodeHeight(d); });

    graph.visibleBubbles = d3.selectAll('circle');

    graph.updateInputAndOutputBubbles();

    graph.maxGraphX = graph.maxGraphNodeX;
    selEdges.attr("d", function (edge) {
      return edge.generatePath(graph);
    });
  }

  getSvgViewDimensions() {
    return [this.container.clientWidth, this.container.clientHeight];
  }

  getSvgExtent(): [[number, number], [number, number]] {
    return [[0, 0], [this.container.clientWidth, this.container.clientHeight]];
  }

  minScale() {
    const graph = this;
    const dimensions = this.getSvgViewDimensions();
    const minXScale = dimensions[0] / (2 * graph.width);
    const minYScale = dimensions[1] / (2 * graph.height);
    const minScale = Math.min(minXScale, minYScale);
    this.panZoom.scaleExtent([minScale, 40]);
    return minScale;
  }

  onresize() {
    const trans = d3.zoomTransform(this.svg.node());
    const ctrans = this.panZoom.constrain()(trans, this.getSvgExtent(), this.panZoom.translateExtent())
    this.panZoom.transform(this.svg, ctrans)
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
    graph.visibleNodes.selectAll<SVGGElement, GNode>("g").each(function (n) {
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
    const [width, height] = this.getSvgViewDimensions();
    const dx = maxX - minX;
    const dy = maxY - minY;
    const x = (minX + maxX) / 2;
    const y = (minY + maxY) / 2;
    const scale = Math.min(width / (1.1 * dx), height / (1.1 * dy));
    const transform = d3.zoomIdentity.translate(1500, 100).scale(0.75);
    this.svg
      .transition().duration(300).call(this.panZoom.translateTo, x, y)
      .transition().duration(300).call(this.panZoom.scaleTo, scale)
      .transition().duration(300).call(this.panZoom.translateTo, x, y);
  }

  viewWholeGraph() {
    this.panZoom.scaleTo(this.svg, 0);
    this.panZoom.translateTo(this.svg, this.minGraphX + this.width / 2, this.minGraphY + this.height / 2)
  }
}
