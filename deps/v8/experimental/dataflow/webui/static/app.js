// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * V8 C++ Dataflow Explorer — Frontend Application
 * Synchronizes Monaco Editor (with C++ highlighting, inline flow annotations,
 * hover provider, and DFG token underlines), Active Path Stepper,
 * Hierarchical Tree View, and Interactive Layered SVG DAG Graph View.
 */

const state = {
  status: null,
  presets: [],
  theme: localStorage.getItem('v8-dataflow-theme') || 'dark', // 'dark' | 'light'
  currentMethod: 'query/from-leaves',
  pruneCycles: true,
  viewMode: 'split', // 'split' | 'graph' | 'tree'
  dagOrientation: 'TD', // 'TD' | 'LR'

  // Open files cache: relPath -> { path, absPath, content, nodes, model }
  fileCache: new Map(),
  openTabs: [], // array of relPath
  previewTab: null, // relPath of current unpinned preview tab (at most one), or null
  tabMru: [], // array of relPath in most-recently-used order
  tabViewStates: new Map(), // relPath -> { line, column, viewState }
  isSwitchingModel: false,
  currentFile: null,
  currentCursorNode: null,

  // Active Query State
  queryData: null, // response from /api/query
  treeById: new Map(), // tree_id -> treeNode
  canonicalToTreeIds: new Map(), // canonical_id -> [tree_id, ...]
  selectedTreeId: null,
  activePath: [], // array of tree_id from root -> selected (or leaf -> root when flipped)
  activeStepIndex: 0,
  collapsedTreeIds: new Set(),
  expandedBranchLimits: new Map(), // tree_id -> max children limit
  taintFilter: 'all',
  rangeFilter: 'all',

  // Monaco Editor instance & decoration collection
  editor: null,
  decorationsCollection: null,

  // DAG Pan/Zoom state
  dagTransform: { x: 24, y: 24, scale: 1 },
  dagBounds: { width: 800, height: 600 },
};

function applyTheme(theme) {
  state.theme = theme === 'light' ? 'light' : 'dark';
  document.documentElement.setAttribute('data-theme', state.theme);
  localStorage.setItem('v8-dataflow-theme', state.theme);

  const iconEl = document.getElementById('theme-toggle-icon');
  const labelEl = document.getElementById('theme-toggle-label');
  if (iconEl && labelEl) {
    if (state.theme === 'light') {
      iconEl.textContent = '🌙';
      labelEl.textContent = 'Dark';
    } else {
      iconEl.textContent = '☀️';
      labelEl.textContent = 'Light';
    }
  }

  if (window.monaco && window.monaco.editor) {
    window.monaco.editor.setTheme(
      state.theme === 'light' ? 'v8-dataflow-light' : 'v8-dataflow-dark'
    );
  }
}

// Utility: escape HTML
function esc(str) {
  if (str === null || str === undefined) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function basename(p) {
  if (!p) return '';
  const idx = p.lastIndexOf('/');
  return idx >= 0 ? p.slice(idx + 1) : p;
}

function getEffectiveLoc(treeNode) {
  if (!treeNode) return { file: '', line: 0, column: 0 };
  const floc = treeNode.flow_location || {};
  const nloc = (treeNode.node && treeNode.node.location) || {};
  if (floc.rel_file && floc.line > 0) {
    return {
      file: floc.rel_file,
      absFile: floc.file,
      line: floc.line,
      column: floc.column || 1,
    };
  }
  return {
    file: nloc.rel_file || nloc.file || '',
    absFile: nloc.file || '',
    line: nloc.line || 0,
    column: nloc.column || 1,
  };
}

// Initialize Monaco Editor & App
window.addEventListener('DOMContentLoaded', async () => {
  applyTheme(state.theme);
  bindUIControls();
  await initMonaco();
  await fetchStatusAndPresets();
});

async function initMonaco() {
  return new Promise((resolve) => {
    window.require.config({ paths: { vs: '/vs' } });
    window.require(['vs/editor/editor.main'], () => {
      monaco.editor.defineTheme('v8-dataflow-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
          { token: 'keyword', foreground: '38bdf8' },
          { token: 'type', foreground: '2dd4bf' },
          { token: 'identifier', foreground: 'e6edf3' },
          { token: 'comment', foreground: '647a90', fontStyle: 'italic' },
        ],
        colors: {
          'editor.background': '#0b0f14',
          'editor.lineHighlightBackground': '#141d28',
          'editorLineNumber.foreground': '#496078',
          'editorLineNumber.activeForeground': '#38bdf8',
          'editorGutter.background': '#0b0f14',
        },
      });

      monaco.editor.defineTheme('v8-dataflow-light', {
        base: 'vs',
        inherit: true,
        rules: [
          { token: 'keyword', foreground: '0284c7' },
          { token: 'type', foreground: '0d9488' },
          { token: 'identifier', foreground: '0f172a' },
          { token: 'comment', foreground: '64748b', fontStyle: 'italic' },
        ],
        colors: {
          'editor.background': '#ffffff',
          'editor.lineHighlightBackground': '#f1f5f9',
          'editorLineNumber.foreground': '#94a3b8',
          'editorLineNumber.activeForeground': '#0284c7',
          'editorGutter.background': '#ffffff',
        },
      });

      const container = document.getElementById('monaco-container');
      state.editor = monaco.editor.create(container, {
        value: '// Loading V8 C++ source…\n',
        language: 'cpp',
        theme: state.theme === 'light' ? 'v8-dataflow-light' : 'v8-dataflow-dark',
        readOnly: true,
        automaticLayout: true,
        fontFamily: "'JetBrains Mono', monospace",
        fontSize: 12.5,
        lineHeight: 20,
        minimap: { enabled: true, scale: 1 },
        scrollBeyondLastLine: false,
        glyphMargin: true,
        renderLineHighlight: 'all',
        smoothScrolling: true,
      });

      state.decorationsCollection = state.editor.createDecorationsCollection([]);

      // Register Hover Provider for DFG Symbols & Active Trace Nodes
      monaco.languages.registerHoverProvider('cpp', {
        provideHover(model, position) {
          if (!state.currentFile) return null;
          const fileObj = state.fileCache.get(state.currentFile);
          if (!fileObj || !fileObj.nodes) return null;

          const line = position.lineNumber;
          const col = position.column;
          const hit = findNearestNodeInFile(fileObj.nodes, line, col, 14);
          if (!hit) return null;

          const mdLines = [
            `**\`[${hit.kind}]\` \`${hit.name}\`**`,
            hit.type ? `Type: \`${hit.type}\`` : '',
            `Location: \`${state.currentFile}:${hit.line}:${hit.column}\``,
          ].filter(Boolean);

          // Check if this node is part of the active query trace
          if (state.canonicalToTreeIds.has(hit.id)) {
            const tids = state.canonicalToTreeIds.get(hit.id);
            const firstTNode = state.treeById.get(tids[0]);
            if (firstTNode) {
              const rangeSuffix =
                firstTNode.value_range && firstTNode.value_range.display
                  ? ` · Value Range: \`${firstTNode.value_range.display}\``
                  : '';
              mdLines.push(
                `---\n**In Active Trace**: Flow Kind \`${firstTNode.flow_kind}\` (Depth ${firstTNode.depth})${rangeSuffix}`
              );
            }
          }
          mdLines.push(`\n*Tip: \`Alt+Click\` symbol to re-root dataflow trace here.*`);

          return {
            range: new monaco.Range(
              hit.line,
              Math.max(1, hit.column),
              hit.line,
              Math.max(1, hit.column + hit.name.length)
            ),
            contents: [{ value: mdLines.join('\n\n') }],
          };
        },
      });

      // Mouse Down handler: Click updates Cursor Inspector; Alt+Click runs Dataflow Trace!
      state.editor.onMouseDown((e) => {
        if (!e.target || !e.target.position || !state.currentFile) return;
        const pos = e.target.position;
        const fileObj = state.fileCache.get(state.currentFile);
        const hit = fileObj
          ? findNearestNodeInFile(fileObj.nodes, pos.lineNumber, pos.column, 20)
          : null;

        updateCursorInspector(hit, pos.lineNumber, pos.column);
        saveCurrentTabViewState();

        if (e.event.altKey) {
          e.event.preventDefault();
          pinFileTab(state.currentFile);
          runDataflowQuery(state.currentMethod, state.currentFile, pos.lineNumber, pos.column);
        }
      });

      state.editor.onDidChangeCursorPosition((e) => {
        if (state.isSwitchingModel || !state.currentFile) return;
        saveCurrentTabViewState(e.position.lineNumber, e.position.column);
      });

      state.editor.onDidScrollChange(() => {
        if (state.isSwitchingModel || !state.currentFile) return;
        saveCurrentTabViewState();
      });

      // Add Monaco Context Menu Actions
      state.editor.addAction({
        id: 'dfg.traceFromLeaves',
        label: 'Dataflow: Trace Origins → Cursor (From Leaves)',
        contextMenuGroupId: 'navigation',
        contextMenuOrder: 1.1,
        run: (ed) => {
          const pos = ed.getPosition();
          if (pos && state.currentFile) {
            pinFileTab(state.currentFile);
            setQueryMethodUI('query/from-leaves');
            runDataflowQuery('query/from-leaves', state.currentFile, pos.lineNumber, pos.column);
          }
        },
      });

      state.editor.addAction({
        id: 'dfg.traceIncoming',
        label: 'Dataflow: Trace Backward (Cursor ← Inputs)',
        contextMenuGroupId: 'navigation',
        contextMenuOrder: 1.2,
        run: (ed) => {
          const pos = ed.getPosition();
          if (pos && state.currentFile) {
            pinFileTab(state.currentFile);
            setQueryMethodUI('query/incoming');
            runDataflowQuery('query/incoming', state.currentFile, pos.lineNumber, pos.column);
          }
        },
      });

      state.editor.addAction({
        id: 'dfg.traceOutgoing',
        label: 'Dataflow: Trace Outgoing (Cursor → Sinks)',
        contextMenuGroupId: 'navigation',
        contextMenuOrder: 1.3,
        run: (ed) => {
          const pos = ed.getPosition();
          if (pos && state.currentFile) {
            pinFileTab(state.currentFile);
            setQueryMethodUI('query/outgoing');
            runDataflowQuery('query/outgoing', state.currentFile, pos.lineNumber, pos.column);
          }
        },
      });

      resolve();
    });
  });
}

function findNearestNodeInFile(nodes, line, col, maxColDist = 20) {
  if (!nodes || !nodes.length) return null;
  let best = null;
  let bestScore = Infinity;
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i];
    if (n.line < line - 1) continue;
    if (n.line > line + 1) break;
    const lineDiff = Math.abs(n.line - line);
    const len = Math.max(1, (n.name || '').length);
    let colDist = 0;
    if (col < n.column) colDist = n.column - col;
    else if (col > n.column + len) colDist = col - (n.column + len);
    if (lineDiff === 0 && colDist > maxColDist) continue;
    const kindBonus = n.kind === 'Variable' || n.kind === 'Parameter' || n.kind === 'Field' ? 0 : 2;
    const score = lineDiff * 100 + colDist * 2 + kindBonus;
    if (score < bestScore) {
      bestScore = score;
      best = n;
    }
  }
  return best;
}

