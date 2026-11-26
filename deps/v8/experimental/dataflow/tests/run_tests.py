#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import json
import sys
import os
import time
import threading
import sqlite3

# Helper to read stderr from server process in background
def read_stderr(process, logs, verbose=False):
    for line in process.stderr:
        logs.append(line)
        if verbose:
            sys.stderr.write(f"[Server Log] {line}")
            sys.stderr.flush()

# Helper to send JSON-RPC request and get response
def send_request(process, req, verbose=False):
    req_str = json.dumps(req) + "\n"
    if verbose:
        sys.stderr.write(f"[Client] Sending: {req_str.strip()}\n")
    process.stdin.write(req_str)
    process.stdin.flush()

    while True:
        resp_str = process.stdout.readline().strip()
        if not resp_str:
            raise RuntimeError("Empty response or EOF from server stdout")

        resp = json.loads(resp_str)
        if "method" in resp and resp["method"] == "window/progress":
            p = resp["params"]
            sys.stderr.write(f"[Client Progress] {p['title']} -> {p['message']} ({p['percentage']}%)\n")
            continue

        if verbose:
            sys.stderr.write(f"[Client] Received: {resp_str}\n")
        return resp

# Assert database contains NO absolute paths
def verify_db_portability(db_path):
    print("\n=== VERIFYING DATABASE PORTABILITY (RELATIVE PATHS) ===")
    if not os.path.exists(db_path):
        print(f"ERROR: Database file not found at {db_path}")
        return False

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 1. Verify nodes table
    cursor.execute("SELECT DISTINCT files.path FROM nodes JOIN files ON nodes.file_id = files.id;")
    nodes_files = cursor.fetchall()
    print(f"Nodes file paths: {nodes_files}")

    for file_row in nodes_files:
        file_path = file_row[0]
        if file_path.startswith("/"):
            print(f"ERROR: Found absolute path in nodes table: {file_path}")
            conn.close()
            return False

    # 2. Verify edges table
    cursor.execute("SELECT DISTINCT files.path FROM edges JOIN files ON edges.file_id = files.id;")
    edges_files = cursor.fetchall()
    print(f"Edges file paths: {edges_files}")

    for file_row in edges_files:
        file_path = file_row[0]
        if file_path.startswith("/"):
            print(f"ERROR: Found absolute path in edges table: {file_path}")
            conn.close()
            return False

    # 3. Verify file_hashes table
    cursor.execute("SELECT DISTINCT files.path FROM file_hashes JOIN files ON file_hashes.file_id = files.id;")
    hash_files = cursor.fetchall()
    print(f"Hashes file paths: {hash_files}")

    for file_row in hash_files:
        file_path = file_row[0]
        if file_path.startswith("/"):
            print(f"ERROR: Found absolute path in file_hashes table: {file_path}")
            conn.close()
            return False

    conn.close()
    print("SUCCESS: Portability verified! All database paths are root-relative!")
    return True

def run_cli_test(binary, test_file):
    print("\n=== RUNNING CLI DFG TEST ===")
    cmd = [binary, test_file, "--"]
    print(f"Running: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: Analyzer failed with exit code {result.returncode}")
        print(result.stderr)
        return False

    stdout = result.stdout
    marker_start = "--- DFG JSON ---"
    marker_end = "----------------"

    if marker_start not in stdout or marker_end not in stdout:
        print("ERROR: Failed to find JSON markers in output.")
        return False

    json_str = stdout.split(marker_start)[1].split(marker_end)[0].strip()

    try:
        dfg = json.loads(json_str)
    except Exception as e:
        print(f"ERROR: Failed to parse DFG JSON: {e}")
        return False

    nodes = dfg.get("nodes", [])
    edges = dfg.get("edges", [])
    print(f"Successfully parsed DFG! Nodes: {len(nodes)}, Edges: {len(edges)}")

    # Verify variables
    expected_vars = {"x", "y", "z", "w"}
    found_vars = set()
    for n in nodes:
        if n.get("kind") in ("Variable", "Parameter"):
            found_vars.add(n.get("name"))

    missing_vars = expected_vars - found_vars
    if missing_vars:
        print(f"ERROR: Missing expected variables: {missing_vars}")
        return False

    # Verify flow paths
    node_map = {n["id"]: n for n in nodes}
    flow_paths = []
    for e in edges:
        from_node = node_map.get(e["from"])
        to_node = node_map.get(e["to"])
        if from_node and to_node:
            flow_paths.append((from_node["name"], to_node["name"], e["kind"]))

    expected_flows = [
        ("10", "x", "Initialization"),
        ("20", "y", "Initialization"),
        ("z", "w", "Initialization")
    ]

    for exp in expected_flows:
        found = False
        for f in flow_paths:
            if f[0] == exp[0] and f[1] == exp[1] and f[2] == exp[2]:
                found = True
                break
        if not found:
            print(f"ERROR: Missing expected flow {exp[0]} -> {exp[1]} ({exp[2]})")
            return False

    print("SUCCESS: CLI DFG verification passed!")
    return True

def run_standalone_index_test(binary, project_dir, build_dir, cache_dir):
    print("\n=== RUNNING STANDALONE INDEXING TEST (--index) ===")

    db_path = os.path.join(cache_dir, ".dataflow.db")
    if os.path.exists(db_path):
        os.remove(db_path)

    cmd = [
        binary, "--index",
        "--workspace-root", project_dir,
        "--cache-dir", cache_dir,
        "--compile-commands-dir", build_dir
    ]
    print(f"Running: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: Standalone indexing failed with exit code {result.returncode}")
        print(result.stderr)
        return False

    stdout = result.stdout
    print("Analyzer output:")
    print(stdout)

    if "SUCCESS: Indexing complete!" not in stdout:
        print("ERROR: Expected completion log in stdout.")
        return False

    if "[Index Progress] [1/3]" not in stdout or "33%" not in stdout:
        print("ERROR: Expected progress logs in stdout.")
        return False

    if "Nodes: 61, Edges: 43" not in stdout:
        print("ERROR: Expected stats 'Nodes: 61, Edges: 43' in stdout.")
        return False

    # Verify DB actually contains relative paths!
    if not verify_db_portability(db_path):
        return False

    print("SUCCESS: Standalone indexing test passed!")
    return True

def run_selective_index_test(binary, project_dir, build_dir, cache_dir):
    print("\n=== RUNNING SELECTIVE CLI FILE INDEXING TEST ===")

    db_path = os.path.join(cache_dir, ".dataflow.db")
    if os.path.exists(db_path):
        os.remove(db_path)

    # Pass trailing positional file filter 'utils.cc'!
    cmd = [
        binary, "--index",
        "--workspace-root", project_dir,
        "--cache-dir", cache_dir,
        "--compile-commands-dir", build_dir,
        "utils.cc"
    ]
    print(f"Running: {' '.join(cmd)}")

    result = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: Selective indexing failed with exit code {result.returncode}")
        print(result.stderr)
        return False

    stdout = result.stdout
    print("Analyzer output:")
    print(stdout)

    if "SUCCESS: Indexing complete!" not in stdout:
        print("ERROR: Expected completion log in stdout.")
        return False

    if "[Index Progress] [1/1]" not in stdout:
        print("ERROR: Expected progress log [1/1] for single file!")
        return False

    # Verify DB only contains 'utils.cc' and 'utils.h' (no main.cc)
    import sqlite3
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute("SELECT path FROM files;")
    files = [r[0] for r in c.fetchall()]
    conn.close()

    print(f"Indexed files in DB: {files}")
    if "main.cc" in files:
        print("ERROR: main.cc should have been excluded by the selective CLI path filter!")
        return False
    if "utils.cc" not in files:
        print("ERROR: utils.cc should have been indexed!")
        return False

    print("SUCCESS: Selective CLI file indexing test passed!")
    return True

