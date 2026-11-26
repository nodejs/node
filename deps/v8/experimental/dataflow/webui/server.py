#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
V8 C++ Dataflow Explorer WebUI Backend Server.
Bridges the C++ `analyzer --server` (CSR mmap query engine) and `.dataflow.db`
(SQLite metadata index) to the Monaco Editor + Interactive Tree/DAG WebUI.
"""

import argparse
from http.server import HTTPServer, SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import mimetypes
import os
import socket
import sqlite3
import subprocess
import sys
import threading
import time
from urllib.parse import parse_qs, urlparse

# NodeKind and EdgeKind mappings matching include/dfg.h
NODE_KINDS = {
    0: "Unknown",
    1: "Variable",
    2: "Parameter",
    3: "Field",
    4: "Literal",
    5: "Return",
    6: "Expression",
}

EDGE_KINDS = {
    0: "Unknown",
    1: "Initialization",
    2: "Assignment",
    3: "ArgPass",
    4: "Return",
    5: "MemberRead",
    6: "MemberWrite",
}

CURATED_PRESETS = [
    {
        "id": "maglev_load_tagged_field_offset",
        "title": "1. MaglevReducer::BuildLoadTaggedField (offset [0, 8188] ← FieldIndex)",
        "subtitle": "Traces uint32_t offset [0, 8188] from PropertyDetails::FieldIndexField * kTaggedSize & object header constants",
        "method": "query/from-leaves",
        "file": "src/maglev/maglev-reducer-inl.h",
        "line": 578,
        "column": 35,
        "symbol": "offset",
    },
    {
        "id": "maglev_reg_frame_param_count",
        "title": "2. RegisterFrameArray::RegisterFrameArray (parameter_count [0, 65535] [trusted])",
        "subtitle": "Traces info.parameter_count() to uint16_t BytecodeArray::parameter_count() in V8 Trusted Space",
        "method": "query/from-leaves",
        "file": "src/maglev/maglev-register-frame-array.h",
        "line": 33,
        "column": 14,
        "symbol": "parameter_count",
    },
    {
        "id": "maglev_catch_handler_offset",
        "title": "3. MaglevGraphBuilder::BuildMergeStates (offset [0, 268435455] ← HandlerTable)",
        "subtitle": "Traces exception handler offset through HandlerTable::GetRangeHandler & 28-bit RangeHandlerBit::decode()",
        "method": "query/from-leaves",
        "file": "src/maglev/maglev-graph-builder.cc",
        "line": 841,
        "column": 17,
        "symbol": "offset",
    },
    {
        "id": "field_index_for_details_offset",
        "title": "4. FieldIndex::ForDetails (offset [0, 8188] ← PropertyDetails)",
        "subtitle": "Traces int offset = details.field_offset() * kTaggedSize with 10-bit FieldIndexField [0, 1023] & sandbox taint",
        "method": "query/from-leaves",
        "file": "src/objects/field-index-inl.h",
        "line": 101,
        "column": 7,
        "symbol": "offset",
    },
    {
        "id": "field_index_smi_load_handler",
        "title": "5. FieldIndex::ForSmiLoadHandler (first_field_offset [0, 1020] ← Map)",
        "subtitle": "Traces in-object property storage offset from uint8_t Map::inobject_properties * kTaggedSize",
        "method": "query/from-leaves",
        "file": "src/objects/field-index-inl.h",
        "line": 32,
        "column": 7,
        "symbol": "first_field_offset_in_storage",
    },
    {
        "id": "code_builtin_id_trusted",
        "title": "6. Code::builtin_id (value [-1, 2527] [trusted] ← Builtin)",
        "subtitle": "Traces int16_t kBuiltinIdOffset field reads/writes on TrustedObject Code across mksnapshot & compiler",
        "method": "query/from-leaves",
        "file": "src/objects/code-inl.h",
        "line": 905,
        "column": 7,
        "symbol": "value",
    },
    {
        "id": "bigint_length_bits",
        "title": "7. BigIntBase::length (LengthBits [0, 16777215] ← bitfield_)",
        "subtitle": "Traces 24-bit BigIntBase::LengthBits::decode() across BigInt allocation & arithmetic bitfield updates",
        "method": "query/from-leaves",
        "file": "src/objects/bigint.h",
        "line": 96,
        "column": 12,
        "symbol": "decode",
    },
    {
        "id": "transitions_simple",
        "title": "8. TransitionsAccessor::Insert (simple_transition [sandbox] ← Map)",
        "subtitle": "Traces Tagged<Map> simple_transition across Map::transitions_or_prototype_info_ store-to-load field unification",
        "method": "query/from-leaves",
        "file": "src/objects/transitions.cc",
        "line": 69,
        "column": 17,
        "symbol": "simple_transition",
    },
    {
        "id": "isolate_throw_outgoing",
        "title": "9. Isolate::Throw (raw_exception → pending exception & debugger sinks)",
        "subtitle": "Traces outgoing Tagged<Object> raw_exception into Isolate::set_pending_exception & Debug::OnThrow",
        "method": "query/outgoing",
        "file": "src/execution/isolate.cc",
        "line": 2745,
        "column": 46,
        "symbol": "raw_exception",
    },
]


def prune_incoming_cycles(node):
    if not node:
        return None
    if node.get("is_cycle"):
        return None
    pruned_children = []
    for child in node.get("children", []):
        pc = prune_incoming_cycles(child)
        if pc is not None:
            pruned_children.append(pc)
    res = {
        "node": node.get("node"),
        "flow_kind": node.get("flow_kind", "Root"),
        "flow_location": node.get("flow_location", {}),
        "children": pruned_children,
        "is_cycle": bool(node.get("is_cycle", False)),
        "is_cached": bool(node.get("is_cached", False)),
        "is_truncated": bool(node.get("is_truncated", False)),
    }
    if "value_range" in node:
        res["value_range"] = node["value_range"]
    if "heap_taint" in node:
        res["heap_taint"] = node["heap_taint"]
    if "contributed_range" in node:
        res["contributed_range"] = node["contributed_range"]
    if "path_taint" in node:
        res["path_taint"] = node["path_taint"]
    if "clobbered_by_heap" in node:
        res["clobbered_by_heap"] = node["clobbered_by_heap"]
    if "is_dead" in node:
        res["is_dead"] = bool(node["is_dead"])
    return res


def build_dag_and_metrics(root, workspace_root, max_dag_nodes=600):
    """
    Walks the query result tree, assigns stable tree_id to every tree node,
    computes summary metrics, and builds a deduplicated DAG (up to max_dag_nodes
    prioritized by BFS distance from root + active leaf paths) for the Graph View.
    """
    if not root:
        return None

    tree_counter = [0]
    total_tree_nodes = [0]
    unique_node_ids = set()
    unique_files = set()
    terminal_leaves = []

    def rel_path(p):
        if not p:
            return ""
        if p.startswith(workspace_root + "/"):
            return p[len(workspace_root) + 1 :]
        return p

    seen_leaf_cids = set()
    root_node_meta = root.get("node") or {}
    is_flipped = (
        str(root_node_meta.get("id", "")) == "0"
        or str(root_node_meta.get("name", "")).startswith("[Flipped]")
    )

    def combine_taint_str(t1, t2):
        if t1 in ("sandbox", "in_sandbox") or t2 in ("sandbox", "in_sandbox"):
            return "sandbox"
        if t1 == "embedder" or t2 == "embedder":
            return "embedder"
        if t1 in ("trusted", "trusted_space") or t2 in ("trusted", "trusted_space"):
            return "trusted"
        return None

    def annotate(
        item,
        depth=0,
        parent_tree_id=None,
        anc_path_taint=None,
        anc_clobbered=False,
        branch_contrib_range=None,
    ):
        tid = f"t_{tree_counter[0]}"
        tree_counter[0] += 1
        total_tree_nodes[0] += 1
        item["tree_id"] = tid
        item["depth"] = depth
        item["parent_tree_id"] = parent_tree_id

        n = item.get("node") or {}
        loc = n.get("location") or {}
        floc = item.get("flow_location") or {}

        if loc.get("file"):
            loc["rel_file"] = rel_path(loc["file"])
            unique_files.add(loc["rel_file"])
        if floc.get("file"):
            floc["rel_file"] = rel_path(floc["file"])
            unique_files.add(floc["rel_file"])

        nid = n.get("id") or f"{n.get('name','')}_{loc.get('rel_file','')}_{loc.get('line',0)}_{loc.get('column',0)}"
        item["canonical_id"] = str(nid)
        if str(nid) != "0":
            unique_node_ids.add(str(nid))

        children = item.get("children") or []
        next_anc_taint = anc_path_taint
        next_anc_clobbered = anc_clobbered
        next_branch_range = branch_contrib_range

        if not is_flipped and depth >= 1:
            if depth == 1:
                next_branch_range = item.get("value_range")
            own_t = item.get("heap_taint")
            ch_taints = {c.get("heap_taint") for c in children if c.get("heap_taint")}
            has_intr_taint = bool(
                own_t
                and (not children or (own_t not in ch_taints and own_t != "embedder"))
            )
            has_intr_sandbox = bool(
                own_t in ("sandbox", "in_sandbox")
                and (not children or own_t not in ch_taints)
            )
            cur_path_taint = combine_taint_str(
                anc_path_taint, own_t if has_intr_taint else None
            )
            if "path_taint" not in item and cur_path_taint:
                item["path_taint"] = cur_path_taint
            if "clobbered_by_heap" not in item and anc_clobbered:
                item["clobbered_by_heap"] = True
            if "contributed_range" not in item and (next_branch_range or item.get("value_range")):
                item["contributed_range"] = next_branch_range or item.get("value_range")
            next_anc_taint = cur_path_taint
            next_anc_clobbered = anc_clobbered or has_intr_sandbox

        for c in children:
            annotate(
                c,
                depth + 1,
                tid,
                next_anc_taint,
                next_anc_clobbered,
                next_branch_range,
            )

        is_terminal_source = (depth == 1) if is_flipped else (
            not children
            and not item.get("is_cached")
            and not item.get("is_cycle")
            and not item.get("is_truncated")
        )
        if is_terminal_source and str(nid) not in seen_leaf_cids and str(nid) != "0":
            seen_leaf_cids.add(str(nid))
            terminal_leaves.append(
                {
                    "tree_id": tid,
                    "canonical_id": str(nid),
                    "name": n.get("name", ""),
                    "kind": n.get("kind", ""),
                    "type": n.get("type", ""),
                    "value_range": item.get("value_range"),
                    "contributed_range": item.get("contributed_range") or item.get("value_range"),
                    "heap_taint": item.get("heap_taint"),
                    "path_taint": item.get("path_taint") or item.get("heap_taint"),
                    "clobbered_by_heap": bool(item.get("clobbered_by_heap", False)),
                    "is_dead": bool(item.get("is_dead", False)),
                    "file": (floc.get("rel_file") if floc.get("line", 0) > 0 else loc.get("rel_file")) or "",
                    "line": (floc.get("line") if floc.get("line", 0) > 0 else loc.get("line")) or 0,
                    "column": (floc.get("column") if floc.get("line", 0) > 0 else loc.get("column")) or 0,
                }
            )

    annotate(root)

    # Build deduplicated DAG via BFS so the core neighborhood around the root
    # and main branches is always represented cleanly
    dag_nodes_map = {}
    dag_edges_map = {}
    from collections import deque

    q = deque(root.get("children") or []) if is_flipped else deque([root])
    while q and len(dag_nodes_map) < max_dag_nodes:
        curr = q.popleft()
        cid = curr["canonical_id"]
        n = curr.get("node") or {}
        loc = n.get("location") or {}
        floc = curr.get("flow_location") or {}

        use_floc = floc.get("line", 0) > 0 and not (is_flipped and n.get("kind") == "Parameter")
        eff_file = (floc.get("rel_file") if use_floc else loc.get("rel_file")) or ""
        eff_abs_file = (floc.get("file") if use_floc else loc.get("file")) or ""
        eff_line = (floc.get("line") if use_floc else loc.get("line")) or 0
        eff_col = (floc.get("column") if use_floc else loc.get("column")) or 0

        has_logical_children = (
            len(curr.get("children") or []) > 0
            or bool(curr.get("is_cached"))
            or bool(curr.get("is_truncated"))
        )

        if cid not in dag_nodes_map:
            dag_nodes_map[cid] = {
                "id": cid,
                "first_tree_id": curr["tree_id"],
                "name": n.get("name", "Unknown"),
                "kind": n.get("kind", "Expression"),
                "type": n.get("type", ""),
                "value_range": curr.get("value_range"),
                "heap_taint": curr.get("heap_taint"),
                "flow_kind": curr.get("flow_kind", "Root"),
                "file": eff_file,
                "abs_file": eff_abs_file,
                "line": eff_line,
                "column": eff_col,
                "decl_file": loc.get("rel_file", ""),
                "decl_line": loc.get("line", 0),
                "decl_column": loc.get("column", 0),
                "is_root": (not has_logical_children) if is_flipped else (curr["depth"] == 0),
                "is_leaf": (curr["depth"] == 1) if is_flipped else (
                    (not has_logical_children) and (not bool(curr.get("is_cycle", False)))
                ),
                "is_cycle": bool(curr.get("is_cycle", False)),
                "is_dead": bool(curr.get("is_dead", False)),
                "depth": curr["depth"],
                "tree_ids": [curr["tree_id"]],
            }
        else:
            if curr["tree_id"] not in dag_nodes_map[cid]["tree_ids"]:
                dag_nodes_map[cid]["tree_ids"].append(curr["tree_id"])
            if has_logical_children and not is_flipped:
                dag_nodes_map[cid]["is_leaf"] = False

        for child in curr.get("children") or []:
            child_cid = child["canonical_id"]
            if child_cid in dag_nodes_map or len(dag_nodes_map) < max_dag_nodes:
                q.append(child)
                edge_key = f"{cid}->{child_cid}"
                if edge_key not in dag_edges_map:
                    c_floc = child.get("flow_location") or {}
                    dag_edges_map[edge_key] = {
                        "id": edge_key,
                        "source": cid,
                        "target": child_cid,
                        "kind": child.get("flow_kind", "Flow"),
                        "file": c_floc.get("rel_file", ""),
                        "line": c_floc.get("line", 0),
                    }

    return {
        "metrics": {
            "totalTreeNodes": total_tree_nodes[0],
            "uniqueNodes": len(unique_node_ids),
            "uniqueFiles": len(unique_files),
            "terminalLeavesCount": len(terminal_leaves),
            "dagNodesCount": len(dag_nodes_map),
            "dagEdgesCount": len(dag_edges_map),
            "dagTruncated": len(unique_node_ids) > len(dag_nodes_map),
        },
        "dag": {
            "nodes": list(dag_nodes_map.values()),
            "edges": list(dag_edges_map.values()),
        },
        "terminalLeaves": terminal_leaves,
    }


class AnalyzerBridge:
    def __init__(self, analyzer_bin, workspace_root, cache_dir):
        self.analyzer_bin = analyzer_bin
        self.workspace_root = os.path.abspath(workspace_root)
        self.cache_dir = os.path.abspath(cache_dir)
        self.db_path = os.path.join(self.cache_dir, ".dataflow.db")
        self.lock = threading.RLock()
        self.req_id = 0
        self.proc = None
        self.status = {
            "initialized": False,
            "workspaceRoot": self.workspace_root,
            "cacheDir": self.cache_dir,
            "nodesCount": 0,
            "edgesCount": 0,
            "filesCount": 0,
            "initTimeMs": 0,
            "gitHead": self._get_git_head(),
        }
        self._start_and_init()

    def _get_git_head(self):
        try:
            out = subprocess.check_output(
                ["git", "-C", self.workspace_root, "log", "-1", "--format=%h (%s)"],
                stderr=subprocess.DEVNULL,
                text=True,
                timeout=3,
            )
            return out.strip()
        except Exception:
            return "unknown"

    def _start_and_init(self):
        print(f"[WebUI] Starting C++ analyzer server: {self.analyzer_bin} --server", flush=True)
        self.proc = subprocess.Popen(
            [self.analyzer_bin, "--server"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        t0 = time.time()
        res = self.rpc(
            "initialize",
            {
                "workspaceRoot": self.workspace_root,
                "cacheDir": self.cache_dir,
                "compileCommandsDir": self.workspace_root,
                "skipIndexing": True,
            },
        )
        dt = (time.time() - t0) * 1000.0
        result_obj = res.get("result") or {}
        conn = self.get_db()
        cur = conn.cursor()
        cur.execute("SELECT COUNT(*) FROM files")
        files_count = cur.fetchone()[0]
        conn.close()

        self.status.update(
            {
                "initialized": result_obj.get("status") == "initialized",
                "nodesCount": result_obj.get("nodesCount", 0),
                "edgesCount": result_obj.get("edgesCount", 0),
                "filesCount": files_count,
                "initTimeMs": round(dt, 1),
            }
        )
        print(
            f"[WebUI] Analyzer initialized in {dt:.1f} ms: "
            f"{self.status['nodesCount']:,} nodes, {self.status['edgesCount']:,} edges, "
            f"{files_count:,} files.",
            flush=True,
        )

    def get_db(self):
        conn = sqlite3.connect(f"file:{self.db_path}?mode=ro", uri=True)
        conn.text_factory = lambda b: b.decode("utf-8", errors="replace")
        conn.row_factory = sqlite3.Row
        return conn

    def rpc(self, method, params):
        with self.lock:
            if self.proc is None or self.proc.poll() is not None:
                self._start_and_init()
            self.req_id += 1
            rid = self.req_id
            payload = json.dumps({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
            self.proc.stdin.write(payload + "\n")
            self.proc.stdin.flush()
            while True:
                line = self.proc.stdout.readline()
                if not line:
                    raise RuntimeError("Analyzer server closed stdout unexpectedly")
                line = line.strip()
                if not line:
                    continue
                parsed = json.loads(line)
                if parsed.get("id") == rid:
                    return parsed

    def _extract_identifier_token(self, rel_file, line, column):
        try:
            abs_path = os.path.join(self.workspace_root, rel_file)
            if not os.path.isfile(abs_path):
                return ""
            with open(abs_path, "r", encoding="utf-8", errors="replace") as f:
                for cur_line, text in enumerate(f, 1):
                    if cur_line == line:
                        line_str = text.rstrip("\r\n")
                        break
                else:
                    return ""
            if not line_str:
                return ""
            idx = min(max(0, column - 1), len(line_str) - 1)
            is_ident = lambda ch: ch.isalnum() or ch == "_"
            if not is_ident(line_str[idx]):
                if idx + 1 < len(line_str) and is_ident(line_str[idx + 1]):
                    idx += 1
                elif idx > 0 and is_ident(line_str[idx - 1]):
                    idx -= 1
                else:
                    return ""
            start = idx
            while start > 0 and is_ident(line_str[start - 1]):
                start -= 1
            end = idx + 1
            while end < len(line_str) and is_ident(line_str[end]):
                end += 1
            return line_str[start:end]
        except Exception:
            return ""

    def resolve_nearest_node(self, rel_file, line, column):
        """
        If the user clicked slightly off the exact start column of a token in Monaco,
        or clicked a variable/parameter/field use site on any line, snap to the closest
        DFG node or use-site edge on that line (or within +-1 line) and return query
        coordinates that preserve same-file use-site line numbers for temporal pruning.
        """
        token_name = self._extract_identifier_token(rel_file, line, column)
        token_call = f"{token_name}()" if token_name else ""
        token_ret = f"{token_name}_return" if token_name else ""
        token_qual_call = f"%::{token_name}()" if token_name else ""

        conn = self.get_db()
        cur = conn.cursor()
        cur.execute("SELECT id FROM files WHERE path = ?", (rel_file,))
        frow = cur.fetchone()
        if not frow:
            conn.close()
            return rel_file, line, column, None

        file_id = frow["id"]
        cur.execute(
            """
            SELECT * FROM (
                SELECT n.id AS id,
                       n.kind AS kind,
                       n.name AS name,
                       COALESCE(t.name, '') AS type_name,
                       n.line AS hit_line,
                       n.column AS hit_col,
                       ? AS decl_file,
                       n.line AS decl_line,
                       n.column AS decl_col
                FROM nodes n
                LEFT JOIN types t ON n.type_id = t.id
                WHERE n.file_id = ?
                  AND n.line BETWEEN ? AND ?
                  AND n.name NOT IN ('', 'expr', 'call()', '_msg')
                UNION ALL
                SELECT n.id AS id,
                       n.kind AS kind,
                       n.name AS name,
                       COALESCE(t.name, '') AS type_name,
                       e.line AS hit_line,
                       e.column AS hit_col,
                       df.path AS decl_file,
                       n.line AS decl_line,
                       n.column AS decl_col
                FROM edges e
                JOIN nodes n ON n.id = e.from_id
                JOIN files df ON df.id = n.file_id
                LEFT JOIN types t ON n.type_id = t.id
                WHERE e.file_id = ?
                  AND e.line BETWEEN ? AND ?
                  AND n.kind IN (1, 2, 3)
                  AND n.name NOT IN ('', 'expr', 'call()', '_msg', 'this')
                UNION ALL
                SELECT n.id AS id,
                       n.kind AS kind,
                       n.name AS name,
                       COALESCE(t.name, '') AS type_name,
                       e.line AS hit_line,
                       e.column AS hit_col,
                       df.path AS decl_file,
                       n.line AS decl_line,
                       n.column AS decl_col
                FROM edges e
                JOIN nodes n ON n.id = e.to_id
                JOIN files df ON df.id = n.file_id
                LEFT JOIN types t ON n.type_id = t.id
                WHERE e.file_id = ?
                  AND e.line BETWEEN ? AND ?
                  AND e.kind IN (1, 2)
                  AND n.kind IN (1, 2, 3)
                  AND n.name NOT IN ('', 'expr', 'call()', '_msg', 'this')
            )
            ORDER BY CASE WHEN ? != '' AND (name IN (?, ?, ?) OR name LIKE ?) THEN 0 ELSE 1 END ASC,
                     ABS(hit_line - ?) ASC,
                     CASE WHEN ? >= hit_col AND ? <= hit_col + LENGTH(name) THEN 0 ELSE 1 END ASC,
                     ABS(hit_col - ?) ASC,
                     CASE WHEN kind IN (1, 2, 3) THEN 0 ELSE 1 END ASC
            LIMIT 1
            """,
            (
                rel_file,
                file_id,
                max(1, line - 1),
                line + 1,
                file_id,
                max(1, line - 1),
                line + 1,
                file_id,
                max(1, line - 1),
                line + 1,
                token_name,
                token_name,
                token_call,
                token_ret,
                token_qual_call,
                line,
                column,
                column,
                column,
            ),
        )
        nrow = cur.fetchone()
        if token_name and (
            not nrow
            or (
                nrow["name"] not in (token_name, token_call, token_ret)
                and not nrow["name"].endswith(f"::{token_call}")
            )
        ):
            cur.execute(
                """
                SELECT n.id AS id,
                       n.kind AS kind,
                       n.name AS name,
                       COALESCE(t.name, '') AS type_name,
                       ? AS hit_line,
                       ? AS hit_col,
                       ? AS decl_file,
                       n.line AS decl_line,
                       n.column AS decl_col
                FROM nodes n
                LEFT JOIN types t ON n.type_id = t.id
                WHERE n.file_id = ?
                  AND n.name IN (?, ?)
                ORDER BY CASE WHEN n.line <= ? THEN 0 ELSE 1 END ASC,
                         ABS(n.line - ?) ASC
                LIMIT 1
                """,
                (line, column, rel_file, file_id, token_name, token_call, line, line),
            )
            by_name = cur.fetchone()
            if by_name:
                nrow = by_name

        conn.close()
        if not nrow:
            return rel_file, line, column, None
        decl_file = nrow["decl_file"] or rel_file
        is_same_file_use_site = (
            decl_file == rel_file
            and line > nrow["decl_line"]
            and token_name != ""
            and token_name == nrow["name"]
        )
        query_file = rel_file if is_same_file_use_site else decl_file
        query_line = line if is_same_file_use_site else nrow["decl_line"]
        query_col = column if is_same_file_use_site else nrow["decl_col"]
        return (
            query_file,
            query_line,
            query_col,
            {
                "id": str(nrow["id"]),
                "kind": NODE_KINDS.get(nrow["kind"], "Expression"),
                "name": nrow["name"],
                "type": nrow["type_name"] or "",
                "line": nrow["hit_line"],
                "column": nrow["hit_col"],
                "declFile": decl_file,
                "declLine": nrow["decl_line"],
                "declColumn": nrow["decl_col"],
                "file": rel_file,
            },
        )


BRIDGE = None
WEBUI_DIR = os.path.dirname(os.path.abspath(__file__))
STATIC_DIR = os.path.join(WEBUI_DIR, "static")
MONACO_VS_DIR = os.path.join(WEBUI_DIR, "node_modules", "monaco-editor", "min", "vs")


class DataflowRequestHandler(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        # Keep stdout/stderr clean
        pass

    def _send_json(self, data, status=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, fs_path):
        if not os.path.isfile(fs_path):
            self.send_error(404, "File not found")
            return
        ctype, _ = mimetypes.guess_type(fs_path)
        if not ctype:
            if fs_path.endswith(".js"):
                ctype = "application/javascript"
            elif fs_path.endswith(".css"):
                ctype = "text/css"
            elif fs_path.endswith(".ttf"):
                ctype = "font/ttf"
            else:
                ctype = "application/octet-stream"
        with open(fs_path, "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        if "/vs/" in fs_path:
            self.send_header("Cache-Control", "public, max-age=86400")
        else:
            self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        if path == "/api/status":
            self._send_json(
                {
                    "status": BRIDGE.status,
                    "presets": CURATED_PRESETS,
                }
            )
            return

        if path == "/api/files":
            q = (qs.get("q", [""])[0] or "").strip()
            limit = min(int(qs.get("limit", ["60"])[0]), 200)
            conn = BRIDGE.get_db()
            cur = conn.cursor()
            if q:
                cur.execute(
                    """
                    SELECT f.path, COUNT(n.id) AS node_count
                    FROM files f
                    LEFT JOIN nodes n ON n.file_id = f.id
                    WHERE f.path LIKE ? AND f.path NOT LIKE 'out/%'
                    GROUP BY f.id
                    ORDER BY
                      CASE WHEN f.path LIKE 'src/%' THEN 0 ELSE 1 END,
                      LENGTH(f.path) ASC
                    LIMIT ?
                    """,
                    (f"%{q}%", limit),
                )
            else:
                cur.execute(
                    """
                    SELECT f.path, COUNT(n.id) AS node_count
                    FROM files f
                    LEFT JOIN nodes n ON n.file_id = f.id
                    WHERE f.path LIKE 'src/%'
                    GROUP BY f.id
                    ORDER BY node_count DESC
                    LIMIT ?
                    """,
                    (limit,),
                )
            rows = [{"path": r["path"], "nodeCount": r["node_count"]} for r in cur.fetchall()]
            conn.close()
            self._send_json({"files": rows})
            return

        if path == "/api/symbols":
            q = (qs.get("q", [""])[0] or "").strip()
            file_filter = (qs.get("file", [""])[0] or "").strip()
            limit = min(int(qs.get("limit", ["60"])[0]), 200)
            conn = BRIDGE.get_db()
            cur = conn.cursor()
            params = []
            where_clauses = ["n.kind IN (1, 2, 3) AND n.name != '' AND n.name != 'this'"]
            if q:
                where_clauses.append("n.name LIKE ?")
                params.append(f"%{q}%")
            if file_filter:
                where_clauses.append("f.path = ?")
                params.append(file_filter)
            else:
                where_clauses.append("f.path LIKE 'src/%'")
            params.append(limit)
            sql = f"""
                SELECT n.name, n.kind, t.name AS type_name, f.path, n.line, n.column
                FROM nodes n
                JOIN files f ON n.file_id = f.id
                LEFT JOIN types t ON n.type_id = t.id
                WHERE {' AND '.join(where_clauses)}
                ORDER BY
                  CASE WHEN n.name = ? THEN 0 ELSE 1 END,
                  LENGTH(n.name) ASC,
                  f.path ASC,
                  n.line ASC
                LIMIT ?
            """
            params.insert(-1, q)
            cur.execute(sql, params)
            rows = [
                {
                    "name": r["name"],
                    "kind": NODE_KINDS.get(r["kind"], "Variable"),
                    "type": r["type_name"] or "",
                    "file": r["path"],
                    "line": r["line"],
                    "column": r["column"],
                }
                for r in cur.fetchall()
            ]
            conn.close()
            self._send_json({"symbols": rows})
            return

        if path == "/api/file":
            req_file = (qs.get("path", [""])[0] or "").strip()
            if not req_file:
                self._send_json({"error": "Missing path parameter"}, status=400)
                return

            ws_real = os.path.realpath(BRIDGE.workspace_root)
            candidate = req_file if os.path.isabs(req_file) else os.path.join(ws_real, req_file)
            abs_file = os.path.realpath(candidate)
            if not abs_file.startswith(ws_real + "/"):
                self._send_json({"error": "Access denied: path outside workspace root"}, status=403)
                return
            rel_file = os.path.relpath(abs_file, ws_real)

            if not os.path.isfile(abs_file):
                self._send_json({"error": f"File not found: {rel_file}"}, status=404)
                return

            with open(abs_file, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()

            conn = BRIDGE.get_db()
            cur = conn.cursor()
            cur.execute("SELECT id FROM files WHERE path = ?", (rel_file,))
            frow = cur.fetchone()
            nodes = []
            if frow:
                cur.execute(
                    """
                    SELECT id, kind, name, type_name, line, column FROM (
                        SELECT n.id AS id,
                               n.kind AS kind,
                               n.name AS name,
                               COALESCE(t.name, '') AS type_name,
                               n.line AS line,
                               n.column AS column,
                               0 AS is_use
                        FROM nodes n
                        LEFT JOIN types t ON n.type_id = t.id
                        WHERE n.file_id = ?
                          AND n.name NOT IN ('', 'expr', '_msg')
                        UNION ALL
                        SELECT n.id AS id,
                               n.kind AS kind,
                               n.name AS name,
                               COALESCE(t.name, '') AS type_name,
                               e.line AS line,
                               e.column AS column,
                               1 AS is_use
                        FROM edges e
                        JOIN nodes n ON n.id = e.from_id
                        LEFT JOIN types t ON n.type_id = t.id
                        WHERE e.file_id = ?
                          AND n.kind IN (1, 2, 3)
                          AND n.name NOT IN ('', 'expr', '_msg', 'this')
                        UNION ALL
                        SELECT n.id AS id,
                               n.kind AS kind,
                               n.name AS name,
                               COALESCE(t.name, '') AS type_name,
                               e.line AS line,
                               e.column AS column,
                               1 AS is_use
                        FROM edges e
                        JOIN nodes n ON n.id = e.to_id
                        LEFT JOIN types t ON n.type_id = t.id
                        WHERE e.file_id = ?
                          AND e.kind IN (1, 2)
                          AND n.kind IN (1, 2, 3)
                          AND n.name NOT IN ('', 'expr', '_msg', 'this')
                    )
                    ORDER BY line ASC, column ASC, is_use ASC
                    """,
                    (frow["id"], frow["id"], frow["id"]),
                )
                seen_locs = set()
                for r in cur.fetchall():
                    loc_key = (r["line"], r["column"])
                    if loc_key in seen_locs:
                        continue
                    seen_locs.add(loc_key)
                    nodes.append(
                        {
                            "id": str(r["id"]),
                            "kind": NODE_KINDS.get(r["kind"], "Expression"),
                            "name": r["name"],
                            "type": r["type_name"] or "",
                            "line": r["line"],
                            "column": r["column"],
                        }
                    )
            conn.close()
            self._send_json(
                {
                    "path": rel_file,
                    "absPath": abs_file,
                    "content": content,
                    "nodes": nodes,
                }
            )
            return

        if path.startswith("/vs/"):
            rel = path[len("/vs/") :]
            vs_real = os.path.realpath(MONACO_VS_DIR)
            target = os.path.realpath(os.path.join(vs_real, rel))
            if not target.startswith(vs_real + "/"):
                self.send_error(403, "Forbidden")
                return
            self._send_file(target)
            return

        if path == "/" or path == "/index.html":
            self._send_file(os.path.join(STATIC_DIR, "index.html"))
            return

        static_real = os.path.realpath(STATIC_DIR)
        static_candidate = os.path.realpath(os.path.join(static_real, path.lstrip("/")))
        if static_candidate.startswith(static_real + "/") and os.path.isfile(static_candidate):
            self._send_file(static_candidate)
            return

        self.send_error(404, "Not found")

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/api/query":
            self.send_error(404, "Not found")
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode("utf-8")
        body = json.loads(raw) if raw else {}

        method = body.get("method", "query/from-leaves")
        req_file = body.get("file", "")
        line = int(body.get("line", 0))
        column = int(body.get("column", 0))
        prune_cycles = bool(body.get("pruneCycles", True))

        if not req_file or line <= 0 or column <= 0:
            self._send_json({"error": "Invalid file, line, or column"}, status=400)
            return

        ws_real = os.path.realpath(BRIDGE.workspace_root)
        candidate = req_file if os.path.isabs(req_file) else os.path.join(ws_real, req_file)
        abs_file = os.path.realpath(candidate)
        if not abs_file.startswith(ws_real + "/"):
            self._send_json({"error": "Access denied: path outside workspace root"}, status=403)
            return
        rel_file = os.path.relpath(abs_file, ws_real)

        decl_rel_file, snapped_line, snapped_col, snapped_node = BRIDGE.resolve_nearest_node(rel_file, line, column)
        decl_abs_file = os.path.join(BRIDGE.workspace_root, decl_rel_file)

        t0 = time.time()
        rpc_res = BRIDGE.rpc(
            method,
            {
                "file": decl_abs_file,
                "line": snapped_line,
                "column": snapped_col,
            },
        )
        query_ms = round((time.time() - t0) * 1000.0, 2)

        root = rpc_res.get("result")
        if root and prune_cycles and method == "query/incoming":
            root = prune_incoming_cycles(root)

        extra = build_dag_and_metrics(root, BRIDGE.workspace_root) if root else None

        self._send_json(
            {
                "method": method,
                "requested": {"file": rel_file, "line": line, "column": column},
                "resolved": {
                    "file": rel_file,
                    "absFile": abs_file,
                    "line": snapped_node["line"] if snapped_node else snapped_line,
                    "column": snapped_node["column"] if snapped_node else snapped_col,
                    "declFile": decl_rel_file,
                    "declLine": snapped_line,
                    "declColumn": snapped_col,
                    "node": snapped_node,
                },
                "queryTimeMs": query_ms,
                "root": root,
                "metrics": extra["metrics"] if extra else None,
                "dag": extra["dag"] if extra else None,
                "terminalLeaves": extra["terminalLeaves"] if extra else [],
            }
        )

    do_HEAD = do_GET


def main():
    dataflow_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    v8_root = os.path.dirname(os.path.dirname(dataflow_root))

    parser = argparse.ArgumentParser(description="V8 C++ Dataflow WebUI Server")
    parser.add_argument(
        "--workspace-root",
        default=v8_root,
        help="Path to the V8 workspace (default: %(default)s)",
    )
    parser.add_argument(
        "--cache-dir",
        default=os.path.join(v8_root, ".cache"),
        help="Path to the .dataflow.db and .dataflow.db.csr cache directory",
    )
    parser.add_argument(
        "--analyzer",
        default=os.path.join(dataflow_root, "build", "analyzer"),
        help="Path to the C++ analyzer binary",
    )
    parser.add_argument("--host", default="0.0.0.0", help="Host interface to bind to (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8765, help="HTTP port to listen on")
    args = parser.parse_args()

    global BRIDGE
    BRIDGE = AnalyzerBridge(args.analyzer, args.workspace_root, args.cache_dir)

    server = ThreadingHTTPServer((args.host, args.port), DataflowRequestHandler)
    print(f"[WebUI] Listening on http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