function updateCursorInspector(nodeHit, fallbackLine, fallbackCol) {
  const kindBadge = document.getElementById('inspector-kind-badge');
  const nameEl = document.getElementById('inspector-symbol-name');
  const typeEl = document.getElementById('inspector-symbol-type');
  const locEl = document.getElementById('inspector-symbol-loc');
  const btnOrigins = document.getElementById('btn-trace-origins-here');
  const btnIncoming = document.getElementById('btn-trace-incoming-here');
  const btnOutgoing = document.getElementById('btn-trace-outgoing-here');

  if (!nodeHit) {
    state.currentCursorNode = {
      file: state.currentFile,
      line: fallbackLine,
      column: fallbackCol,
    };
    kindBadge.className = 'node-kind-badge kind-Expression';
    kindBadge.textContent = 'Cursor';
    nameEl.textContent = `Line ${fallbackLine}, Col ${fallbackCol}`;
    typeEl.textContent = '';
    locEl.textContent = state.currentFile || '';
  } else {
    state.currentCursorNode = {
      ...nodeHit,
      file: state.currentFile,
    };
    kindBadge.className = `node-kind-badge kind-${nodeHit.kind}`;
    kindBadge.textContent = nodeHit.kind;
    nameEl.textContent = nodeHit.name;
    typeEl.textContent = nodeHit.type ? `: ${nodeHit.type}` : '';
    locEl.textContent = `${basename(state.currentFile)}:${nodeHit.line}:${nodeHit.column}`;
  }

  btnOrigins.disabled = !state.currentFile;
  btnIncoming.disabled = !state.currentFile;
  btnOutgoing.disabled = !state.currentFile;
}

async function fetchStatusAndPresets() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();
    state.status = data.status;
    state.presets = data.presets || [];

    const pill = document.getElementById('workspace-status-pill');
    const s = state.status;
    const fullStatusText = `v8-dataflow @ ${s.gitHead.split(' ')[0]} · ${(s.nodesCount / 1e6).toFixed(2)}M nodes · ${(s.edgesCount / 1e6).toFixed(2)}M edges · ${s.filesCount.toLocaleString()} files`;
    pill.textContent = `${(s.nodesCount / 1e6).toFixed(1)}M nodes · ${(s.edgesCount / 1e6).toFixed(1)}M edges`;
    pill.title = fullStatusText;

    const select = document.getElementById('preset-select');
    select.innerHTML = `<option value="">⚡ Presets (${state.presets.length})…</option>`;
    for (const p of state.presets) {
      const opt = document.createElement('option');
      opt.value = p.id;
      opt.textContent = p.title;
      select.appendChild(opt);
    }

    // If URL parameters specify a file or query, restore from URL first;
    // otherwise load the first preset automatically (using replaceState).
    const restoredFromUrl = await restoreStateFromUrlOrHistory(null, true);
    if (!restoredFromUrl && state.presets.length > 0) {
      await loadPreset(state.presets[0], { replaceHistory: true });
    }
  } catch (err) {
    console.error('Failed to fetch status:', err);
  }
}

function formatMethodShort(method) {
  if (method === 'query/incoming') return 'incoming';
  if (method === 'query/outgoing') return 'outgoing';
  return 'from-leaves';
}

function parseMethodShort(shortMode) {
  if (shortMode === 'incoming' || shortMode === 'query/incoming') return 'query/incoming';
  if (shortMode === 'outgoing' || shortMode === 'query/outgoing') return 'query/outgoing';
  return 'query/from-leaves';
}

function setViewModeUI(v) {
  if (!v || !['split', 'graph', 'tree'].includes(v)) return;
  state.viewMode = v;
  document.querySelectorAll('.view-tab').forEach((t) => {
    t.classList.toggle('active', t.dataset.view === v);
  });
  const graphPanel = document.getElementById('graph-view-panel');
  const treePanel = document.getElementById('tree-view-panel');
  if (graphPanel && treePanel) {
    graphPanel.classList.toggle('hidden', v === 'tree');
    treePanel.classList.toggle('hidden', v === 'graph');
  }
}

function buildCurrentHistorySnapshot() {
  const curLoc = state.currentCursorNode || {};
  const activeTreeNode = state.selectedTreeId ? state.treeById.get(state.selectedTreeId) : null;
  const treeLoc = activeTreeNode ? getEffectiveLoc(activeTreeNode) : null;

  const viewFile = curLoc.file || (treeLoc && treeLoc.file) || state.currentFile || '';
  const viewLine = curLoc.line || (treeLoc && treeLoc.line) || 1;
  const viewCol = curLoc.column || (treeLoc && treeLoc.column) || 1;
  const viewSymbol =
    curLoc.name ||
    (activeTreeNode && activeTreeNode.node && activeTreeNode.node.name) ||
    '';

  const qReq = (state.queryData && (state.queryData.requested || state.queryData.resolved)) || null;
  return {
    file: viewFile,
    line: viewLine,
    col: viewCol,
    symbol: viewSymbol,
    qFile: qReq ? qReq.file : '',
    qLine: qReq ? qReq.line : 0,
    qCol: qReq ? qReq.column : 0,
    method: state.currentMethod,
    view: state.viewMode,
    prune: state.pruneCycles ? 1 : 0,
    step: state.selectedTreeId || '',
    taint: state.taintFilter || 'all',
    range: state.rangeFilter || 'all',
  };
}

function setFilterSelectsUI(taintFilter, rangeFilter) {
  state.taintFilter = taintFilter || 'all';
  state.rangeFilter = rangeFilter || 'all';
  const taintEl = document.getElementById('taint-filter-select');
  const rangeEl = document.getElementById('range-filter-select');
  if (taintEl) {
    taintEl.value = state.taintFilter;
    taintEl.classList.toggle('active-filter', state.taintFilter !== 'all');
  }
  if (rangeEl) {
    rangeEl.value = state.rangeFilter;
    rangeEl.classList.toggle('active-filter', state.rangeFilter !== 'all');
  }
}

function syncBrowserHistory({ replace = false } = {}) {
  if (state.isRestoringHistory) return;

  const snap = buildCurrentHistorySnapshot();
  const params = new URLSearchParams();
  if (snap.file) {
    params.set('file', snap.file);
    params.set('line', String(snap.line));
    params.set('col', String(snap.col));
  }
  if (snap.symbol) {
    params.set('symbol', snap.symbol);
  }
  if (snap.qFile && snap.qLine > 0) {
    params.set('query', `${snap.qFile}:${snap.qLine}:${snap.qCol}`);
    params.set('mode', formatMethodShort(snap.method));
  }
  if (snap.step) {
    params.set('step', snap.step);
  }
  if (snap.view && snap.view !== 'split') {
    params.set('view', snap.view);
  }
  if (!snap.prune) {
    params.set('prune', '0');
  }
  if (snap.taint && snap.taint !== 'all') {
    params.set('taint', snap.taint);
  }
  if (snap.range && snap.range !== 'all') {
    params.set('range', snap.range);
  }

  const qs = params.toString();
  const newUrl = qs ? `${window.location.pathname}?${qs}` : window.location.pathname;
  const currentUrl = `${window.location.pathname}${window.location.search}`;

  if (snap.file) {
    const label = snap.symbol ? `${snap.symbol} (${basename(snap.file)}:${snap.line})` : `${basename(snap.file)}:${snap.line}`;
    document.title = `${label} — V8 C++ Dataflow Explorer`;
  }

  if (replace || newUrl === currentUrl) {
    window.history.replaceState(snap, '', newUrl);
  } else {
    window.history.pushState(snap, '', newUrl);
  }
}

async function restoreStateFromUrlOrHistory(histState = null, isInitialLoad = false) {
  let snap = histState;
  if (!snap) {
    const params = new URLSearchParams(window.location.search);
    const fileParam = params.get('file') || '';
    const queryParam = params.get('query') || '';
    if (!fileParam && !queryParam) {
      return false;
    }
    let qFile = '';
    let qLine = 0;
    let qCol = 1;
    if (queryParam) {
      const parts = queryParam.split(':');
      if (parts.length >= 2) {
        qCol = parts.length >= 3 ? parseInt(parts.pop(), 10) || 1 : 1;
        qLine = parseInt(parts.pop(), 10) || 1;
        qFile = parts.join(':');
      }
    } else if (fileParam) {
      qFile = fileParam;
      qLine = parseInt(params.get('line') || '1', 10) || 1;
      qCol = parseInt(params.get('col') || '1', 10) || 1;
    }
    snap = {
      file: fileParam || qFile,
      line: parseInt(params.get('line') || String(qLine || 1), 10) || 1,
      col: parseInt(params.get('col') || String(qCol || 1), 10) || 1,
      symbol: params.get('symbol') || '',
      qFile,
      qLine,
      qCol,
      method: parseMethodShort(params.get('mode') || 'from-leaves'),
      view: params.get('view') || 'split',
      prune: params.get('prune') === '0' ? 0 : 1,
      step: params.get('step') || '',
      taint: params.get('taint') || 'all',
      range: params.get('range') || 'all',
    };
  }

  state.isRestoringHistory = true;
  try {
    setViewModeUI(snap.view || 'split');
    setFilterSelectsUI(snap.taint || 'all', snap.range || 'all');
    state.pruneCycles = snap.prune !== 0;
    const pruneCheckbox = document.getElementById('prune-cycles-checkbox');
    if (pruneCheckbox) pruneCheckbox.checked = state.pruneCycles;
    setQueryMethodUI(snap.method || 'query/from-leaves');

    const curQ = (state.queryData && (state.queryData.requested || state.queryData.resolved)) || null;
    const sameQuery =
      curQ &&
      curQ.file === snap.qFile &&
      Number(curQ.line) === Number(snap.qLine) &&
      Number(curQ.column) === Number(snap.qCol) &&
      state.queryData.method === snap.method;

    if (snap.qFile && snap.qLine > 0) {
      if (!sameQuery) {
        await openFileInEditor(snap.qFile, snap.qLine, snap.qCol, {
          skipHistory: true,
          pin: true,
        });
        await runDataflowQuery(snap.method, snap.qFile, snap.qLine, snap.qCol, {
          skipHistory: true,
          targetTreeId: snap.step || null,
        });
      } else {
        if (snap.step && state.treeById.has(snap.step)) {
          await selectTreeNode(snap.step, { skipHistory: true });
        }
        renderTreeView();
      }
      if (
        snap.file &&
        (snap.file !== state.currentFile ||
          (state.currentCursorNode && Number(state.currentCursorNode.line) !== Number(snap.line)))
      ) {
        await openFileInEditor(snap.file, snap.line, snap.col, { skipHistory: true });
      }
    } else if (snap.file) {
      await openFileInEditor(snap.file, snap.line, snap.col, {
        skipHistory: true,
        pin: true,
      });
    }
  } finally {
    state.isRestoringHistory = false;
  }

  if (isInitialLoad) {
    syncBrowserHistory({ replace: true });
  }
  return true;
}