def run_v8_advanced_features_test(binary, project_dir, build_dir, cache_dir):
    print("\n=== RUNNING ADVANCED V8 COMPILER DFG INTEGRATION TESTS ===")

    # Clear cache database
    db_path = os.path.join(cache_dir, ".dataflow.db")
    if os.path.exists(db_path):
        os.remove(db_path)

    # Force full re-indexing of mock_v8.cc and mock_v8.h
    cmd = [
        binary, "--index",
        "--workspace-root", project_dir,
        "--cache-dir", cache_dir,
        "--compile-commands-dir", build_dir
    ]
    print(f"Forcing full codebase index: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("ERROR: Re-indexing failed!")
        print(result.stderr)
        return False

    # Spawn server for JSON-RPC queries
    process = subprocess.Popen(
        [binary, "--server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    def read_rpc_response(req_id):
        while True:
            line = process.stdout.readline()
            if not line:
                break
            try:
                resp = json.loads(line)
                if resp.get("id") == req_id:
                    return resp
            except:
                pass
        return None

    try:
        # 1. Initialize
        init_req = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "workspaceRoot": project_dir,
                "cacheDir": cache_dir,
                "compileCommandsDir": build_dir
            }
        }
        process.stdin.write(json.dumps(init_req) + "\n")
        process.stdin.flush()
        init_resp = read_rpc_response(1)
        if not init_resp or "error" in init_resp:
            print("ERROR: Failed to initialize server.")
            process.terminate()
            return False

        # ------------------------------------------------------------
        # TEST 1: Temporal Flow Pruning, Operator* Peeling & QueryFromLeaves
        # ------------------------------------------------------------
        query_req1 = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "query/incoming",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.cc"),
                "line": 28,
                "column": 49
            }
        }
        print("\nTest 1: Querying temporal flow for UpdateLoop variable current...")
        process.stdin.write(json.dumps(query_req1) + "\n")
        process.stdin.flush()

        resp1 = read_rpc_response(2)
        if not resp1 or "error" in resp1:
            print("ERROR: Test 1 query failed.")
            process.terminate()
            return False

        res1 = resp1.get("result")
        if not res1:
            print("ERROR: Test 1 returned null tree.")
            process.terminate()
            return False

        # Flatten path helper to check temporal sequence
        path_nodes = []
        def flatten_tree(node):
            node_data = node.get("node")
            if node_data:
                path_nodes.append(node_data)
            for child in node.get("children", []):
                flatten_tree(child)

        flatten_tree(res1)

        names_and_lines = [(n["name"], n["location"]["line"]) for n in path_nodes]
        print(f"Test 1 Dataflow Path: {names_and_lines}")

        all_lines = set(l for name, l in names_and_lines)
        print(f"Test 1 Traversed Lines: {all_lines}")
        if not all_lines.issuperset({28, 31, 32, 17}):
            print(f"ERROR: Temporal flow check failed! Path did not traverse all expected lines: {all_lines}")
            process.terminate()
            return False

        # Verify that 'desc' parameter (line 27) is successfully reached
        param_reached = any(name == "desc" and l == 27 for name, l in names_and_lines)
        if not param_reached:
            print("ERROR: Parameter desc at line 27 was not reached!")
            process.terminate()
            return False

        # Verify DefParam (desc at line 27) does NOT get a bogus duplicate DEAD node
        bogus_def_dead = any(name == "DEAD: desc" and l == 27 for name, l in names_and_lines)
        if bogus_def_dead:
            print("ERROR: DefParam 'desc' at line 27 should not have a duplicate DEAD node!")
            process.terminate()
            return False

        # Also test query/from-leaves on 'current' (line 24, col 49) to ensure both
        # 'desc' (behind elidable DeclParam) and 'expr' (new DescriptorArray) are leaves!
        query_leaves_req = {
            "jsonrpc": "2.0",
            "id": 20,
            "method": "query/from-leaves",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.cc"),
                "line": 28,
                "column": 49
            }
        }
        process.stdin.write(json.dumps(query_leaves_req) + "\n")
        process.stdin.flush()
        resp_leaves = read_rpc_response(20)
        if not resp_leaves or not resp_leaves.get("result"):
            print("ERROR: query/from-leaves on current failed.")
            process.terminate()
            return False
        leaf_children = [c["node"]["name"] for c in resp_leaves["result"].get("children", [])]
        print(f"Test 1 From-Leaves Root Children: {leaf_children}")
        if "desc" not in leaf_children or "expr" not in leaf_children:
            print(f"ERROR: query/from-leaves missed expected leaves ('desc', 'expr'): {leaf_children}")
            process.terminate()
            return False

        # Also test query/from-leaves on UpdateLoop_return (downstream of the current loop!)
        # and verify that no dead-end cycle stubs (like 'prev' or 'ReallocateArray()') appear
        # as childless terminal nodes!
        query_downstream_leaves_req = {
            "jsonrpc": "2.0",
            "id": 21,
            "method": "query/from-leaves",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.h"),
                "line": 68,
                "column": 45
            }
        }
        process.stdin.write(json.dumps(query_downstream_leaves_req) + "\n")
        process.stdin.flush()
        resp_downstream_leaves = read_rpc_response(21)
        if not resp_downstream_leaves or not resp_downstream_leaves.get("result"):
            print("ERROR: query/from-leaves on UpdateLoop_return failed.")
            process.terminate()
            return False
        def collect_terminals(node, terms):
            children = node.get("children", [])
            if not children and not node.get("is_cached", False):
                terms.append(node.get("node", {}).get("name"))
            else:
                for c in children:
                    collect_terminals(c, terms)
        downstream_terminals = []
        for c in resp_downstream_leaves["result"].get("children", []):
            collect_terminals(c, downstream_terminals)
        print(f"Test 1 Downstream From-Leaves Terminals: {downstream_terminals}")
        if not downstream_terminals or any(t != "UpdateLoop_return" for t in downstream_terminals):
            print(f"ERROR: Dead-end cycle stub found in query/from-leaves terminals: {downstream_terminals}")
            process.terminate()
            return False

        # Also test query/outgoing on out-of-line definition AllocateArray (mock_v8.cc:16:45)
        query_out_def_req = {
            "jsonrpc": "2.0",
            "id": 22,
            "method": "query/outgoing",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.cc"),
                "line": 16,
                "column": 45
            }
        }
        process.stdin.write(json.dumps(query_out_def_req) + "\n")
        process.stdin.flush()
        resp_out_def = read_rpc_response(22)
        if not resp_out_def or not resp_out_def.get("result"):
            print("ERROR: query/outgoing on AllocateArray definition failed.")
            process.terminate()
            return False
        out_def_children = [c["node"]["name"] for c in resp_out_def["result"].get("children", [])]
        print(f"Test 1 Outgoing from AllocateArray definition: {out_def_children}")
        if "AllocateArray()" not in out_def_children:
            print(f"ERROR: query/outgoing on out-of-line definition missed callers: {out_def_children}")
            process.terminate()
            return False

        print("SUCCESS: Test 1 (Temporal Flow Pruning, Operator* Peeling & From-Leaves) passed!")

        # ------------------------------------------------------------
        # TEST 2: Store-to-Load / Field-Write Unification
        # Query incoming flow to Map::instance_descriptors() read (mock_v8.cc:4:12)
        # ------------------------------------------------------------
        query_req2 = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "query/incoming",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.h"),
                "line": 58,
                "column": 49
            }
        }
        print("\nTest 2: Querying store-to-load field-write flow for Map::instance_descriptors_ field at mock_v8.h:58...")
        process.stdin.write(json.dumps(query_req2) + "\n")
        process.stdin.flush()

        resp2 = read_rpc_response(3)
        if not resp2 or "error" in resp2:
            print("ERROR: Test 2 query failed.")
            process.terminate()
            return False

        res2 = resp2.get("result")
        if not res2:
            print("ERROR: Test 2 returned null tree.")
            process.terminate()
            return False

        path_nodes2 = []
        def flatten_tree2(node):
            node_data = node.get("node")
            if node_data:
                path_nodes2.append(node_data)
            for child in node.get("children", []):
                flatten_tree2(child)
        flatten_tree2(res2)

        names_and_lines2 = [(n["name"], n["location"]["line"]) for n in path_nodes2]
        print(f"Test 2 Dataflow Path: {names_and_lines2}")

        # back to instance_descriptors_.store(value) write at line 11!
        store_reached = any(name == "value" and l == 11 for name, l in names_and_lines2)
        if not store_reached:
            print("ERROR: Store write at line 11 was not reached by the loader getter read!")
            process.terminate()
            return False

        print("SUCCESS: Test 2 (Store-to-Load Field Unification) passed!")

        # ------------------------------------------------------------
        # TEST 3: Return Unification
        # Query incoming flow to AllocateArray(size) return (mock_v8.cc:19:12)
        # ------------------------------------------------------------
        query_req3 = {
            "jsonrpc": "2.0",
            "id": 4,
            "method": "query/incoming",
            "params": {
                "file": os.path.join(project_dir, "mock_v8.cc"),
                "line": 16,
                "column": 45
            }
        }
        print("\nTest 3: Querying Return Unification for AllocateArray at line 16...")
        process.stdin.write(json.dumps(query_req3) + "\n")
        process.stdin.flush()

        resp3 = read_rpc_response(4)
        if not resp3 or "error" in resp3:
            print("ERROR: Test 3 query failed.")
            process.terminate()
            return False

        res3 = resp3.get("result")
        if not res3:
            print("ERROR: Test 3 returned null tree.")
            process.terminate()
            return False

        path_nodes3 = []
        def flatten_tree3(node):
            node_data = node.get("node")
            if node_data:
                path_nodes3.append(node_data)
            for child in node.get("children", []):
                flatten_tree3(child)
        flatten_tree3(res3)
        names_and_lines3 = [(n["name"], n["location"]["line"]) for n in path_nodes3]
        print(f"Test 3 Dataflow Path: {names_and_lines3}")
        if not any(name == "raw" and l == 17 for name, l in names_and_lines3):
            print("ERROR: Test 3 Return Unification did not reach allocation 'raw' at line 17!")
            process.terminate()
            return False

        print("SUCCESS: Test 3 (Return Unification) passed!")

    finally:
        process.terminate()
        process.wait()

    print("SUCCESS: Advanced V8 compiler DFG integration tests passed cleanly!")
    return True