async function loadPreset(preset, opts = {}) {
  setQueryMethodUI(preset.method);
  await openFileInEditor(preset.file, preset.line, preset.column, {
    skipHistory: true,
    pin: true,
  });
  await runDataflowQuery(preset.method, preset.file, preset.line, preset.column, {
    replaceHistory: Boolean(opts.replaceHistory),
  });
}

function setQueryMethodUI(method) {
  state.currentMethod = method;
  document.querySelectorAll('.mode-btn').forEach((btn) => {
    btn.classList.toggle('active', btn.dataset.method === method);
  });
}

function saveCurrentTabViewState(overrideLine = 0, overrideColumn = 0) {
  if (!state.editor || !state.currentFile) return;
  const pos = state.editor.getPosition();
  const prev = state.tabViewStates.get(state.currentFile) || {};
  const line =
    overrideLine > 0
      ? overrideLine
      : (pos && pos.lineNumber) ||
        (state.currentCursorNode &&
          state.currentCursorNode.file === state.currentFile &&
          state.currentCursorNode.line) ||
        prev.line ||
        1;
  const column =
    overrideColumn > 0
      ? overrideColumn
      : (pos && pos.column) ||
        (state.currentCursorNode &&
          state.currentCursorNode.file === state.currentFile &&
          state.currentCursorNode.column) ||
        prev.column ||
        1;
  state.tabViewStates.set(state.currentFile, {
    line,
    column,
    viewState: state.editor.saveViewState(),
  });
}

function touchTabMru(tabPath) {
  if (!tabPath) return;
  state.tabMru = state.tabMru.filter((p) => p !== tabPath);
  state.tabMru.push(tabPath);
}

function pinFileTab(tabPath) {
  if (!tabPath) return;
  if (state.previewTab === tabPath) {
    state.previewTab = null;
    renderFileTabs();
  }
}

async function closeFileTab(tabPath) {
  const idx = state.openTabs.indexOf(tabPath);
  if (idx < 0) return;

  state.openTabs.splice(idx, 1);
  state.tabMru = state.tabMru.filter((p) => p !== tabPath);
  if (state.previewTab === tabPath) {
    state.previewTab = null;
  }

  if (state.currentFile === tabPath) {
    if (state.openTabs.length > 0) {
      let nextTab = null;
      for (let i = state.tabMru.length - 1; i >= 0; i--) {
        if (state.openTabs.includes(state.tabMru[i])) {
          nextTab = state.tabMru[i];
          break;
        }
      }
      if (!nextTab) {
        nextTab = state.openTabs[Math.min(idx, state.openTabs.length - 1)];
      }
      await openFileInEditor(nextTab, 0, 0, { preserveViewState: true });
      return;
    } else {
      state.currentFile = null;
      state.currentCursorNode = null;
      if (state.editor && window.monaco) {
        state.isSwitchingModel = true;
        try {
          if (!state.emptyModel) {
            state.emptyModel = monaco.editor.createModel(
              '// No file open. Click any node in the trace or search for a V8 file above.\n',
              'cpp'
            );
          }
          state.editor.setModel(state.emptyModel);
          if (state.decorationsCollection) {
            state.decorationsCollection.set([]);
          }
        } finally {
          state.isSwitchingModel = false;
        }
      }
    }
  }
  renderFileTabs();
}

async function openFileInEditor(relFile, targetLine = 1, targetColumn = 1, opts = {}) {
  if (!relFile) return;

  if (state.currentFile && state.editor) {
    saveCurrentTabViewState();
  }

  let fileObj = state.fileCache.get(relFile);
  if (!fileObj) {
    const res = await fetch(`/api/file?path=${encodeURIComponent(relFile)}`);
    if (!res.ok) {
      console.warn('Could not load file:', relFile);
      return;
    }
    fileObj = await res.json();
    state.fileCache.set(relFile, fileObj);
    if (fileObj.path && fileObj.path !== relFile) {
      state.fileCache.set(fileObj.path, fileObj);
    }
  }

  const normalizedPath = fileObj.path;
  const shouldPin = Boolean(opts.pin);
  const alreadyOpenIdx = state.openTabs.indexOf(normalizedPath);

  if (alreadyOpenIdx >= 0) {
    if (shouldPin && state.previewTab === normalizedPath) {
      state.previewTab = null;
    }
  } else if (shouldPin) {
    if (state.previewTab) {
      const prevIdx = state.openTabs.indexOf(state.previewTab);
      if (prevIdx >= 0) {
        state.openTabs.splice(prevIdx, 0, normalizedPath);
      } else {
        state.openTabs.push(normalizedPath);
      }
    } else {
      state.openTabs.push(normalizedPath);
    }
  } else {
    // Preview mode: reuse the existing preview tab slot instead of spawning a new tab per trace step
    if (state.previewTab) {
      const prevIdx = state.openTabs.indexOf(state.previewTab);
      if (prevIdx >= 0) {
        state.tabMru = state.tabMru.filter((p) => p !== state.previewTab);
        state.openTabs[prevIdx] = normalizedPath;
      } else {
        state.openTabs.push(normalizedPath);
      }
    } else {
      state.openTabs.push(normalizedPath);
    }
    state.previewTab = normalizedPath;
  }

  state.currentFile = normalizedPath;
  touchTabMru(normalizedPath);

  if (state.editor && window.monaco) {
    if (!fileObj.model) {
      const uri = monaco.Uri.parse(`file:///${normalizedPath.replace(/^\/+/, '')}`);
      fileObj.model =
        monaco.editor.getModel(uri) ||
        monaco.editor.createModel(fileObj.content, 'cpp', uri);
    }

    const saved = state.tabViewStates.get(normalizedPath);
    const useSavedView = Boolean(opts.preserveViewState) || targetLine <= 0;

    state.isSwitchingModel = true;
    try {
      if (state.editor.getModel() !== fileObj.model) {
        state.editor.setModel(fileObj.model);
      }

      if (useSavedView && saved && saved.viewState) {
        state.editor.restoreViewState(saved.viewState);
        const effLine = saved.line || 1;
        const effCol = saved.column || 1;
        const hit = findNearestNodeInFile(fileObj.nodes, effLine, effCol, 20);
        updateCursorInspector(hit, effLine, effCol);
      } else {
        let effLine = targetLine > 0 ? targetLine : (saved && saved.line) || 0;
        let effCol = targetLine > 0 ? targetColumn : (saved && saved.column) || 1;
        if (effLine <= 0 && state.treeById.size > 0) {
          for (const tid of state.activePath) {
            const loc = getEffectiveLoc(state.treeById.get(tid));
            if (loc.file === normalizedPath && loc.line > 0) {
              effLine = loc.line;
              effCol = loc.column || 1;
              break;
            }
          }
          if (effLine <= 0) {
            for (const tnode of state.treeById.values()) {
              const loc = getEffectiveLoc(tnode);
              if (loc.file === normalizedPath && loc.line > 0) {
                effLine = loc.line;
                effCol = loc.column || 1;
                break;
              }
            }
          }
        }
        if (effLine <= 0) effLine = 1;
        effCol = Math.max(1, effCol || 1);

        state.editor.setPosition({ lineNumber: effLine, column: effCol });
        state.editor.revealLineInCenterIfOutsideViewport(
          effLine,
          monaco.editor.ScrollType.Smooth
        );
        const hit = findNearestNodeInFile(fileObj.nodes, effLine, effCol, 20);
        updateCursorInspector(hit, effLine, effCol);
        saveCurrentTabViewState(effLine, effCol);
      }
      applyEditorDecorations();
    } finally {
      state.isSwitchingModel = false;
    }
  }

  renderFileTabs();

  if (!opts.skipHistory) {
    syncBrowserHistory({ replace: Boolean(opts.replaceHistory) });
  }
}

function renderFileTabs() {
  const bar = document.getElementById('file-tabs-bar');
  if (!bar) return;
  bar.innerHTML = '';

  // Count how many tree nodes belong to each file
  const fileCounts = new Map();
  if (state.treeById.size > 0) {
    for (const tnode of state.treeById.values()) {
      const loc = getEffectiveLoc(tnode);
      if (loc.file) {
        fileCounts.set(loc.file, (fileCounts.get(loc.file) || 0) + 1);
      }
    }
  }

  // Disambiguate tabs that share the same basename by showing parent folder
  const baseCounts = new Map();
  for (const tabPath of state.openTabs) {
    const b = basename(tabPath);
    baseCounts.set(b, (baseCounts.get(b) || 0) + 1);
  }

  let activeBtn = null;
  for (const tabPath of state.openTabs) {
    const isActive = tabPath === state.currentFile;
    const isPreview = tabPath === state.previewTab;
    const btn = document.createElement('div');
    btn.setAttribute('role', 'tab');
    btn.setAttribute('aria-selected', isActive ? 'true' : 'false');
    btn.className = [
      'file-tab',
      isActive ? 'active' : '',
      isPreview ? 'preview' : '',
    ]
      .filter(Boolean)
      .join(' ');
    btn.title = isPreview
      ? `${tabPath} (Preview tab — click again or double-click to keep open)`
      : tabPath;

    const count = fileCounts.get(tabPath) || 0;
    const base = basename(tabPath);
    let dirPrefix = '';
    if ((baseCounts.get(base) || 0) > 1) {
      const parts = tabPath.split('/');
      if (parts.length >= 2) {
        dirPrefix = `${parts[parts.length - 2]}/`;
      }
    }

    btn.innerHTML = `
      <span class="file-tab-label">${
        dirPrefix ? `<span class="file-tab-dir">${esc(dirPrefix)}</span>` : ''
      }${esc(base)}</span>
      ${count > 0 ? `<span class="file-tab-badge" title="${count} trace nodes in this file">${count}</span>` : ''}
      <span class="file-tab-close" role="button" aria-label="Close ${esc(base)}" title="Close (${esc(base)})">×</span>
    `;

    const closeEl = btn.querySelector('.file-tab-close');
    if (closeEl) {
      closeEl.addEventListener('click', (e) => {
        e.stopPropagation();
        closeFileTab(tabPath);
      });
    }

    btn.addEventListener('click', () => {
      if (tabPath === state.currentFile) {
        if (isPreview) {
          pinFileTab(tabPath);
        }
        return;
      }
      openFileInEditor(tabPath, 0, 0, { preserveViewState: true });
    });

    btn.addEventListener('dblclick', (e) => {
      e.stopPropagation();
      pinFileTab(tabPath);
    });

    btn.addEventListener('auxclick', (e) => {
      if (e.button === 1) {
        e.preventDefault();
        e.stopPropagation();
        closeFileTab(tabPath);
      }
    });

    btn.addEventListener('mousedown', (e) => {
      if (e.button === 1) {
        e.preventDefault();
      }
    });

    if (isActive) {
      activeBtn = btn;
    }
    bar.appendChild(btn);
  }

  if (activeBtn) {
    requestAnimationFrame(() => {
      activeBtn.scrollIntoView({
        behavior: 'smooth',
        block: 'nearest',
        inline: 'nearest',
      });
    });
  }
}

async function runDataflowQuery(method, file, line, column, opts = {}) {
  const badge = document.getElementById('query-stats-badge');
  badge.textContent = 'Querying CSR Index…';

  try {
    const res = await fetch('/api/query', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        method,
        file,
        line,
        column,
        pruneCycles: state.pruneCycles,
      }),
    });
    const data = await res.json();
    if (!res.ok || !data.root) {
      badge.textContent = 'No dataflow at location';
      return;
    }

    state.queryData = data;
    indexQueryTree(data.root);

    if (data.resolved && data.resolved.file) {
      const rootFile = data.resolved.file;
      if (!state.openTabs.includes(rootFile)) {
        await openFileInEditor(
          rootFile,
          data.resolved.line || line,
          data.resolved.column || column,
          { skipHistory: true, pin: true }
        );
      } else {
        pinFileTab(rootFile);
        if (state.currentFile === rootFile && data.resolved.line > 0) {
          saveCurrentTabViewState(data.resolved.line, data.resolved.column || 1);
        }
      }
    }

    const m = data.metrics;
    const fullStats = `${data.queryTimeMs} ms · ${m.totalTreeNodes.toLocaleString()} nodes · ${m.terminalLeavesCount} leaves · ${m.uniqueFiles} files`;
    badge.textContent = `${Math.round(data.queryTimeMs)} ms · ${m.totalTreeNodes.toLocaleString()} nodes · ${m.terminalLeavesCount} leaves`;
    badge.title = fullStats;

    // Keep the queried root selected by default unless a specific step/node was requested.
    let initialNode = data.root;
    if (opts.targetTreeId && state.treeById.has(opts.targetTreeId)) {
      initialNode = state.treeById.get(opts.targetTreeId);
    }

    await selectTreeNode(initialNode.tree_id, {
      jumpToRootFileFirst: true,
      skipHistory: true,
    });
    renderFileTabs();
    renderTreeView();
    renderDagGraph();

    if (!opts.skipHistory) {
      syncBrowserHistory({ replace: Boolean(opts.replaceHistory) });
    }
  } catch (err) {
    console.error('Dataflow query error:', err);
    badge.textContent = 'Query Error';
  }
}

function getEffectiveTreeChildren(tnode) {
  if (!tnode) return [];
  if (tnode.children && tnode.children.length > 0) {
    return tnode.children;
  }
  if (tnode.is_cached && tnode.canonical_id) {
    const tids = state.canonicalToTreeIds.get(tnode.canonical_id) || [];
    for (const tid of tids) {
      const primary = state.treeById.get(tid);
      if (primary && primary.children && primary.children.length > 0) {
        return primary.children;
      }
    }
  }
  return [];
}

function indexQueryTree(root) {
  state.treeById.clear();
  state.canonicalToTreeIds.clear();
  state.collapsedTreeIds.clear();
  state.expandedBranchLimits.clear();

  function walk(node) {
    state.treeById.set(node.tree_id, node);
    const cid = node.canonical_id;
    if (!state.canonicalToTreeIds.has(cid)) {
      state.canonicalToTreeIds.set(cid, []);
    }
    state.canonicalToTreeIds.get(cid).push(node.tree_id);

    // Auto-collapse very deep/wide branches past depth 3 initially, and start
    // cached shared references collapsed so they can be expanded on demand
    if (
      (node.depth >= 3 && (node.children || []).length > 0) ||
      node.is_cached
    ) {
      state.collapsedTreeIds.add(node.tree_id);
    }
    for (const c of node.children || []) {
      walk(c);
    }
  }
  walk(root);
}

async function selectTreeNode(treeId, opts = {}) {
  const targetNode = state.treeById.get(treeId);
  if (!targetNode) return;

  state.selectedTreeId = treeId;

  // Build path from root (t_0) down to targetNode
  const chain = [];
  let curr = targetNode;
  let uncollapsedAny = false;
  while (curr) {
    chain.push(curr.tree_id);
    // Ensure all ancestors are expanded in Tree View
    if (curr.parent_tree_id) {
      if (state.collapsedTreeIds.has(curr.parent_tree_id)) {
        state.collapsedTreeIds.delete(curr.parent_tree_id);
        uncollapsedAny = true;
      }
    }
    curr = curr.parent_tree_id ? state.treeById.get(curr.parent_tree_id) : null;
  }
  chain.reverse(); // [root, ..., targetNode]
  state.activePath = chain;

  if (opts.jumpToRootFileFirst && state.queryData && state.queryData.resolved) {
    // Keep focus on the queried target node or selected step
    state.activeStepIndex = chain.length - 1;
  } else {
    state.activeStepIndex = chain.length - 1;
  }

  renderStepperBar();
  const existingRow = document.querySelector(
    `#flow-tree-root .tree-node-row[data-tree-id="${treeId}"]`
  );
  if (uncollapsedAny || (!existingRow && !opts.jumpToRootFileFirst)) {
    renderTreeView();
  } else {
    updateTreeSelectionHighlights();
  }
  updateDagSelectionHighlights();

  const loc = getEffectiveLoc(targetNode);
  if (loc.file && loc.line > 0) {
    await openFileInEditor(loc.file, loc.line, loc.column, { skipHistory: true });
  } else {
    applyEditorDecorations();
  }

  if (!opts.skipHistory) {
    syncBrowserHistory({ replace: Boolean(opts.replaceHistory) });
  }
}

async function stepActivePath(delta) {
  if (!state.activePath.length) return;
  const nextIdx = Math.max(0, Math.min(state.activePath.length - 1, state.activeStepIndex + delta));
  state.activeStepIndex = nextIdx;
  const tid = state.activePath[nextIdx];
  state.selectedTreeId = tid;

  renderStepperBar();
  updateTreeSelectionHighlights();
  updateDagSelectionHighlights();

  const tnode = state.treeById.get(tid);
  const loc = getEffectiveLoc(tnode);
  if (loc.file && loc.line > 0) {
    await openFileInEditor(loc.file, loc.line, loc.column, { skipHistory: true });
  }
  syncBrowserHistory();
}

function renderStepperBar() {
  const prevBtn = document.getElementById('btn-prev-step');
  const nextBtn = document.getElementById('btn-next-step');
  const counterEl = document.getElementById('stepper-counter');
  const crumbsEl = document.getElementById('stepper-crumbs');

  const total = state.activePath.length;
  if (total === 0) {
    prevBtn.disabled = true;
    nextBtn.disabled = true;
    counterEl.textContent = 'Step 0 / 0';
    crumbsEl.innerHTML = `<span class="stepper-placeholder">Select any node in the Tree or DAG to step through its dataflow path.</span>`;
    return;
  }

  prevBtn.disabled = state.activeStepIndex <= 0;
  nextBtn.disabled = state.activeStepIndex >= total - 1;
  counterEl.textContent = `Step ${state.activeStepIndex + 1} / ${total}`;

  crumbsEl.innerHTML = '';
  state.activePath.forEach((tid, idx) => {
    const tnode = state.treeById.get(tid);
    if (!tnode) return;
    const n = tnode.node || {};
    const loc = getEffectiveLoc(tnode);

    if (idx > 0) {
      const arrow = document.createElement('span');
      arrow.className = 'step-arrow';
      arrow.textContent = `──[${tnode.flow_kind || 'Flow'}]──▶`;
      crumbsEl.appendChild(arrow);
    }

    const crumb = document.createElement('button');
    crumb.className = `step-crumb ${idx === state.activeStepIndex ? 'active' : ''}`;
    const rangeHtml =
      tnode.value_range && tnode.value_range.display
        ? `<span class="value-range-pill">${esc(tnode.value_range.display)}</span>`
        : '';
    const taintKind = normalizeTaintKind(tnode.heap_taint);
    const taintHtml = taintKind
      ? `<span class="heap-taint-pill taint-${esc(taintKind)}" title="${esc(formatTaintTitle(taintKind, false))}">${esc(formatTaintLabel(taintKind, false))}</span>`
      : '';
    const deadHtml = tnode.is_dead
      ? `<span class="dead-node-pill" title="Dead parameter / uncalled function (no incoming call sites in index)">DEAD</span>`
      : '';
    crumb.innerHTML = `
      <span class="step-idx">${idx + 1}</span>
      <span>${esc(n.name || 'expr')}</span>
      ${deadHtml}
      ${rangeHtml}
      ${taintHtml}
      <span style="opacity:0.7">${esc(basename(loc.file))}:${loc.line}</span>
    `;
    crumb.addEventListener('click', async () => {
      state.activeStepIndex = idx;
      state.selectedTreeId = tid;
      renderStepperBar();
      updateTreeSelectionHighlights();
      updateDagSelectionHighlights();
      if (loc.file && loc.line > 0) {
        await openFileInEditor(loc.file, loc.line, loc.column, { skipHistory: true });
      }
      syncBrowserHistory();
    });
    crumbsEl.appendChild(crumb);

    if (idx === state.activeStepIndex) {
      setTimeout(() => crumb.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'center' }), 20);
    }
  });
}

// Compute exact 1-based [startCol, endCol] span for a node on a line in Monaco
function getTokenRangeOnLine(model, line, col, rawName) {
  const cleanName = (rawName || '')
    .replace(/^DEAD:\s*/, '')
    .replace(/_return$/, '')
    .replace(/\(\)$/, '')
    .trim();

  if (!model || line < 1 || line > model.getLineCount()) {
    const fallbackLen = Math.max(1, cleanName.length || 1);
    return new monaco.Range(line, Math.max(1, col), line, Math.max(1, col) + fallbackLen);
  }

  const lineText = model.getLineContent(line);
  const idx0 = Math.max(0, Math.min(lineText.length, (col || 1) - 1));

  // 1. If cleanName is a valid C++ identifier (not generic "expr" / "call" / "?:"),
  // look for cleanName at or near col on this line.
  if (cleanName && cleanName !== 'expr' && cleanName !== 'call' && cleanName !== '?:') {
    if (lineText.slice(idx0, idx0 + cleanName.length) === cleanName) {
      return new monaco.Range(line, idx0 + 1, line, idx0 + 1 + cleanName.length);
    }
    // Search within a local window (e.g. when col points to Receiver->member or Class::Method)
    const searchStart = Math.max(0, idx0 - 16);
    const searchEnd = Math.min(lineText.length, idx0 + 48);
    const windowText = lineText.slice(searchStart, searchEnd);
    const escaped = cleanName.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const re = new RegExp(`\\b${escaped}\\b`);
    const m = re.exec(windowText);
    if (m) {
      const startCol = searchStart + m.index + 1;
      return new monaco.Range(line, startCol, line, startCol + cleanName.length);
    }
  }

  // 2. Fallback: extract the C++ identifier token starting at or containing idx0
  const isIdentChar = (ch) => /[A-Za-z0-9_]/.test(ch);
  let s = idx0;
  while (s < lineText.length && !isIdentChar(lineText[s]) && s - idx0 < 12) {
    s++;
  }
  if (s < lineText.length && isIdentChar(lineText[s])) {
    while (s > 0 && isIdentChar(lineText[s - 1])) s--;
    let e = s;
    while (e < lineText.length && isIdentChar(lineText[e])) e++;
    if (e > s) {
      return new monaco.Range(line, s + 1, line, e + 1);
    }
  }

  const len = Math.max(1, Math.min(cleanName.length || 4, Math.max(1, lineText.length - idx0)));
  return new monaco.Range(line, idx0 + 1, line, idx0 + 1 + len);
}