def run_server_test(binary, project_dir, build_dir, cache_dir, verbose=False):
    print("\n=== RUNNING SERVER INTEGRATION TEST ===")

    # Clean cache
    db_path = os.path.join(cache_dir, ".dataflow.db")
    if os.path.exists(db_path):
        os.remove(db_path)

    # Start server
    process = subprocess.Popen(
        [binary, "--server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    server_logs = []
    stderr_thread = threading.Thread(target=read_stderr, args=(process, server_logs, verbose))
    stderr_thread.daemon = True
    stderr_thread.start()

    time.sleep(0.5)

    # 1. Initialize (Cache Misses)
    print("\nStep 1: Initializing server (First run, expecting cache misses)...")
    init_req = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "workspaceRoot": project_dir,
            "cacheDir": cache_dir,
            "compileCommandsDir": build_dir
        }
    }

    init_resp = send_request(process, init_req, verbose)
    if "error" in init_resp:
        print(f"ERROR: Init failed: {init_resp['error']}")
        process.terminate()
        return False

    print(f"SUCCESS: Initialized! Result: {init_resp['result']}")

    # Assert relative database paths!
    if not verify_db_portability(db_path):
        process.terminate()
        return False

    time.sleep(0.5)
    log_str = "".join(server_logs)
    if "Cache miss" not in log_str:
        print("ERROR: Expected cache misses in logs!")
        process.terminate()
        return False
    print("SUCCESS: Confirmed cache misses in logs.")

    # 2. Query Incoming Flow
    print("\nStep 2: Querying incoming flow for 'w' at main.cc:19:9...")
    query_req = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "query/incoming",
        "params": {
            "file": f"{project_dir}/main.cc",
            "line": 19,
            "column": 9
        }
    }

    query_resp = send_request(process, query_req, verbose)
    if "error" in query_resp:
        print(f"ERROR: Query failed: {query_resp['error']}")
        process.terminate()
        return False

    tree = query_resp["result"]
    if not tree or tree["node"]["name"] != "w":
        print(f"ERROR: Expected query root 'w', got: {tree}")
        process.terminate()
        return False

    # Assert that the returned JSON paths are absolute!
    # The extension expects absolute paths to open files.
    root_file = tree["node"]["location"]["file"]
    if not root_file.startswith("/"):
        print(f"ERROR: Query result JSON must contain absolute paths! Got: {root_file}")
        process.terminate()
        return False
    print("SUCCESS: Confirmed query result JSON contains absolute paths.")

    print("SUCCESS: Query returned correct flow tree!")

    # 3. Restart and check Caching (Cache Hits)
    print("\nStep 3: Restarting server to verify incremental caching (expecting cache hits)...")
    process.terminate()
    process.wait()

    # Start new server
    process = subprocess.Popen(
        [binary, "--server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    server_logs.clear()
    stderr_thread = threading.Thread(target=read_stderr, args=(process, server_logs, verbose))
    stderr_thread.daemon = True
    stderr_thread.start()

    time.sleep(0.5)

    init_resp = send_request(process, init_req, verbose)
    if "error" in init_resp:
        print(f"ERROR: Re-init failed: {init_resp['error']}")
        process.terminate()
        return False

    time.sleep(0.5)
    log_str = "".join(server_logs)
    if "Cache hit" not in log_str:
        print("ERROR: Expected cache hits in logs!")
        process.terminate()
        return False
    print("SUCCESS: Confirmed cache hits in logs!")

    process.terminate()
    process.wait()
    print("SUCCESS: Server integration test passed!")
    return True

def run_visitor_edge_cases_test(binary):
    print("\n=== RUNNING VISITOR EDGE CASES & OFFSET FIELD REGRESSION TEST ===")
    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        include_dir = os.path.join(tmpdir, "include")
        os.makedirs(include_dir, exist_ok=True)
        api_hdr_path = os.path.join(include_dir, "public_api.h")
        with open(api_hdr_path, "w") as f:
            f.write("""\
#pragma once
namespace v8 {
int UncalledPublicApiEntry(int api_uncalled_param);
int CalledPublicApiEntry(unsigned short api_called_param);
}  // namespace v8
""")
        src_path = os.path.join(tmpdir, "visitor_cases.cc")
        code = """\
#include "include/public_api.h"
namespace v8 {
namespace base {
template <class T, int shift, int size, class U = unsigned int>
class BitField {
 public:
  static U encode(T value) { return static_cast<U>(value) << shift; }
  static U update(U previous, T value) { return previous | encode(value); }
  static T decode(U value) { return static_cast<T>((value >> shift) & ((1u << size) - 1u)); }
};
}  // namespace base
namespace internal {
struct Isolate {};
struct PtrComprCageBase {};
template <typename T, int kOffset>
class TaggedField {
 public:
  static T load(const void* host) { return T{}; }
  static T Acquire_Load(PtrComprCageBase cage_base, const void* host) { return T{}; }
  static void store(void* host, T value) {}
  static T Release_CompareAndSwap(void* host, T old_val, T new_val) { return old_val; }
};
}  // namespace internal
}  // namespace v8

class CustomBox {
 public:
  int stored_val_ = 0;
  CustomBox& operator=(int rhs) {
    stored_val_ = rhs;
    return *this;
  }
};

class OverloadedTarget {
 public:
  const char* str_field_ = nullptr;
  int int_field_ = 0;
  explicit OverloadedTarget(const char* s) : str_field_(s) {}
  explicit OverloadedTarget(int x) : int_field_(x) {}
};

struct TemplatedCtorTarget {
  int first_;
  int second_;
  template <typename A, typename B>
  explicit TemplatedCtorTarget(A&& a, B&& b) : first_(a), second_(b) {}
};

class Zone {
 public:
  template <typename T, typename... Args>
  T* New(Args... args) {
    return new T(args...);
  }
};

class NodeBase {
 public:
  template <typename Derived, typename... Args>
  static Derived* Allocate(Zone* zone, int input_count, Args... args) {
    unsigned long bitfield = (unsigned long)input_count;
    return new Derived(bitfield, args...);
  }
  template <typename Derived, typename... Args>
  static Derived* New(Zone* zone, int input_count, Args... args) {
    return Allocate<Derived>(zone, input_count, args...);
  }
};

struct ShiftedNode {
  unsigned long bitfield_;
  int offset_;
  explicit ShiftedNode(unsigned long bitfield, int offset)
      : bitfield_(bitfield), offset_(offset) {}
};

class MaglevReducer {
 public:
  Zone* zone_;
  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(int input_node, Args... args) {
    return NodeBase::New<NodeT>(zone_, 1, args...);
  }
};

class MockJSObject {
 public:
  enum {
    kTaggedValOffset = 8,
    kSandboxedPtrOffset = 16,
  };
  using ModeBits = v8::base::BitField<int, 2, 4>;

  int guard_ = 1;
  unsigned int flags_ = 0;

  void WriteSandboxedPointerField(int offset, v8::internal::Isolate* isolate, int value) {}
  int ReadSandboxedPointerField(int offset, v8::internal::Isolate* isolate) const { return 0; }

  void set_mode_with_dcheck(int mode_in) {
    if (this->guard_ == 0) return;
    this->flags_ = ModeBits::encode(mode_in);
  }
  int mode() const {
    return ModeBits::decode(this->flags_);
  }

  void StoreOffsets(v8::internal::Isolate* isolate, int tagged_in, int sandboxed_in) {
    v8::internal::TaggedField<int, kTaggedValOffset>::Release_CompareAndSwap(this, 0, tagged_in);
    this->WriteSandboxedPointerField(kSandboxedPtrOffset, isolate, sandboxed_in);
  }

  int LoadTaggedOffset() const {
    v8::internal::PtrComprCageBase cage_base;
    return v8::internal::TaggedField<int, kTaggedValOffset>::Acquire_Load(cage_base, this);
  }

  int LoadSandboxedOffset(v8::internal::Isolate* isolate) const {
    return this->ReadSandboxedPointerField(kSandboxedPtrOffset, isolate);
  }
};

struct cast_helper {
  static int to_storage_type(int v) { return v; }
  static int to_return_type(int v) { return v; }
};

struct AsAtomicImpl {
  static int* to_storage_addr(int* p) { return p; }
  static int Acquire_Load(int* addr) {
    return cast_helper::to_return_type(*to_storage_addr(addr));
  }
  static void Release_Store(int* addr, int val) {
    *to_storage_addr(addr) = cast_helper::to_storage_type(val);
  }
};

struct CustomAtomicSlot {
  int val_ = 0;
  int Acquire_Load() const { return val_; }
};

template <typename T>
struct DirectHandle {
  T val_ = 0;
  DirectHandle() = default;
  explicit DirectHandle(T v) : val_(v) {}
  T operator*() const { return val_; }
};

struct NullMaybeType {};
constexpr NullMaybeType kNullMaybe{};

template <typename T>
struct MaybeDirectHandle {
  T val_ = 0;
  MaybeDirectHandle() = default;
  MaybeDirectHandle(NullMaybeType) {}
  explicit MaybeDirectHandle(T v) : val_(v) {}
  MaybeDirectHandle(DirectHandle<T> h) : val_(*h) {}
  bool To(DirectHandle<T>* out) const {
    *out = DirectHandle<T>(val_);
    return true;
  }
};

struct BigInt {
  template <template <typename> typename HandleType>
  static MaybeDirectHandle<int> FromObject(v8::internal::Isolate* isolate,
                                           HandleType<int> obj);
};

template <template <typename> typename HandleType>
MaybeDirectHandle<int> BigInt::FromObject(v8::internal::Isolate* isolate,
                                          HandleType<int> obj) {
  if (!isolate) return kNullMaybe;
  return MaybeDirectHandle<int>(*obj);
}

namespace v8 {
namespace internal {
class ReadOnlyRoots {
 public:
  int read_only_roots_[4] = {1, 2, 3, 4};
  int undefined_value() const { return read_only_roots_[0]; }
  int empty_fixed_array() const { return read_only_roots_[1]; }
  int the_hole_value() const { return read_only_roots_[2]; }
};
class RootsTable {
 public:
  int roots_[4] = {10, 20, 30, 40};
  int const& undefined_value() const { return roots_[0]; }
  int const& empty_fixed_array() const { return roots_[1]; }
};
template <typename Impl>
class FactoryBase {
 public:
  RootsTable table_;
  DirectHandle<int> undefined_value() { return DirectHandle<int>(table_.undefined_value()); }
  DirectHandle<int> empty_fixed_array() { return DirectHandle<int>(table_.empty_fixed_array()); }
  DirectHandle<int> AllocateCustom(int x) { return DirectHandle<int>(x); }
};
class Factory : public FactoryBase<Factory> {};
}  // namespace internal
}  // namespace v8

int ComputePassthroughValue(int input_v) {
  int inner_computed = input_v;
  return inner_computed;
}

int ComputeWithDefault(int default_param = 4242) {
  return default_param;
}

class HeapObjectLayout {};
class TrustedObjectLayout : public HeapObjectLayout {};

class MockHeapMap : public HeapObjectLayout {
 public:
  unsigned char instance_size_words_ = 0;
  void set_instance_size_words(unsigned char v) { instance_size_words_ = v; }
  int instance_size_words() const { return instance_size_words_; }
};

class MockTrustedBytecode : public TrustedObjectLayout {
 public:
  unsigned short frame_size_ = 0;
  void set_frame_size(unsigned short v) { frame_size_ = v; }
  int frame_size() const { return frame_size_; }
};

int UncalledHelperWithDeadParam(int dead_param) {
  int dead_derived = dead_param + 1;
  return dead_derived;
}

int MakeRegIndex(int idx) { return idx + 100; }

template <typename T>
struct MockRegisterFrameArray {
  int first_reg_ = 0;
  MockRegisterFrameArray() {
    int first_param = MakeRegIndex(0);
    first_reg_ = first_param;
  }
};

template <class Derived, class ElementT>
class MockTaggedArrayBase : public HeapObjectLayout {
 public:
  const Derived* derived() const { return static_cast<const Derived*>(this); }
  inline ElementT get(unsigned int index) const;
};

template <class D, class ElementT_>
ElementT_ MockTaggedArrayBase<D, ElementT_>::get(unsigned int index) const {
  return derived()->objects()[index];
}

class MockFixedArray : public MockTaggedArrayBase<MockFixedArray, int> {
 public:
  int flexible_array_member_data_[4];
  const int* objects() const { return flexible_array_member_data_; }
};

class MockArrayList : public MockTaggedArrayBase<MockArrayList, int> {
 public:
  int flexible_array_member_data_[4];
  const int* objects() const { return flexible_array_member_data_; }
};

class MockUnusedArray : public MockTaggedArrayBase<MockUnusedArray, int> {
 public:
  int flexible_array_member_data_[4];
  const int* objects() const { return flexible_array_member_data_; }
};

int ReadUnusedArray(const MockUnusedArray& u) {
  int unused_elem_read = u.get(0);
  return unused_elem_read;
}

int NonTemplateOffsetSink(int offset_arg) {
  return offset_arg;
}

template <typename BaseT>
class MockTemplateReducer {
 public:
  int BuildLoadField(int object, int offset);
  int BuildInternalLoad(int object) {
    return BuildLoadField(object, 4);
  }
};

template <typename BaseT>
int MockTemplateReducer<BaseT>::BuildLoadField(int object, int offset) {
  return object + NonTemplateOffsetSink(offset);
}

class MockGraphBuilder {
 public:
  MockTemplateReducer<MockGraphBuilder> reducer_;
  int BuildLoadField(int object, int field_idx_offset) {
    return reducer_.BuildInternalLoad(object) +
           reducer_.BuildLoadField(object, field_idx_offset);
  }
};

class MockGraphOptimizer {
 public:
  MockTemplateReducer<MockGraphOptimizer> reducer_;
  int OptimizeLoad(int object) {
    return reducer_.BuildInternalLoad(object);
  }
};

namespace v8 {
int UncalledPublicApiEntry(int api_uncalled_param) {
  int api_uncalled_sink = api_uncalled_param;
  return api_uncalled_sink;
}
int CalledPublicApiEntry(unsigned short api_called_param) {
  int api_called_sink = api_called_param;
  return api_called_sink;
}
}  // namespace v8

int RunVisitorCases(Zone* zone, v8::internal::Isolate* isolate) {
  int seed_tagged = 101;
  int seed_sandboxed = 202;
  int seed_mode = 7;
  MockJSObject obj;
  obj.StoreOffsets(isolate, seed_tagged, seed_sandboxed);
  obj.set_mode_with_dcheck(seed_mode);

  int out_tagged = obj.LoadTaggedOffset();
  int out_sandboxed = obj.LoadSandboxedOffset(isolate);
  int out_mode = obj.mode();

  int arr[2] = {out_tagged, out_sandboxed};
  int range_elem = 0;
  for (int item : arr) {
    range_elem = item;
  }

  int lambda_arg = 303;
  auto fn = [](int param_x) { return param_x; };
  int lambda_res = fn(lambda_arg);

  int assign_rhs = 404;
  CustomBox box;
  box = assign_rhs;
  int box_out = box.stored_val_;

  int ctor_int_seed = 505;
  OverloadedTarget* target = zone->New<OverloadedTarget>(ctor_int_seed);
  int target_out = target->int_field_;

  int tmpl_ctor_a = 515;
  int tmpl_ctor_b = 525;
  TemplatedCtorTarget* tmpl_target = zone->New<TemplatedCtorTarget>(tmpl_ctor_a, tmpl_ctor_b);
  int tmpl_target_out = tmpl_target->second_;

  int shifted_input_node = 535;
  int shifted_offset_seed = 545;
  MaglevReducer reducer{zone};
  ShiftedNode* shifted_node = reducer.AddNewNode<ShiftedNode>(shifted_input_node, shifted_offset_seed);
  int shifted_offset_out = shifted_node->offset_;

  int atomic_seed = 606;
  int atomic_cell = 0;
  AsAtomicImpl::Release_Store(&atomic_cell, cast_helper::to_storage_type(atomic_seed));
  int atomic_out = cast_helper::to_return_type(AsAtomicImpl::Acquire_Load(&atomic_cell));

  CustomAtomicSlot custom_slot{atomic_out};
  int member_acquire_out = custom_slot.Acquire_Load();

  int bigint_input_seed = 707;
  DirectHandle<int> value{bigint_input_seed};
  DirectHandle<int> converted_value{0};
  if (!(BigInt::FromObject(isolate, value)).To(&converted_value)) {
    return 0;
  }

  v8::internal::ReadOnlyRoots ro_roots;
  v8::internal::RootsTable roots_table;
  v8::internal::Factory factory;
  DirectHandle<int> root_val = factory.undefined_value();
  int ro_val = ro_roots.the_hole_value();
  int rt_val = roots_table.empty_fixed_array();

  int call_seed = 808;
  int call_result = ComputePassthroughValue(call_seed);
  int other_call_seed = 909;
  int other_call_result = ComputePassthroughValue(other_call_seed);

  int use_site_seed = 1010;
  int reassigned_var = 0;
  reassigned_var = use_site_seed;

  int default_arg_out = ComputeWithDefault();
  int explicit_override_seed = 4343;
  int explicit_arg_out = ComputeWithDefault(explicit_override_seed);

  constexpr int kTaggedSize = 4;
  constexpr int kHeaderSize = 8;
  int computed_field_offset = obj.mode() * kTaggedSize + kHeaderSize;
  int masked_low_bits = explicit_arg_out & 3;

  MockHeapMap heap_map;
  heap_map.set_instance_size_words(10);
  int sandbox_size_bytes = heap_map.instance_size_words() * kTaggedSize;

  MockTrustedBytecode trusted_bc;
  trusted_bc.set_frame_size(16);
  int trusted_frame_bytes = trusted_bc.frame_size() + 8;
  int combined_taint_bytes = sandbox_size_bytes + trusted_frame_bytes;

  MockRegisterFrameArray<int> reg_arr_int;
  MockRegisterFrameArray<double> reg_arr_dbl;

  MockFixedArray fixed_arr{};
  MockArrayList list_arr{};
  int fixed_elem_read = fixed_arr.get(0);
  int list_elem_read = list_arr.get(0);
  int combined_array_sink = fixed_elem_read + list_elem_read;

  MockGraphBuilder graph_builder{};
  MockGraphOptimizer graph_optimizer{};
  int field_index_offset_seed = 64;
  int reducer_load_out = graph_builder.BuildLoadField(1, field_index_offset_seed) +
                         graph_optimizer.OptimizeLoad(1);

  int loop_sink = 0;
  unsigned char raw_hi = 200;
  unsigned char raw_lo = 10;
  int loop_bound = static_cast<int>(raw_hi) - static_cast<int>(raw_lo);
  for (int loop_idx = 0; loop_idx < loop_bound; loop_idx++) {
    loop_sink = ComputePassthroughValue(loop_idx);
  }
  int unbounded_idx = 0;
  while (unbounded_idx < loop_sink) {
    unbounded_idx++;
  }
  int unbounded_sink = ComputePassthroughValue(unbounded_idx);
  int bounded_loop_sink = 0;
  for (int bounded_idx = 0; bounded_idx <= 16; ++bounded_idx) {
    bounded_loop_sink = bounded_idx;
  }
  int api_internal_call_res = v8::CalledPublicApiEntry(5);

  return out_mode + range_elem + lambda_res + box_out + target_out + member_acquire_out + converted_value.val_ + *root_val + ro_val + rt_val + call_result + other_call_result + reassigned_var + default_arg_out + explicit_arg_out + computed_field_offset + masked_low_bits + combined_taint_bytes + reg_arr_int.first_reg_ + reg_arr_dbl.first_reg_ + combined_array_sink + reducer_load_out + loop_sink + unbounded_sink + bounded_loop_sink + api_internal_call_res;
}
"""
        with open(src_path, "w") as f:
            f.write(code)
        cc_json = [{
            "directory": tmpdir,
            "command": f"clang++ -std=c++17 -I{tmpdir} -c {src_path}",
            "file": src_path,
        }]
        with open(os.path.join(tmpdir, "compile_commands.json"), "w") as f:
            json.dump(cc_json, f)

        idx_res = subprocess.run(
            [binary, "--index", f"--compile-commands-dir={tmpdir}", f"--cache-dir={tmpdir}", f"--workspace-root={tmpdir}"],
            capture_output=True, text=True
        )
        if idx_res.returncode != 0:
            print("ERROR: Indexing visitor_cases.cc failed:", idx_res.stderr)
            return False

        proc = subprocess.Popen(
            [binary, "--server"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        try:
            def rpc(req_id, method, params):
                req = {"jsonrpc": "2.0", "id": req_id, "method": method, "params": params}
                proc.stdin.write(json.dumps(req) + "\n")
                proc.stdin.flush()
                while True:
                    line = proc.stdout.readline()
                    if not line:
                        return None
                    try:
                        data = json.loads(line.strip())
                        if data.get("id") == req_id:
                            return data.get("result")
                    except json.JSONDecodeError:
                        pass

            rpc(1, "initialize", {"workspaceRoot": tmpdir, "cacheDir": tmpdir, "compileCommandsDir": tmpdir})

            def flatten_names(tree):
                if not tree:
                    return []
                out = []
                if tree.get("node"):
                    out.append(tree["node"]["name"])
                for c in tree.get("children", []):
                    out.extend(flatten_names(c))
                return out

            # Find line numbers dynamically
            lines = code.splitlines()
            def line_of(substr):
                for idx, l in enumerate(lines, 1):
                    if substr in l:
                        return idx
                raise RuntimeError(f"Substring not found: {substr}")

            # 1. TaggedField<int, kTaggedValOffset> store -> load
            t1 = rpc(2, "query/from-leaves", {"file": src_path, "line": line_of("int out_tagged ="), "column": 7})
            names1 = flatten_names(t1)
            if "seed_tagged" not in names1 or "kTaggedValOffset" not in names1:
                print(f"ERROR: TaggedField offset flow failed: {names1}")
                return False

            # 2. 3-arg WriteSandboxedPointerField(kSandboxedPtrOffset, isolate, sandboxed_in) -> ReadSandboxedPointerField
            t2 = rpc(3, "query/from-leaves", {"file": src_path, "line": line_of("int out_sandboxed ="), "column": 7})
            names2 = flatten_names(t2)
            if "seed_sandboxed" not in names2 or "kSandboxedPtrOffset" not in names2:
                print(f"ERROR: 3-arg WriteSandboxedPointerField enum offset flow failed: {names2}")
                return False

            # 3. BitField SetterFieldVisitor with leading guard_ read
            t3 = rpc(4, "query/from-leaves", {"file": src_path, "line": line_of("int out_mode ="), "column": 7})
            names3 = flatten_names(t3)
            if "seed_mode" not in names3:
                print(f"ERROR: BitField SetterFieldVisitor with leading guard read failed: {names3}")
                return False

            # 4. CXXForRangeStmt: item traces to arr/out_tagged/out_sandboxed and has NO __begin1
            t4 = rpc(5, "query/from-leaves", {"file": src_path, "line": line_of("for (int item : arr)"), "column": 12})
            names4 = flatten_names(t4)
            if "__begin1" in names4 or "arr" not in names4:
                print(f"ERROR: CXXForRangeStmt flow failed or leaked __begin1: {names4}")
                return False

            # 5. LambdaExpr non-static operator() parameter mapping
            t5 = rpc(6, "query/from-leaves", {"file": src_path, "line": line_of("auto fn = [](int param_x)"), "column": 20})
            names5 = flatten_names(t5)
            if "lambda_arg" not in names5:
                print(f"ERROR: LambdaExpr operator() parameter mapping failed: {names5}")
                return False

            # 6. Non-trivial member operator= parameter mapping
            t6 = rpc(7, "query/from-leaves", {"file": src_path, "line": line_of("int box_out ="), "column": 7})
            names6 = flatten_names(t6)
            if "assign_rhs" not in names6:
                print(f"ERROR: CustomBox::operator= parameter mapping failed: {names6}")
                return False

            # 7. Zone::New<OverloadedTarget>(int) constructor overload disambiguation
            t7 = rpc(8, "query/from-leaves", {"file": src_path, "line": line_of("int target_out ="), "column": 7})
            names7 = flatten_names(t7)
            if "ctor_int_seed" not in names7:
                print(f"ERROR: Zone::New<OverloadedTarget>(int) overload disambiguation failed: {names7}")
                return False

            # 7b. Zone::New<TemplatedCtorTarget>(a, b) templated constructor forwarding
            t7b = rpc(81, "query/from-leaves", {"file": src_path, "line": line_of("int tmpl_target_out ="), "column": 7})
            names7b = flatten_names(t7b)
            if "tmpl_ctor_b" not in names7b or "tmpl_ctor_a" in names7b:
                print(f"ERROR: Zone::New<TemplatedCtorTarget>(a, b) templated constructor forwarding failed: {names7b}")
                return False

            # 7c. Multi-hop shifted-argument forwarding (MaglevReducer::AddNewNode -> NodeBase::New -> NodeBase::Allocate)
            t7c = rpc(82, "query/from-leaves", {"file": src_path, "line": line_of("int shifted_offset_out ="), "column": 7})
            names7c = flatten_names(t7c)
            if "shifted_offset_seed" not in names7c or "shifted_input_node" in names7c or "zone" in names7c:
                print(f"ERROR: MaglevReducer::AddNewNode<ShiftedNode> shifted-arg forwarding failed: {names7c}")
                return False

            # 8. atomic-utils passthrough (AsAtomicImpl::Release_Store/Acquire_Load, to_storage_type, to_return_type) & member Acquire_Load()
            t8 = rpc(9, "query/from-leaves", {"file": src_path, "line": line_of("int member_acquire_out ="), "column": 7})
            names8 = flatten_names(t8)
            if "atomic_seed" not in names8 or "atomic_cell" not in names8:
                print(f"ERROR: atomic-utils passthrough & member Acquire_Load flow failed: {names8}")
                return False

            # 9. ASSIGN_RETURN_ON_EXCEPTION + BigInt::FromObject(isolate, value) -> converted_value (must trace to bigint_input_seed, NOT isolate)
            t9 = rpc(10, "query/from-leaves", {"file": src_path, "line": line_of("DirectHandle<int> converted_value"), "column": 21})
            names9 = flatten_names(t9)
            if "bigint_input_seed" not in names9 or "isolate" in names9 or "kNullMaybe" in names9:
                print(f"ERROR: BigInt::FromObject ASSIGN_RETURN_ON_EXCEPTION flow failed or leaked isolate/kNullMaybe: {names9}")
                return False

            # 10. Read-only root getter force-termination on Factory/FactoryBase, ReadOnlyRoots, and RootsTable
            for var_decl, expected_leaf in [
                ("DirectHandle<int> root_val =", "undefined_value()"),
                ("int ro_val =", "the_hole_value()"),
                ("int rt_val =", "empty_fixed_array()"),
            ]:
                col = lines[line_of(var_decl) - 1].index(var_decl.split()[1]) + 1
                t_ro = rpc(11, "query/incoming", {"file": src_path, "line": line_of(var_decl), "column": col})
                ro_names = flatten_names(t_ro)
                if expected_leaf not in ro_names or "roots_" in ro_names or "read_only_roots_" in ro_names or any("_return" in n for n in ro_names):
                    print(f"ERROR: Read-only root getter force-termination failed for {var_decl}: {ro_names}")
                    return False

            # 11. Elision of foo_return in QueryIncoming & QueryFromLeaves (ComputePassthroughValue() -> inner_computed without ComputePassthroughValue_return, preserving call-site FlowLoc)
            t11 = rpc(12, "query/incoming", {"file": src_path, "line": line_of("int call_result ="), "column": 7})
            names11 = flatten_names(t11)
            if "ComputePassthroughValue()" not in names11 or "inner_computed" not in names11 or "ComputePassthroughValue_return" in names11:
                print(f"ERROR: foo_return elision in QueryIncoming failed: {names11}")
                return False
            if "call_seed" not in names11 or "other_call_seed" in names11:
                print(f"ERROR: Call-site context sensitivity in QueryIncoming failed for call_result (expected call_seed and not other_call_seed): {names11}")
                return False

            t12 = rpc(14, "query/incoming", {"file": src_path, "line": line_of("int other_call_result ="), "column": 7})
            names12 = flatten_names(t12)
            if "other_call_seed" not in names12 or "call_seed" in names12:
                print(f"ERROR: Call-site context sensitivity in QueryIncoming failed for other_call_result (expected other_call_seed and not call_seed): {names12}")
                return False

            t11_leaves = rpc(13, "query/from-leaves", {"file": src_path, "line": line_of("int call_result ="), "column": 7})
            names11_leaves = flatten_names(t11_leaves)
            if "call_seed" not in names11_leaves or "other_call_seed" in names11_leaves:
                print(f"ERROR: Call-site context sensitivity in QueryFromLeaves failed for call_result: {names11_leaves}")
                return False
            leaves_flow_lines = {}
            def collect_flow_lines(node):
                if not node:
                    return
                if node.get("node"):
                    leaves_flow_lines[node["node"]["name"]] = node.get("flow_location", {}).get("line")
                for c in node.get("children", []):
                    collect_flow_lines(c)
            collect_flow_lines(t11_leaves)
            expected_call_line = line_of("int call_result =")
            if leaves_flow_lines.get("ComputePassthroughValue()") != expected_call_line:
                print(f"ERROR: query/from-leaves overwrote CallExpr flow_location line ({leaves_flow_lines.get('ComputePassthroughValue()')} != {expected_call_line})")
                return False

            # 11. Use-site token resolution: querying on the assignment line `reassigned_var = use_site_seed;` resolves `reassigned_var` and includes `use_site_seed`
            t13 = rpc(15, "query/incoming", {"file": src_path, "line": line_of("reassigned_var = use_site_seed;"), "column": 5})
            names13 = flatten_names(t13)
            if "reassigned_var" not in names13 or "use_site_seed" not in names13:
                print(f"ERROR: Use-site token resolution on assignment line failed: {names13}")
                return False

            # 12. Outgoing parameter self-child elimination: querying outgoing on parameter `input_v` must not nest `input_v` as a direct child of itself
            t14 = rpc(16, "query/outgoing", {"file": src_path, "line": line_of("int ComputePassthroughValue(int input_v)"), "column": 34})
            if not t14 or t14.get("node", {}).get("name") != "input_v":
                print(f"ERROR: Failed to resolve parameter input_v for outgoing query: {t14}")
                return False
            for c in t14.get("children", []):
                if c.get("node", {}).get("name") == "input_v":
                    print(f"ERROR: Self-child input_v -> input_v was not eliminated in query/outgoing: {t14}")
                    return False

            # 13. Default argument values:
            # 13a. Call site omitting argument (`ComputeWithDefault()`) receives the default value `4242` and NOT `explicit_override_seed`
            t15a = rpc(17, "query/from-leaves", {"file": src_path, "line": line_of("int default_arg_out ="), "column": 7})
            names15a = flatten_names(t15a)
            if "4242" not in names15a or "explicit_override_seed" in names15a:
                print(f"ERROR: Default argument flow on omitted call site failed: {names15a}")
                return False

            # 13b. Call site passing explicit argument (`ComputeWithDefault(explicit_override_seed)`) receives `explicit_override_seed` and overrides default `4242`
            t15b = rpc(18, "query/from-leaves", {"file": src_path, "line": line_of("int explicit_arg_out ="), "column": 7})
            names15b = flatten_names(t15b)
            if "explicit_override_seed" not in names15b or "4242" in names15b:
                print(f"ERROR: Explicit argument override of default parameter failed: {names15b}")
                return False

            # 13c. Querying directly on the parameter `default_param` (no call stack context) includes BOTH the declaration default `4242` and caller `explicit_override_seed`
            t15c = rpc(19, "query/from-leaves", {"file": src_path, "line": line_of("int ComputeWithDefault(int default_param = 4242)"), "column": 29})
            names15c = flatten_names(t15c)
            if "4242" not in names15c or "explicit_override_seed" not in names15c:
                print(f"ERROR: Direct query on parameter with default value failed to include both default and explicit caller: {names15c}")
                return False
            if t15c.get("value_range") != {"min": 4242, "max": 4343, "display": "[4242, 4343]"}:
                print(f"ERROR: Expected union value_range [4242, 4343] on default_param, got: {t15c.get('value_range')}")
                return False

            # 14. Integral value range propagation across BitField, constexpr constants, *, +, and &
            # Here seed_mode = 7 flows into ModeBits (4-bit field, max [0, 15]), so obj.mode() has exact range [7, 7],
            # and computed_field_offset = obj.mode() * 4 + 8 has range [36, 36].
            t16_in = rpc(20, "query/incoming", {"file": src_path, "line": line_of("int computed_field_offset ="), "column": 7})
            if not t16_in or t16_in.get("value_range") != {"min": 36, "max": 36, "display": "[36]"}:
                print(f"ERROR: Expected value_range [36] on computed_field_offset (incoming), got: {t16_in.get('value_range') if t16_in else None}")
                return False

            t16_leaves = rpc(21, "query/from-leaves", {"file": src_path, "line": line_of("int computed_field_offset ="), "column": 7})
            if not t16_leaves or t16_leaves.get("value_range") != {"min": 36, "max": 36, "display": "[36]"}:
                print(f"ERROR: Expected value_range [36] on computed_field_offset (from-leaves), got: {t16_leaves.get('value_range') if t16_leaves else None}")
                return False

            t17_in = rpc(22, "query/incoming", {"file": src_path, "line": line_of("int masked_low_bits ="), "column": 7})
            if not t17_in or t17_in.get("value_range") != {"min": 3, "max": 3, "display": "[3]"}:
                print(f"ERROR: Expected value_range [3] on masked_low_bits (4343 & 3), got: {t17_in.get('value_range') if t17_in else None}")
                return False

            # 15. Heap read taint tracking (sandbox vs trusted) and storage-range widening on heap fields
            t18_sandbox = rpc(23, "query/incoming", {"file": src_path, "line": line_of("int sandbox_size_bytes ="), "column": 7})
            if not t18_sandbox or t18_sandbox.get("heap_taint") != "sandbox" or t18_sandbox.get("value_range") != {"min": 0, "max": 1020, "display": "[0, 1020]"}:
                print(f"ERROR: Expected heap_taint='sandbox' and widened storage value_range [0, 1020] (unsigned char [0,255] * 4) on sandbox_size_bytes, got: {t18_sandbox}")
                return False

            t18_trusted = rpc(24, "query/incoming", {"file": src_path, "line": line_of("int trusted_frame_bytes ="), "column": 7})
            if not t18_trusted or t18_trusted.get("heap_taint") != "trusted" or t18_trusted.get("value_range") != {"min": 8, "max": 24, "display": "[8, 24]"}:
                print(f"ERROR: Expected heap_taint='trusted' and unclobbered value_range [8, 24] ([0, 16] + 8) on trusted_frame_bytes, got: {t18_trusted}")
                return False

            t18_combined = rpc(25, "query/from-leaves", {"file": src_path, "line": line_of("int combined_taint_bytes ="), "column": 7})
            if not t18_combined or t18_combined.get("heap_taint") != "sandbox" or t18_combined.get("value_range") != {"min": 8, "max": 1044, "display": "[8, 1044]"}:
                print(f"ERROR: Expected combined heap_taint='sandbox' and value_range [8, 1044] on combined_taint_bytes, got: {t18_combined}")
                return False
            for leaf_c in t18_combined.get("children", []):
                lname = leaf_c.get("node", {}).get("name")
                if lname == "10" and not leaf_c.get("clobbered_by_heap"):
                    print(f"ERROR: Expected sandbox leaf '10' to have clobbered_by_heap=True, got: {leaf_c}")
                    return False
                if lname == "16" and leaf_c.get("clobbered_by_heap"):
                    print(f"ERROR: Expected trusted leaf '16' NOT to be clobbered_by_heap, got: {leaf_c}")
                    return False

            # 16. Dead parameter marking (UncalledHelperWithDeadParam::dead_param has zero incoming ArgPass edges)
            t19_dead = rpc(26, "query/from-leaves", {"file": src_path, "line": line_of("int dead_derived = dead_param + 1;"), "column": 7})
            if not t19_dead or not t19_dead.get("is_dead"):
                print(f"ERROR: Expected is_dead=True on dead_derived, got: {t19_dead}")
                return False

            # 17. Deduplication of non-dependent expressions across class template instantiations (MakeRegIndex() in MockRegisterFrameArray<T>)
            t20_tmpl = rpc(27, "query/from-leaves", {"file": src_path, "line": line_of("int first_param = MakeRegIndex(0);"), "column": 9})
            names20 = flatten_names(t20_tmpl)
            if names20.count("MakeRegIndex()") != 1:
                print(f"ERROR: Expected MakeRegIndex() to appear exactly once across MockRegisterFrameArray<T> instantiations, got {names20.count('MakeRegIndex()')} in {names20}")
                return False

            # 18. Template specialization splitting across CRTP MockTaggedArrayBase<D, ElementT>::get()
            t21_spec = rpc(28, "query/from-leaves", {"file": src_path, "line": line_of("int combined_array_sink ="), "column": 7})
            top_leaves21 = t21_spec.get("children", []) if t21_spec else []
            flex_leaves21 = [c for c in top_leaves21 if c.get("node", {}).get("name") == "flexible_array_member_data_"]
            if len(flex_leaves21) != 2:
                print(f"ERROR: Expected exactly 2 flexible_array_member_data_ leaves for combined_array_sink (excluding MockUnusedArray), got {len(flex_leaves21)}: {[c.get('node') for c in flex_leaves21]}")
                return False
            fixed_flex_line = line_of("class MockFixedArray") + 2
            list_flex_line = line_of("class MockArrayList") + 2
            for flex_leaf in flex_leaves21:
                leaf_line = flex_leaf["node"]["location"]["line"]
                subtree_names = flatten_names(flex_leaf)
                if leaf_line == fixed_flex_line:
                    if "fixed_elem_read" not in subtree_names or "list_elem_read" in subtree_names or "unused_elem_read" in subtree_names:
                        print(f"ERROR: MockFixedArray::flexible_array_member_data_ leaked across specializations: {subtree_names}")
                        return False
                elif leaf_line == list_flex_line:
                    if "list_elem_read" not in subtree_names or "fixed_elem_read" in subtree_names or "unused_elem_read" in subtree_names:
                        print(f"ERROR: MockArrayList::flexible_array_member_data_ leaked across specializations: {subtree_names}")
                        return False
                else:
                    print(f"ERROR: Unexpected flexible_array_member_data_ leaf line {leaf_line} (expected {fixed_flex_line} or {list_flex_line})")
                    return False

            # 19. Out-of-line class template method parameter unification (MockTemplateReducer<BaseT>::BuildLoadField offset)
            ool_line = line_of("int MockTemplateReducer<BaseT>::BuildLoadField(int object, int offset)")
            ool_col = lines[ool_line - 1].index("offset)") + 1
            t22_ool = rpc(29, "query/from-leaves", {"file": src_path, "line": ool_line, "column": ool_col})
            names22 = flatten_names(t22_ool)
            if "field_index_offset_seed" not in names22 or "field_idx_offset" not in names22:
                print(f"ERROR: Out-of-line class template method parameter query failed to trace field_index_offset_seed: {names22}")
                return False

            sink_line = line_of("int NonTemplateOffsetSink(int offset_arg)")
            sink_col = lines[sink_line - 1].index("offset_arg") + 1
            t22_sink = rpc(2901, "query/from-leaves", {"file": src_path, "line": sink_line, "column": sink_col})
            dead_sink_leaves = [c.get("node", {}).get("name") for c in t22_sink.get("children", []) if c.get("is_dead")]
            if dead_sink_leaves:
                print(f"ERROR: Uninstantiated template pattern parameter leaked as dead leaf into NonTemplateOffsetSink: {dead_sink_leaves}")
                return False

            # 20. Unary increment (i++) assignment edge, dynamic for-loop bound ([0, 190]), unbounded while loop ([0, 2^31-1]), and constant loop bound ([0, 16])
            loop_line = line_of("for (int loop_idx = 0; loop_idx < loop_bound; loop_idx++)")
            loop_col = lines[loop_line - 1].index("loop_idx = 0") + 1
            t23_inc = rpc(30, "query/incoming", {"file": src_path, "line": loop_line, "column": loop_col})
            names23 = flatten_names(t23_inc)
            if "0" not in names23 or "++" not in names23 or "loop_bound" not in names23:
                print(f"ERROR: Expected '0', '++', and 'loop_bound' in incoming flow of loop_idx, got: {names23}")
                return False
            if t23_inc.get("value_range") != {"min": 0, "max": 190, "display": "[0, 190]"}:
                print(f"ERROR: Expected dynamic-bounded value_range [0, 190] on loop_idx (incoming), got: {t23_inc.get('value_range')}")
                return False

            t23_leaves = rpc(31, "query/from-leaves", {"file": src_path, "line": line_of("loop_sink = ComputePassthroughValue(loop_idx);"), "column": 39})
            if not t23_leaves or t23_leaves.get("value_range") != {"min": 0, "max": 190, "display": "[0, 190]"}:
                print(f"ERROR: Expected dynamic-bounded value_range [0, 190] on loop_idx (from-leaves), got: {t23_leaves.get('value_range') if t23_leaves else None}")
                return False

            t23_unbounded = rpc(33, "query/from-leaves", {"file": src_path, "line": line_of("int unbounded_sink = ComputePassthroughValue(unbounded_idx);"), "column": 48})
            if not t23_unbounded or t23_unbounded.get("value_range") != {"min": 0, "max": 2147483647, "display": "[0, 2^31-1]"}:
                print(f"ERROR: Expected widened value_range [0, 2^31-1] on unbounded_idx (from-leaves), got: {t23_unbounded.get('value_range') if t23_unbounded else None}")
                return False

            t23_bounded = rpc(32, "query/incoming", {"file": src_path, "line": line_of("bounded_loop_sink = bounded_idx;"), "column": 25})
            if not t23_bounded or t23_bounded.get("value_range") != {"min": 0, "max": 16, "display": "[0, 16]"}:
                print(f"ERROR: Expected constant-bounded loop value_range [0, 16] on bounded_idx, got: {t23_bounded.get('value_range') if t23_bounded else None}")
                return False

            # 21. Public API (include/) parameters are never dead, accept arbitrary embedder values, and carry embedder taint
            t24_uncalled_inc = rpc(34, "query/incoming", {"file": src_path, "line": line_of("int api_uncalled_sink = api_uncalled_param;"), "column": 9})
            if not t24_uncalled_inc or t24_uncalled_inc.get("is_dead") or t24_uncalled_inc.get("heap_taint") != "embedder" or t24_uncalled_inc.get("value_range") != {"min": -2147483648, "max": 2147483647, "display": "[-2^31, 2^31-1]"}:
                print(f"ERROR: Expected live [-2^31, 2^31-1] with heap_taint='embedder' on uncalled include/ API parameter sink (incoming), got: is_dead={t24_uncalled_inc.get('is_dead') if t24_uncalled_inc else None}, taint={t24_uncalled_inc.get('heap_taint') if t24_uncalled_inc else None}, range={t24_uncalled_inc.get('value_range') if t24_uncalled_inc else None}")
                return False
            t24_called_leaves = rpc(35, "query/from-leaves", {"file": src_path, "line": line_of("int api_called_sink = api_called_param;"), "column": 9})
            names24 = flatten_names(t24_called_leaves)
            if not t24_called_leaves or t24_called_leaves.get("is_dead") or t24_called_leaves.get("heap_taint") != "embedder" or t24_called_leaves.get("value_range") != {"min": 0, "max": 65535, "display": "[0, 65535]"} or "5" not in names24 or "api_called_param" not in names24:
                print(f"ERROR: Expected live [0, 65535] with heap_taint='embedder' and both '5' and 'api_called_param' leaves on include/ API parameter sink (from-leaves), got: taint={t24_called_leaves.get('heap_taint') if t24_called_leaves else None}, range={t24_called_leaves.get('value_range') if t24_called_leaves else None}, names={names24}")
                return False

            print("SUCCESS: All 25 visitor, offset field, atomic-utils, ASSIGN_RETURN_ON_EXCEPTION, read-only root, foo_return elision, call-site context sensitivity, use-site resolution, self-child elimination, default argument value, integral value range, heap taint, dead parameter, template expression deduplication, template specialization splitting, unary increment, and public API include/ assertions passed!")
            return True
        finally:
            proc.terminate()
            proc.wait()


def main():
    tests_dir = os.path.dirname(os.path.abspath(__file__))
    workspace_root = os.path.dirname(tests_dir)

    binary = os.path.join(workspace_root, "build", "analyzer")
    test_project_dir = os.path.join(tests_dir, "test_project")
    test_project_build = os.path.join(test_project_dir, "build")
    test_project_cache = os.path.join(test_project_dir, ".cache")

    verbose = "--verbose" in sys.argv

    print(f"Workspace Root: {workspace_root}")
    print(f"Analyzer Binary: {binary}")

    if not os.path.exists(binary):
        print(f"ERROR: Analyzer binary not found at {binary}. Please build it first.")
        sys.exit(1)

    # 1. Ensure test project compile_commands.json is generated
    print("\n--- Preparing Test Project ---")
    os.makedirs(test_project_build, exist_ok=True)
    cmake_cmd = ["cmake", "-B", "build", "-S", "."]
    print(f"Running CMake in test project: {' '.join(cmake_cmd)}")
    res = subprocess.run(cmake_cmd, cwd=test_project_dir, capture_output=True, text=True)
    if res.returncode != 0:
        print("ERROR: Failed to configure test project with CMake:")
        print(res.stderr)
        sys.exit(1)
    print("Test project configured successfully (compile_commands.json generated).")

    # 2. Run CLI DFG Test
    cli_passed = run_cli_test(binary, os.path.join(test_project_dir, "main.cc"))
    if not cli_passed:
        print("\nERROR: CLI Test FAILED!")
        sys.exit(1)

    # 3. Run Standalone Indexing Test
    standalone_passed = run_standalone_index_test(binary, test_project_dir, test_project_build, test_project_cache)
    if not standalone_passed:
        print("\nERROR: Standalone Indexing Test FAILED!")
        sys.exit(1)

    # 3.5. Run Selective CLI File Indexing Test
    selective_passed = run_selective_index_test(binary, test_project_dir, test_project_build, test_project_cache)
    if not selective_passed:
        print("\nERROR: Selective CLI File Indexing Test FAILED!")
        sys.exit(1)

    # 3.8. Run Advanced V8 Compiler DFG Integration Tests
    v8_advanced_passed = run_v8_advanced_features_test(binary, test_project_dir, test_project_build, test_project_cache)
    if not v8_advanced_passed:
        print("\nERROR: Advanced V8 Compiler DFG Integration Tests FAILED!")
        sys.exit(1)

    # 3.9. Run Visitor Edge Cases & Offset Field Regression Tests
    visitor_cases_passed = run_visitor_edge_cases_test(binary)
    if not visitor_cases_passed:
        print("\nERROR: Visitor Edge Cases Regression Test FAILED!")
        sys.exit(1)

    # 4. Run Server Test
    server_passed = run_server_test(binary, test_project_dir, test_project_build, test_project_cache, verbose)
    if not server_passed:
        print("\nERROR: Server Test FAILED!")
        sys.exit(1)

    print("\n==================================")
    print("  ALL AUTOMATED TESTS PASSED!")
    print("==================================")

if __name__ == "__main__":
    main()