// Apply Monaco Editor Line Highlights, Token Ranges, and Inline Dataflow Annotations!
function applyEditorDecorations() {
  if (!state.editor || !state.decorationsCollection || !state.currentFile) return;

  const model = state.editor.getModel();
  const newDecorations = [];
  const fileObj = state.fileCache.get(state.currentFile);

  // 1. Subtle underline for indexed DFG symbols in the file (up to 600 nodes to keep rendering ultra-fast)
  if (fileObj && fileObj.nodes) {
    const limit = Math.min(fileObj.nodes.length, 600);
    for (let i = 0; i < limit; i++) {
      const n = fileObj.nodes[i];
      if (n.line > 0 && n.column > 0 && n.name && n.name !== 'expr') {
        newDecorations.push({
          range: getTokenRangeOnLine(model, n.line, n.column, n.name),
          options: {
            inlineClassName: 'monaco-dfg-indexed-token',
          },
        });
      }
    }
  }

  // 2. Highlight all nodes in the current dataflow tree that fall in state.currentFile
  if (state.treeById.size > 0) {
    const activePathSet = new Set(state.activePath);
    const selectedNode = state.treeById.get(state.selectedTreeId);
    const annotatedLines = new Set();

    // First pass: Selected step node
    if (selectedNode) {
      const loc = getEffectiveLoc(selectedNode);
      if (loc.file === state.currentFile && loc.line > 0) {
        const n = selectedNode.node || {};
        const stepIdx = state.activePath.indexOf(selectedNode.tree_id) + 1;
        const stepTotal = state.activePath.length;
        annotatedLines.add(loc.line);

        const rangeLabel =
          selectedNode.value_range && selectedNode.value_range.display
            ? ` ${selectedNode.value_range.display}`
            : '';
        const taintLabel = selectedNode.heap_taint
          ? ` [${formatTaintLabel(selectedNode.heap_taint, false)}]`
          : '';
        const deadLabel = selectedNode.is_dead ? ' [DEAD]' : '';
        newDecorations.push({
          range: new monaco.Range(loc.line, 1, loc.line, 1),
          options: {
            isWholeLine: true,
            className: 'monaco-active-step-line',
            after: {
              content: `   ★ [Step ${stepIdx}/${stepTotal} · ${selectedNode.flow_kind}]${deadLabel} ${n.name || ''}${rangeLabel}${taintLabel} ${n.type ? ': ' + n.type : ''}`,
              inlineClassName: 'monaco-inline-annotation-active',
            },
          },
        });

        newDecorations.push({
          range: getTokenRangeOnLine(model, loc.line, loc.column, n.name),
          options: {
            inlineClassName: 'monaco-active-step-token',
          },
        });
      }
    }

    // Second pass: Other steps along the active path in this file
    state.activePath.forEach((tid, idx) => {
      if (tid === state.selectedTreeId) return;
      const tnode = state.treeById.get(tid);
      if (!tnode) return;
      const loc = getEffectiveLoc(tnode);
      if (loc.file === state.currentFile && loc.line > 0 && !annotatedLines.has(loc.line)) {
        annotatedLines.add(loc.line);
        const n = tnode.node || {};
        const rangeLabel =
          tnode.value_range && tnode.value_range.display
            ? ` ${tnode.value_range.display}`
            : '';
        const taintLabel = tnode.heap_taint
          ? ` [${formatTaintLabel(tnode.heap_taint, false)}]`
          : '';
        const deadLabel = tnode.is_dead ? ' [DEAD]' : '';
        newDecorations.push({
          range: new monaco.Range(loc.line, 1, loc.line, 1),
          options: {
            isWholeLine: true,
            className: 'monaco-path-step-line',
            after: {
              content: `   ◆ [Step ${idx + 1}/${state.activePath.length} · ${tnode.flow_kind}]${deadLabel} ${n.name || ''}${rangeLabel}${taintLabel} ${n.type ? ': ' + n.type : ''}`,
              inlineClassName: 'monaco-inline-annotation-path',
            },
          },
        });
      }
    });

    // Third pass: Other parallel trace nodes in this file
    for (const tnode of state.treeById.values()) {
      if (activePathSet.has(tnode.tree_id)) continue;
      const loc = getEffectiveLoc(tnode);
      if (loc.file === state.currentFile && loc.line > 0 && !annotatedLines.has(loc.line)) {
        annotatedLines.add(loc.line);
        const n = tnode.node || {};
        newDecorations.push({
          range: new monaco.Range(loc.line, 1, loc.line, 1),
          options: {
            isWholeLine: true,
            className: 'monaco-trace-other-line',
            after: {
              content: `   ◇ [${tnode.flow_kind}] ${n.name || ''}`,
              inlineClassName: 'monaco-inline-annotation-other',
            },
          },
        });
      }
    }
  }

  state.decorationsCollection.set(newDecorations);
}

function normalizeTaintKind(taint) {
  if (!taint) return '';
  if (taint === 'trusted' || taint === 'trusted_space') return 'trusted';
  if (taint === 'embedder') return 'embedder';
  if (taint === 'sandbox' || taint === 'in_sandbox') return 'sandbox';
  return taint;
}

function formatTaintLabel(taint, isPath = false) {
  const kind = normalizeTaintKind(taint);
  if (!kind) return '';
  let base = 'heap:sandbox';
  if (kind === 'trusted') base = 'heap:trusted';
  else if (kind === 'embedder') base = 'embedder';
  return isPath ? `via ${base}` : base;
}

function formatTaintTitle(taint, isPath = false) {
  const kind = normalizeTaintKind(taint);
  if (kind === 'trusted') {
    return isPath
      ? 'Flow path to target passes through a V8 Trusted Space Heap Read'
      : 'Tainted by V8 Trusted Space Heap Read';
  }
  if (kind === 'embedder') {
    return isPath
      ? 'Flow path to target originates from an Embedder-Provided Public API Value (include/)'
      : 'Tainted by Embedder-Provided Public API Value (include/)';
  }
  return isPath
    ? 'Flow path to target passes through a V8 In-Sandbox Heap Read (Untrusted)'
    : 'Tainted by V8 In-Sandbox Heap Read (Untrusted)';
}

function matchesLeafTaintFilter(tnode, taintFilter) {
  if (!taintFilter || taintFilter === 'all') return true;
  const own = normalizeTaintKind(tnode.heap_taint);
  const path = normalizeTaintKind(tnode.path_taint || tnode.heap_taint);
  switch (taintFilter) {
    case 'leaf-tainted':
      return Boolean(own);
    case 'leaf-sandbox':
      return own === 'sandbox';
    case 'leaf-embedder':
      return own === 'embedder';
    case 'leaf-trusted':
      return own === 'trusted';
    case 'path-tainted':
      return Boolean(path);
    case 'path-sandbox':
      return path === 'sandbox';
    case 'path-embedder':
      return path === 'embedder';
    case 'path-trusted':
      return path === 'trusted';
    case 'untainted-path':
      return !own && !path;
    case 'untainted-leaf':
      return !own;
    default:
      return true;
  }
}

function matchesLeafRangeFilter(tnode, rangeFilter, rootRange) {
  if (!rangeFilter || rangeFilter === 'all') return true;
  const contrib = tnode.contributed_range || tnode.value_range || null;
  const clobbered = Boolean(tnode.clobbered_by_heap);
  const isDead = Boolean(tnode.is_dead);
  switch (rangeFilter) {
    case 'live':
      return !isDead;
    case 'dead':
      return isDead;
    case 'unclobbered':
      return !clobbered && !isDead;
    case 'bounds':
      return (
        !clobbered &&
        Boolean(contrib) &&
        (contrib.min !== null || contrib.max !== null)
      );
    case 'max-bound':
      if (clobbered || !contrib) return false;
      if (rootRange && rootRange.max !== null && rootRange.max !== undefined) {
        return contrib.max === rootRange.max;
      }
      return contrib.max === null;
    case 'min-bound':
      if (clobbered || !contrib) return false;
      if (rootRange && rootRange.min !== null && rootRange.min !== undefined) {
        return contrib.min === rootRange.min;
      }
      return contrib.min === null;
    case 'interval':
      return (
        !clobbered &&
        Boolean(contrib) &&
        (contrib.min !== null || contrib.max !== null) &&
        contrib.min !== contrib.max
      );
    case 'constant':
      return (
        !clobbered &&
        Boolean(contrib) &&
        contrib.min !== null &&
        contrib.min === contrib.max
      );
    case 'clobbered':
      return clobbered;
    case 'none':
      return !contrib && !tnode.value_range;
    default:
      return true;
  }
}

// Render Hierarchical Tree View
function renderTreeView() {
  const container = document.getElementById('flow-tree-root');
  const summaryEl = document.getElementById('tree-summary-label');
  container.innerHTML = '';

  if (!state.queryData || !state.queryData.root) {
    summaryEl.textContent = 'No Active Trace';
    return;
  }

  const filterText = (document.getElementById('tree-filter-input').value || '').trim().toLowerCase();
  const taintFilter = state.taintFilter || 'all';
  const rangeFilter = state.rangeFilter || 'all';
  const hasLeafFilter = taintFilter !== 'all' || rangeFilter !== 'all';
  const hasAnyFilter = Boolean(filterText) || hasLeafFilter;
  const leavesOnly = document.getElementById('leaves-only-checkbox').checked;
  const activePathSet = new Set(state.activePath);
  const rootRange = state.queryData.root.value_range || null;
  const isFromLeaves =
    state.queryData.method === 'query/from-leaves' ||
    state.queryData.root.canonical_id === '0';

  if (leavesOnly) {
    const leaves = state.queryData.terminalLeaves || [];
    let matchedCount = 0;
    for (const lf of leaves) {
      const tnode = state.treeById.get(lf.tree_id) || lf;
      if (!matchesLeafTaintFilter(tnode, taintFilter)) continue;
      if (!matchesLeafRangeFilter(tnode, rangeFilter, rootRange)) continue;
      if (filterText && !matchesTreeFilter(tnode, filterText)) continue;
      matchedCount++;
      if (state.treeById.has(lf.tree_id)) {
        container.appendChild(createTreeRowDOM(state.treeById.get(lf.tree_id), activePathSet, false));
      }
    }
    summaryEl.textContent = hasAnyFilter
      ? `Terminal Leaves (${matchedCount} / ${leaves.length})`
      : `Terminal Leaves (${leaves.length})`;
    return;
  }

  function subtreeMatchesLeafAndTextFilters(tnode, ancestorTextMatched = false) {
    const selfTextMatched = filterText ? matchesTreeFilter(tnode, filterText) : true;
    if (isFromLeaves) {
      if (tnode.depth === 1) {
        if (!matchesLeafTaintFilter(tnode, taintFilter)) return false;
        if (!matchesLeafRangeFilter(tnode, rangeFilter, rootRange)) return false;
        if (!filterText) return true;
        return subtreeMatchesFilter(tnode, filterText);
      }
      if (tnode.depth >= 2) {
        if (!filterText || ancestorTextMatched || selfTextMatched) return true;
        return subtreeMatchesFilter(tnode, filterText);
      }
      return true;
    }

    // For query/incoming or query/outgoing: check if any terminal node in subtree matches
    const children = tnode.children || [];
    if (children.length === 0) {
      if (hasLeafFilter) {
        if (!matchesLeafTaintFilter(tnode, taintFilter)) return false;
        if (!matchesLeafRangeFilter(tnode, rangeFilter, rootRange)) return false;
      }
      return !filterText || ancestorTextMatched || selfTextMatched;
    }
    for (const c of children) {
      if (subtreeMatchesLeafAndTextFilters(c, ancestorTextMatched || selfTextMatched)) {
        return true;
      }
    }
    return !hasLeafFilter && Boolean(filterText) && selfTextMatched;
  }

  const m = state.queryData.metrics;
  const rootChildren = getEffectiveTreeChildren(state.queryData.root);
  if (isFromLeaves && hasAnyFilter) {
    const matchedRootLeaves = rootChildren.filter((c) =>
      subtreeMatchesLeafAndTextFilters(c, false)
    ).length;
    summaryEl.textContent = `Tree (${matchedRootLeaves} / ${m.terminalLeavesCount} leaves · ${m.totalTreeNodes.toLocaleString()} nodes)`;
  } else {
    summaryEl.textContent = `Tree (${m.totalTreeNodes.toLocaleString()} nodes · ${m.terminalLeavesCount} leaves)`;
  }

  function buildSubtreeDOM(tnode, ancestorCanonicalIds = new Set(), ancestorTextMatched = false) {
    const selfTextMatched = filterText ? matchesTreeFilter(tnode, filterText) : false;
    if (hasAnyFilter && !subtreeMatchesLeafAndTextFilters(tnode, ancestorTextMatched)) {
      return null;
    }
    const wrapper = document.createElement('div');
    const canExpandCached =
      !tnode.is_cached || !ancestorCanonicalIds.has(tnode.canonical_id);
    const allChildren = canExpandCached ? getEffectiveTreeChildren(tnode) : [];
    const nextTextMatched = ancestorTextMatched || selfTextMatched;
    const matchingChildren = hasAnyFilter
      ? allChildren.filter((c) => subtreeMatchesLeafAndTextFilters(c, nextTextMatched))
      : allChildren;
    const hasChildren = matchingChildren.length > 0;
    const isCollapsed =
      state.collapsedTreeIds.has(tnode.tree_id) &&
      (!filterText || tnode.is_cached);

    const row = createTreeRowDOM(tnode, activePathSet, hasChildren, isCollapsed, () => {
      if (state.collapsedTreeIds.has(tnode.tree_id)) {
        state.collapsedTreeIds.delete(tnode.tree_id);
      } else {
        state.collapsedTreeIds.add(tnode.tree_id);
      }
      renderTreeView();
    });
    wrapper.appendChild(row);

    if (hasChildren && !isCollapsed) {
      const nextAncestors = new Set(ancestorCanonicalIds);
      if (tnode.canonical_id) {
        nextAncestors.add(tnode.canonical_id);
      }
      const childContainer = document.createElement('div');
      childContainer.className = 'tree-children';

      const defaultLimit = hasAnyFilter ? 100 : 40;
      const limit = state.expandedBranchLimits.get(tnode.tree_id) || defaultLimit;
      const slice = matchingChildren.slice(0, limit);
      const renderedIds = new Set();
      for (const c of slice) {
        const sub = buildSubtreeDOM(c, nextAncestors, nextTextMatched);
        if (sub) {
          childContainer.appendChild(sub);
          renderedIds.add(c.tree_id);
        }
      }
      // Always render any branch on the currently selected active path even if its index >= limit
      if (matchingChildren.length > limit) {
        for (let i = limit; i < matchingChildren.length; i++) {
          const c = matchingChildren[i];
          if (activePathSet.has(c.tree_id) && !renderedIds.has(c.tree_id)) {
            const sub = buildSubtreeDOM(c, nextAncestors, nextTextMatched);
            if (sub) {
              childContainer.appendChild(sub);
              renderedIds.add(c.tree_id);
            }
          }
        }
      }
      const remaining = matchingChildren.length - renderedIds.size;
      if (remaining > 0) {
        const more = document.createElement('div');
        more.className = 'tree-node-row tree-more-branches-row';
        more.textContent = `▸ … +${remaining} more branches (click to expand)`;
        more.title = `Click to show all ${matchingChildren.length} branches`;
        more.addEventListener('click', (e) => {
          e.stopPropagation();
          const nextLimit =
            remaining <= 600 ? matchingChildren.length : limit + 500;
          state.expandedBranchLimits.set(tnode.tree_id, nextLimit);
          renderTreeView();
        });
        childContainer.appendChild(more);
      }
      wrapper.appendChild(childContainer);
    }
    return wrapper;
  }

  const treeDOM = buildSubtreeDOM(state.queryData.root);
  if (treeDOM) container.appendChild(treeDOM);
}

function matchesTreeFilter(tnode, q) {
  const n = tnode.node || {};
  const loc = getEffectiveLoc(tnode);
  const vr = (tnode.value_range && tnode.value_range.display) || '';
  const cr = (tnode.contributed_range && tnode.contributed_range.display) || '';
  const ht = tnode.heap_taint || '';
  const pt = tnode.path_taint || '';
  return (
    (n.name || '').toLowerCase().includes(q) ||
    (n.type || '').toLowerCase().includes(q) ||
    vr.toLowerCase().includes(q) ||
    cr.toLowerCase().includes(q) ||
    ht.toLowerCase().includes(q) ||
    pt.toLowerCase().includes(q) ||
    (tnode.is_dead && 'dead'.includes(q)) ||
    (loc.file || '').toLowerCase().includes(q) ||
    (tnode.flow_kind || '').toLowerCase().includes(q)
  );
}

function subtreeMatchesFilter(tnode, q) {
  if (matchesTreeFilter(tnode, q)) return true;
  for (const c of tnode.children || []) {
    if (subtreeMatchesFilter(c, q)) return true;
  }
  return false;
}

function createTreeRowDOM(tnode, activePathSet, hasChildren, isCollapsed = false, onToggleCollapse = null) {
  const n = tnode.node || {};
  const loc = getEffectiveLoc(tnode);
  const row = document.createElement('div');
  row.dataset.treeId = tnode.tree_id;
  row.className = [
    'tree-node-row',
    activePathSet.has(tnode.tree_id) ? 'on-active-path' : '',
    tnode.tree_id === state.selectedTreeId ? 'selected' : '',
  ]
    .filter(Boolean)
    .join(' ');

  const twisty = document.createElement('span');
  twisty.className = 'tree-twisty';
  twisty.textContent = hasChildren ? (isCollapsed ? '▶' : '▼') : '•';
  if (hasChildren && onToggleCollapse) {
    twisty.addEventListener('click', (e) => {
      e.stopPropagation();
      onToggleCollapse();
    });
  }
  row.appendChild(twisty);

  const kind = n.kind || 'Expression';
  const badge = document.createElement('span');
  badge.className = `node-kind-badge kind-${kind}`;
  badge.textContent = kind.slice(0, 4);
  row.appendChild(badge);

  const nameSpan = document.createElement('span');
  nameSpan.className = 'tree-symbol-name';
  nameSpan.textContent = (tnode.is_cycle ? '[Cycle] ' : '') + (n.name || 'expr');
  row.appendChild(nameSpan);

  if (tnode.is_dead) {
    const dpill = document.createElement('span');
    dpill.className = 'dead-node-pill';
    dpill.title = 'Dead parameter / uncalled function (no incoming call sites in index)';
    dpill.textContent = 'DEAD';
    row.appendChild(dpill);
  }

  if (tnode.value_range && tnode.value_range.display) {
    const rpill = document.createElement('span');
    rpill.className = 'value-range-pill';
    rpill.title = `Estimated Integral Value Range: ${tnode.value_range.display}`;
    rpill.textContent = tnode.value_range.display;
    row.appendChild(rpill);
  }

  const ownDisplay = (tnode.value_range && tnode.value_range.display) || '';
  const contribDisplay = (tnode.contributed_range && tnode.contributed_range.display) || '';
  if (tnode.clobbered_by_heap) {
    const cpill = document.createElement('span');
    cpill.className = 'contributed-range-pill clobbered';
    cpill.title = `Value range is overwritten by an intermediate heap-tainted load/field before reaching target${contribDisplay ? ' (target range: ' + contribDisplay + ')' : ''}`;
    cpill.textContent = '→ clobbered';
    row.appendChild(cpill);
  } else if (contribDisplay && contribDisplay !== ownDisplay) {
    const cpill = document.createElement('span');
    cpill.className = 'contributed-range-pill';
    cpill.title = `Contributed Value Range at Target: ${contribDisplay}`;
    cpill.textContent = `→ ${contribDisplay}`;
    row.appendChild(cpill);
  }

  if (tnode.heap_taint) {
    const tkind = normalizeTaintKind(tnode.heap_taint);
    const tpill = document.createElement('span');
    tpill.className = `heap-taint-pill taint-${tkind}`;
    tpill.title = formatTaintTitle(tkind, false);
    tpill.textContent = formatTaintLabel(tkind, false);
    row.appendChild(tpill);
  } else if (tnode.path_taint) {
    const pkind = normalizeTaintKind(tnode.path_taint);
    const ppill = document.createElement('span');
    ppill.className = `heap-taint-pill path-taint taint-${pkind}`;
    ppill.title = formatTaintTitle(pkind, true);
    ppill.textContent = formatTaintLabel(pkind, true);
    row.appendChild(ppill);
  }

  if (tnode.flow_kind && tnode.flow_kind !== 'Root') {
    const fpill = document.createElement('span');
    fpill.className = 'flow-kind-pill';
    fpill.textContent = tnode.flow_kind;
    row.appendChild(fpill);
  }

  if (n.type) {
    const typeSpan = document.createElement('span');
    typeSpan.className = 'tree-symbol-type';
    typeSpan.title = n.type;
    typeSpan.textContent = n.type;
    row.appendChild(typeSpan);
  }

  if (loc.file && loc.line > 0) {
    const locSpan = document.createElement('span');
    locSpan.className = 'tree-loc-chip';
    locSpan.textContent = `${basename(loc.file)}:${loc.line}`;
    row.appendChild(locSpan);
  }

  row.addEventListener('click', () => {
    selectTreeNode(tnode.tree_id);
  });

  return row;
}

function updateTreeSelectionHighlights() {
  const activePathSet = new Set(state.activePath);
  document.querySelectorAll('#flow-tree-root .tree-node-row').forEach((row) => {
    const tid = row.dataset.treeId;
    if (!tid) return;
    row.classList.toggle('on-active-path', activePathSet.has(tid));
    row.classList.toggle('selected', tid === state.selectedTreeId);
  });
}

// Render Layered SVG Directed Acyclic Graph (DAG) View
function renderDagGraph() {
  const viewport = document.getElementById('dag-viewport');
  const summaryChip = document.getElementById('graph-summary-chip');
  viewport.innerHTML = '';

  if (!state.queryData || !state.queryData.dag) {
    summaryChip.textContent = 'Directed Dataflow Graph (DAG)';
    return;
  }

  const { nodes, edges } = state.queryData.dag;
  const m = state.queryData.metrics;
  summaryChip.textContent = `DAG: ${nodes.length} unique nodes · ${edges.length} edges${m.dagTruncated ? ' (top neighborhood)' : ''}`;

  if (!nodes.length) return;

  // Compute Layered Layout (TD or LR)
  const isLR = state.dagOrientation === 'LR';
  const nodeW = 210;
  const nodeH = 54;
  const rankGap = isLR ? 265 : 92;
  const siblingGap = isLR ? 72 : 228;

  // Group nodes by depth layer
  const layers = new Map();
  let maxDepth = 0;
  const nodeMap = new Map();
  for (const n of nodes) {
    nodeMap.set(n.id, n);
    const d = n.depth || 0;
    if (d > maxDepth) maxDepth = d;
    if (!layers.has(d)) layers.set(d, []);
    layers.get(d).push(n);
  }

  // Build incoming adjacency for barycenter ordering
  const incomingMap = new Map();
  for (const e of edges) {
    if (!incomingMap.has(e.target)) incomingMap.set(e.target, []);
    incomingMap.get(e.target).push(e.source);
  }

  // Assign coordinates layer by layer with barycenter sorting to minimize crossings
  const coords = new Map();
  for (let d = 0; d <= maxDepth; d++) {
    const layerNodes = layers.get(d) || [];
    if (d > 0) {
      layerNodes.sort((a, b) => {
        const pa = incomingMap.get(a.id) || [];
        const pb = incomingMap.get(b.id) || [];
        const avgA = pa.length
          ? pa.reduce((acc, pid) => acc + ((coords.get(pid) || {}).order || 0), 0) / pa.length
          : 0;
        const avgB = pb.length
          ? pb.reduce((acc, pid) => acc + ((coords.get(pid) || {}).order || 0), 0) / pb.length
          : 0;
        return avgA - avgB;
      });
    }

    const totalSpan = (layerNodes.length - 1) * siblingGap;
    layerNodes.forEach((n, idx) => {
      const offset = idx * siblingGap - totalSpan / 2;
      if (isLR) {
        coords.set(n.id, { x: d * rankGap, y: offset, order: idx });
      } else {
        coords.set(n.id, { x: offset, y: d * rankGap, order: idx });
      }
    });
  }

  // Normalize coordinates so minX/minY start at (20, 20)
  let minX = Infinity,
    minY = Infinity,
    maxX = -Infinity,
    maxY = -Infinity;
  for (const c of coords.values()) {
    if (c.x < minX) minX = c.x;
    if (c.y < minY) minY = c.y;
    if (c.x + nodeW > maxX) maxX = c.x + nodeW;
    if (c.y + nodeH > maxY) maxY = c.y + nodeH;
  }
  for (const c of coords.values()) {
    c.x = c.x - minX + 24;
    c.y = c.y - minY + 40;
  }
  state.dagBounds = {
    width: Math.max(400, maxX - minX + 60),
    height: Math.max(300, maxY - minY + 80),
  };

  // Determine active canonical IDs along state.activePath
  const activeCanonicalList = state.activePath
    .map((tid) => {
      const tn = state.treeById.get(tid);
      return tn ? tn.canonical_id : null;
    })
    .filter(Boolean);
  const activeCanonicalSet = new Set(activeCanonicalList);
  const activeEdgeSet = new Set();
  for (let i = 0; i < activeCanonicalList.length - 1; i++) {
    activeEdgeSet.add(`${activeCanonicalList[i]}->${activeCanonicalList[i + 1]}`);
  }
  const selectedTNode = state.treeById.get(state.selectedTreeId);
  const selectedCanonicalId = selectedTNode ? selectedTNode.canonical_id : null;

  const ns = 'http://www.w3.org/2000/svg';

  // Render Edges first
  for (const e of edges) {
    const src = coords.get(e.source);
    const dst = coords.get(e.target);
    if (!src || !dst) continue;

    const isEdgeActive = activeEdgeSet.has(e.id);
    const path = document.createElementNS(ns, 'path');
    path.dataset.edgeId = e.id;
    path.setAttribute('class', `dag-edge ${isEdgeActive ? 'active-path' : ''}`);
    path.setAttribute('marker-end', isEdgeActive ? 'url(#arrow-active)' : 'url(#arrow-default)');

    let dStr;
    if (isLR) {
      const x1 = src.x + nodeW;
      const y1 = src.y + nodeH / 2;
      const x2 = dst.x;
      const y2 = dst.y + nodeH / 2;
      const cx = (x1 + x2) / 2;
      dStr = `M ${x1} ${y1} C ${cx} ${y1}, ${cx} ${y2}, ${x2} ${y2}`;
    } else {
      const x1 = src.x + nodeW / 2;
      const y1 = src.y + nodeH;
      const x2 = dst.x + nodeW / 2;
      const y2 = dst.y;
      const cy = (y1 + y2) / 2;
      dStr = `M ${x1} ${y1} C ${x1} ${cy}, ${x2} ${cy}, ${x2} ${y2}`;
    }
    path.setAttribute('d', dStr);
    viewport.appendChild(path);
  }

  // Render Nodes
  for (const n of nodes) {
    const c = coords.get(n.id);
    if (!c) continue;

    const g = document.createElementNS(ns, 'g');
    g.dataset.canonicalId = n.id;
    g.setAttribute(
      'class',
      [
        'dag-node-group',
        activeCanonicalSet.has(n.id) ? 'on-active-path' : '',
        n.id === selectedCanonicalId ? 'selected-node' : '',
        n.is_dead ? 'dead-dag-node' : '',
      ]
        .filter(Boolean)
        .join(' ')
    );
    g.setAttribute('transform', `translate(${c.x}, ${c.y})`);

    const rect = document.createElementNS(ns, 'rect');
    rect.setAttribute('class', 'dag-node-rect');
    rect.setAttribute('width', String(nodeW));
    rect.setAttribute('height', String(nodeH));
    g.appendChild(rect);

    // Top row: [Kind] SymbolName
    const titleText = document.createElementNS(ns, 'text');
    titleText.setAttribute('class', 'dag-node-title');
    titleText.setAttribute('x', '10');
    titleText.setAttribute('y', '20');
    titleText.setAttribute('font-family', "'JetBrains Mono', monospace");
    titleText.setAttribute('font-size', '11.5');
    titleText.setAttribute('font-weight', '600');
    const shortName = (n.name || 'expr').length > 22 ? n.name.slice(0, 21) + '…' : n.name || 'expr';
    titleText.textContent = `[${(n.kind || 'Expr').slice(0, 3)}]${n.is_dead ? ' [DEAD]' : ''} ${shortName}`;
    g.appendChild(titleText);

    // Bottom row: file:line · flow_kind
    const subText = document.createElementNS(ns, 'text');
    subText.setAttribute('class', 'dag-node-sub');
    subText.setAttribute('x', '10');
    subText.setAttribute('y', '39');
    subText.setAttribute('font-family', "'JetBrains Mono', monospace");
    subText.setAttribute('font-size', '10.5');
    const locLabel = n.file ? `${basename(n.file)}:${n.line}` : 'root';
    const rangeTag = n.value_range && n.value_range.display ? ` · ${n.value_range.display}` : '';
    const taintTag = n.heap_taint
      ? ` · ${formatTaintLabel(n.heap_taint, false)}`
      : '';
    subText.textContent = `${locLabel} · ${n.flow_kind}${rangeTag}${taintTag}`;
    g.appendChild(subText);

    g.addEventListener('click', (ev) => {
      ev.stopPropagation();
      // Prefer a tree_id on the current active path if available, otherwise first_tree_id
      let targetTid = n.first_tree_id;
      for (const tid of n.tree_ids || []) {
        if (state.activePath.includes(tid)) {
          targetTid = tid;
          break;
        }
      }
      selectTreeNode(targetTid);
    });

    viewport.appendChild(g);
  }

  fitDagToViewport();
}

function updateDagSelectionHighlights() {
  const activeCanonicalList = state.activePath
    .map((tid) => {
      const tn = state.treeById.get(tid);
      return tn ? tn.canonical_id : null;
    })
    .filter(Boolean);
  const activeCanonicalSet = new Set(activeCanonicalList);
  const activeEdgeSet = new Set();
  for (let i = 0; i < activeCanonicalList.length - 1; i++) {
    activeEdgeSet.add(`${activeCanonicalList[i]}->${activeCanonicalList[i + 1]}`);
  }
  const selectedTNode = state.treeById.get(state.selectedTreeId);
  const selectedCanonicalId = selectedTNode ? selectedTNode.canonical_id : null;

  document.querySelectorAll('#dag-viewport .dag-node-group').forEach((g) => {
    const cid = g.dataset.canonicalId;
    g.classList.toggle('on-active-path', activeCanonicalSet.has(cid));
    g.classList.toggle('selected-node', cid === selectedCanonicalId);
  });

  document.querySelectorAll('#dag-viewport .dag-edge').forEach((p) => {
    const eid = p.dataset.edgeId;
    const isAct = activeEdgeSet.has(eid);
    p.classList.toggle('active-path', isAct);
    p.setAttribute('marker-end', isAct ? 'url(#arrow-active)' : 'url(#arrow-default)');
  });
}

function applyDagTransform() {
  const vp = document.getElementById('dag-viewport');
  const { x, y, scale } = state.dagTransform;
  vp.setAttribute('transform', `translate(${x}, ${y}) scale(${scale})`);
}

function fitDagToViewport() {
  const svg = document.getElementById('dag-svg');
  const rect = svg.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0) return;

  const pad = 28;
  const scaleX = (rect.width - pad * 2) / Math.max(1, state.dagBounds.width);
  const scaleY = (rect.height - pad * 2) / Math.max(1, state.dagBounds.height);
  const scale = Math.max(0.22, Math.min(1.15, Math.min(scaleX, scaleY)));

  state.dagTransform = {
    x: Math.max(16, (rect.width - state.dagBounds.width * scale) / 2),
    y: Math.max(28, (rect.height - state.dagBounds.height * scale) / 2),
    scale,
  };
  applyDagTransform();
}

// Bind All Interactive UI Events
function bindUIControls() {
  const themeBtn = document.getElementById('btn-theme-toggle');
  if (themeBtn) {
    themeBtn.addEventListener('click', () => {
      applyTheme(state.theme === 'light' ? 'dark' : 'light');
    });
  }

  // Preset selector
  document.getElementById('preset-select').addEventListener('change', async (e) => {
    const id = e.target.value;
    const preset = state.presets.find((p) => p.id === id);
    if (preset) {
      await loadPreset(preset);
    }
  });

  // Query Mode Buttons
  document.querySelectorAll('.mode-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const method = btn.dataset.method;
      setQueryMethodUI(method);
      if (state.queryData && state.queryData.resolved) {
        const r = state.queryData.resolved;
        runDataflowQuery(method, r.file, r.line, r.column);
      }
    });
  });

  // Prune Cycles Toggle
  document.getElementById('prune-cycles-checkbox').addEventListener('change', (e) => {
    state.pruneCycles = e.target.checked;
    if (state.queryData && state.queryData.resolved) {
      const r = state.queryData.resolved;
      runDataflowQuery(state.currentMethod, r.file, r.line, r.column);
    }
  });

  // Stepper Prev/Next & Keyboard '[' and ']'
  document.getElementById('btn-prev-step').addEventListener('click', () => stepActivePath(-1));
  document.getElementById('btn-next-step').addEventListener('click', () => stepActivePath(1));
  window.addEventListener('keydown', (e) => {
    if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) return;
    if (e.key === '[') {
      e.preventDefault();
      stepActivePath(-1);
    } else if (e.key === ']') {
      e.preventDefault();
      stepActivePath(1);
    } else if (e.key === '/') {
      e.preventDefault();
      document.getElementById('omnibox-input').focus();
    }
  });

  // Horizontal mouse-wheel scrolling on editor file tabs bar
  const fileTabsBar = document.getElementById('file-tabs-bar');
  if (fileTabsBar) {
    fileTabsBar.addEventListener(
      'wheel',
      (e) => {
        if (Math.abs(e.deltaY) > Math.abs(e.deltaX)) {
          e.preventDefault();
          fileTabsBar.scrollLeft += e.deltaY;
        }
      },
      { passive: false }
    );
  }

  // Bottom Cursor Inspector Action Buttons
  document.getElementById('btn-trace-origins-here').addEventListener('click', () => {
    if (!state.currentCursorNode) return;
    pinFileTab(state.currentCursorNode.file);
    setQueryMethodUI('query/from-leaves');
    runDataflowQuery(
      'query/from-leaves',
      state.currentCursorNode.file,
      state.currentCursorNode.line,
      state.currentCursorNode.column
    );
  });

  document.getElementById('btn-trace-incoming-here').addEventListener('click', () => {
    if (!state.currentCursorNode) return;
    pinFileTab(state.currentCursorNode.file);
    setQueryMethodUI('query/incoming');
    runDataflowQuery(
      'query/incoming',
      state.currentCursorNode.file,
      state.currentCursorNode.line,
      state.currentCursorNode.column
    );
  });

  document.getElementById('btn-trace-outgoing-here').addEventListener('click', () => {
    if (!state.currentCursorNode) return;
    pinFileTab(state.currentCursorNode.file);
    setQueryMethodUI('query/outgoing');
    runDataflowQuery(
      'query/outgoing',
      state.currentCursorNode.file,
      state.currentCursorNode.line,
      state.currentCursorNode.column
    );
  });

  // Browser History Back / Forward (popstate)
  window.addEventListener('popstate', (e) => {
    restoreStateFromUrlOrHistory(e.state || null, false);
  });

  // View Mode Tabs (Split | Graph | Tree)
  document.querySelectorAll('.view-tab').forEach((tab) => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.view-tab').forEach((t) => t.classList.remove('active'));
      tab.classList.add('active');
      const v = tab.dataset.view;
      state.viewMode = v;
      const graphPanel = document.getElementById('graph-view-panel');
      const treePanel = document.getElementById('tree-view-panel');
      graphPanel.classList.toggle('hidden', v === 'tree');
      treePanel.classList.toggle('hidden', v === 'graph');
      if (v !== 'tree') {
        setTimeout(() => fitDagToViewport(), 30);
      }
      syncBrowserHistory();
    });
  });

  // Tree filter & Leaves-only toggle
  document.getElementById('tree-filter-input').addEventListener('input', () => renderTreeView());
  document.getElementById('leaves-only-checkbox').addEventListener('change', () => renderTreeView());
  const taintFilterEl = document.getElementById('taint-filter-select');
  if (taintFilterEl) {
    taintFilterEl.addEventListener('change', (e) => {
      setFilterSelectsUI(e.target.value, state.rangeFilter);
      renderTreeView();
      syncBrowserHistory({ replace: true });
    });
  }
  const rangeFilterEl = document.getElementById('range-filter-select');
  if (rangeFilterEl) {
    rangeFilterEl.addEventListener('change', (e) => {
      setFilterSelectsUI(state.taintFilter, e.target.value);
      renderTreeView();
      syncBrowserHistory({ replace: true });
    });
  }
  document.getElementById('btn-expand-all').addEventListener('click', () => {
    state.collapsedTreeIds.clear();
    // Keep secondary `is_cached` DAG reconvergence references collapsed by
    // default so Expand All expands all primary tree branches in O(N) without
    // exploding shared diamond subtrees in O(2^D).
    for (const tnode of state.treeById.values()) {
      if (tnode.is_cached) {
        state.collapsedTreeIds.add(tnode.tree_id);
      } else if ((tnode.children || []).length > 40) {
        state.expandedBranchLimits.set(tnode.tree_id, tnode.children.length);
      }
    }
    renderTreeView();
  });
  document.getElementById('btn-collapse-all').addEventListener('click', () => {
    state.expandedBranchLimits.clear();
    for (const tnode of state.treeById.values()) {
      if (tnode.depth >= 2 && (tnode.children || []).length > 0) {
        state.collapsedTreeIds.add(tnode.tree_id);
      }
    }
    renderTreeView();
  });

  // Graph Orientation & Zoom/Pan Controls
  document.getElementById('btn-graph-orientation').addEventListener('click', (e) => {
    state.dagOrientation = state.dagOrientation === 'TD' ? 'LR' : 'TD';
    e.target.textContent = `Layout: ${state.dagOrientation}`;
    renderDagGraph();
  });
  const svg = document.getElementById('dag-svg');

  function zoomDagAtPoint(newScale, focalX, focalY) {
    const oldScale = state.dagTransform.scale;
    const clampedScale = Math.max(0.15, Math.min(2.5, newScale));
    if (clampedScale === oldScale) return;
    const ratio = clampedScale / oldScale;
    state.dagTransform.x = focalX - (focalX - state.dagTransform.x) * ratio;
    state.dagTransform.y = focalY - (focalY - state.dagTransform.y) * ratio;
    state.dagTransform.scale = clampedScale;
    applyDagTransform();
  }

  document.getElementById('btn-graph-zoom-in').addEventListener('click', () => {
    const rect = svg.getBoundingClientRect();
    zoomDagAtPoint(state.dagTransform.scale * 1.25, rect.width / 2, rect.height / 2);
  });
  document.getElementById('btn-graph-zoom-out').addEventListener('click', () => {
    const rect = svg.getBoundingClientRect();
    zoomDagAtPoint(state.dagTransform.scale / 1.25, rect.width / 2, rect.height / 2);
  });
  document.getElementById('btn-graph-fit').addEventListener('click', () => fitDagToViewport());

  let isDragging = false;
  let startX = 0,
    startY = 0;
  svg.addEventListener('mousedown', (e) => {
    isDragging = true;
    startX = e.clientX - state.dagTransform.x;
    startY = e.clientY - state.dagTransform.y;
  });
  window.addEventListener('mousemove', (e) => {
    if (!isDragging) return;
    state.dagTransform.x = e.clientX - startX;
    state.dagTransform.y = e.clientY - startY;
    applyDagTransform();
  });
  window.addEventListener('mouseup', () => {
    isDragging = false;
  });
  svg.addEventListener(
    'wheel',
    (e) => {
      e.preventDefault();
      const rect = svg.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;
      const factor = e.deltaY < 0 ? 1.12 : 0.89;
      zoomDagAtPoint(state.dagTransform.scale * factor, mouseX, mouseY);
      if (isDragging) {
        startX = e.clientX - state.dagTransform.x;
        startY = e.clientY - state.dagTransform.y;
      }
    },
    { passive: false }
  );

  // Omnibox File & Symbol Search
  const omniInput = document.getElementById('omnibox-input');
  const omniDrop = document.getElementById('omnibox-dropdown');
  let omniTimer = null;

  omniInput.addEventListener('input', () => {
    clearTimeout(omniTimer);
    const q = omniInput.value.trim();
    if (!q) {
      omniDrop.classList.add('hidden');
      return;
    }
    omniTimer = setTimeout(() => runOmniboxSearch(q), 140);
  });

  document.addEventListener('click', (e) => {
    if (!document.getElementById('omnibox-wrapper').contains(e.target)) {
      omniDrop.classList.add('hidden');
    }
  });
}

async function runOmniboxSearch(q) {
  const omniDrop = document.getElementById('omnibox-dropdown');
  try {
    const [filesRes, symsRes] = await Promise.all([
      fetch(`/api/files?q=${encodeURIComponent(q)}&limit=8`),
      fetch(`/api/symbols?q=${encodeURIComponent(q)}&limit=10`),
    ]);
    const filesData = await filesRes.json();
    const symsData = await symsRes.json();

    omniDrop.innerHTML = '';

    if (filesData.files && filesData.files.length > 0) {
      const hdr = document.createElement('div');
      hdr.className = 'omnibox-section-title';
      hdr.textContent = 'V8 Source Files';
      omniDrop.appendChild(hdr);

      for (const f of filesData.files) {
        const item = document.createElement('div');
        item.className = 'omnibox-item';
        item.innerHTML = `
          <div class="omnibox-item-main">
            <span class="node-kind-badge kind-Literal">File</span>
            <span class="omnibox-item-name">${esc(f.path)}</span>
          </div>
          <span class="omnibox-item-meta">${f.nodeCount.toLocaleString()} DFG nodes</span>
        `;
        item.addEventListener('click', () => {
          omniDrop.classList.add('hidden');
          openFileInEditor(f.path, 0, 0, { pin: true, preserveViewState: true });
        });
        omniDrop.appendChild(item);
      }
    }

    if (symsData.symbols && symsData.symbols.length > 0) {
      const hdr = document.createElement('div');
      hdr.className = 'omnibox-section-title';
      hdr.textContent = 'Indexed DFG Symbols (Click to Trace)';
      omniDrop.appendChild(hdr);

      for (const s of symsData.symbols) {
        const item = document.createElement('div');
        item.className = 'omnibox-item';
        item.innerHTML = `
          <div class="omnibox-item-main">
            <span class="node-kind-badge kind-${esc(s.kind)}">${esc(s.kind.slice(0, 4))}</span>
            <span class="omnibox-item-name">${esc(s.name)}</span>
            <span class="omnibox-item-meta" style="color:var(--accent-cyan)">${esc(s.type)}</span>
          </div>
          <span class="omnibox-item-meta">${esc(basename(s.file))}:${s.line}</span>
        `;
        item.addEventListener('click', async () => {
          omniDrop.classList.add('hidden');
          await openFileInEditor(s.file, s.line, s.column, { pin: true });
          await runDataflowQuery(state.currentMethod, s.file, s.line, s.column);
        });
        omniDrop.appendChild(item);
      }
    }

    omniDrop.classList.remove('hidden');
  } catch (err) {
    console.error('Omnibox search error:', err);
  }
}
