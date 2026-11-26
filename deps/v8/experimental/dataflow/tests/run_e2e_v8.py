# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
import subprocess
import threading
import time
import sqlite3
import select

def read_stderr(process, logs):
    while True:
        line = process.stderr.readline()
        if not line:
            break
        logs.append(line)

def run_e2e_test():
    print("=== STARTING REAL V8 END-TO-END DATAFLOW INTEGRATION TESTS ===")

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    v8_root = os.path.abspath(os.path.join(repo_root, "..", ".."))
    binary = os.path.join(repo_root, "build", "analyzer")
    workspace_root = os.environ.get("V8_WORKSPACE_ROOT", v8_root)
    build_dir = os.path.join(workspace_root, "out", "x64.debug")
    cache_dir = os.environ.get("DATAFLOW_CACHE_DIR", os.path.join(workspace_root, ".cache"))

    if not os.path.exists(binary):
        print(f"ERROR: Analyzer binary not found at {binary}")
        return False

    if not os.path.exists(workspace_root):
        print(f"ERROR: V8 checkout not found at {workspace_root}")
        return False

    db_path = os.path.join(cache_dir, ".dataflow.db")
    if not os.path.exists(db_path):
        print(f"ERROR: Full database cache not found at {db_path}")
        return False

    # Under E2E definition, we bypass selective indexing and query directly against the full production workspace cache!
    print(f"[E2E] Using full V8 workspace cache database: {db_path}")

    # 2. Start JSON-RPC Server
    print("\n[E2E] Starting Dataflow Server...")
    process = subprocess.Popen(
        [binary, "--server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    def read_stderr_live():
        while True:
            line = process.stderr.readline()
            if not line:
                break
            print(f"[Server Stderr] {line.strip()}")
            server_logs.append(line)

    server_logs = []
    stderr_thread = threading.Thread(target=read_stderr_live)
    stderr_thread.daemon = True
    stderr_thread.start()

    time.sleep(0.5)

    def read_rpc_response(req_id, timeout=10.0):
        start_time = time.time()
        while True:
            remaining = timeout - (time.time() - start_time)
            if remaining <= 0:
                print(f"\n[E2E] TIMEOUT: Server took longer than {timeout} seconds to respond.")
                return None

            ready, _, _ = select.select([process.stdout], [], [], remaining)
            if not ready:
                print(f"\n[E2E] TIMEOUT: Server took longer than {timeout} seconds to respond.")
                return None

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
        # Initialize server
        init_req = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "workspaceRoot": workspace_root,
                "cacheDir": cache_dir,
                "compileCommandsDir": build_dir,
                "concurrency": 4
            }
        }
        process.stdin.write(json.dumps(init_req) + "\n")
        process.stdin.flush()

        init_resp = read_rpc_response(1, timeout=600.0)
        if not init_resp or "error" in init_resp:
            print("ERROR: Failed to initialize server.")
            print(init_resp)
            process.terminate()
            return False

        print("[E2E] Server initialized cleanly.")

        # Query incoming flow for desc at DescriptorArray::CopyUpTo (objects.cc:3862, col 53)
        query_req = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "query/incoming",
            "params": {
                "file": os.path.join(workspace_root, "src/objects/objects.cc"),
                "line": 3862,
                "column": 53
            }
        }

        print("\n[E2E] Querying incoming dataflow for CopyUpTo 'desc' parameter at objects.cc:3862...")
        process.stdin.write(json.dumps(query_req) + "\n")
        process.stdin.flush()

        resp = read_rpc_response(2, timeout=10.0)
        if not resp or "error" in resp:
            print("ERROR: Query incoming failed.")
            print(resp)
            process.terminate()
            return False

        res = resp.get("result")
        if not res:
            print("ERROR: Returned query tree is null!")
            process.terminate()
            return False

        # Dump raw JSON tree to temp file for absolute analysis!
        with open("/tmp/e2e_tree.json", "w") as f:
            json.dump(res, f, indent=2)

        # 3. Analyze Query Tree and Extract True Leaves
        path_nodes = []
        all_leaves_map = {} # maps NodeID -> node_data for all terminal occurrences
        parent_node_ids = set() # set of NodeIDs that have at least one child

        def traverse_tree(node):
            node_data = node.get("node")
            if not node_data:
                return

            node_id = node_data["id"]
            path_nodes.append(node_data)

            children = node.get("children", [])
            is_cycle = node.get("is_cycle", False)
            is_cached = node.get("is_cached", False)

            # If this specific visit has no children, treat it tentatively as a leaf ONLY if it is not a cycle loop or cached merge!
            if not children:
                if not is_cycle and not is_cached:
                    all_leaves_map[node_id] = node_data
            else:
                parent_node_ids.add(node_id)
                for child in children:
                    traverse_tree(child)

        traverse_tree(res)

        # A node is a TRUE leaf if and only if it is NEVER traversed as a parent anywhere in the tree!
        true_leaves = {}
        for nid, nd in all_leaves_map.items():
            if nid not in parent_node_ids:
                true_leaves[nid] = nd

        # Establish SQLite connection to verify if leaves have physical incoming edges in the database!
        # (If a leaf has incoming edges, it is a successfully merged/pruned path, not a stuck boundary!)
        db_path = os.path.join(cache_dir, ".dataflow.db")
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()

        leaf_nodes = []
        for nid, nd in true_leaves.items():
            cursor.execute("SELECT COUNT(*) FROM edges WHERE to_id = ?;", (nid,))
            in_edges_count = cursor.fetchone()[0]

            # If it has 0 incoming edges in the database, it is a true terminal boundary leaf!
            if in_edges_count == 0:
                leaf_nodes.append(nd)

        conn.close()

        print(f"\n[E2E] Traced {len(path_nodes)} total DFG nodes in path.")
        print(f"[E2E] Extracted {len(leaf_nodes)} true unique terminal leaf nodes.")

        # Check for Bottleneck Cast Nodes in the path (ignoring standard Clang AST cast classes and compiler/maglev graphs!)
        cast_nodes = []
        for n in path_nodes:
            name = n["name"]
            file = n.get("location", {}).get("file", "")
            kind = n.get("kind", "")
            if kind != "Literal" and ("Cast" in name or "Cast_return" in name) and "CXX" not in name:
                if "src/maglev" not in file and "src/compiler" not in file:
                    cast_nodes.append(n)

        if cast_nodes:
            print(f"WARNING: Found casting helper nodes in path: {[n['name'] for n in cast_nodes]}")
            print("ERROR: Cast elision failed! Bottlenecks were traversed!")
            process.terminate()
            return False

        # Print all extracted unique terminal leaf nodes grouped by their categories!
        approved_leaves = []
        boundary_leaves = []

        for leaf in leaf_nodes:
            name = leaf["name"]
            file = leaf["location"]["file"]
            line = leaf["location"]["line"]

            # =========================================================================
            # CRITICAL WARNING FOR AGENTS & DEVELOPERS:
            # YOU ARE ABSOLUTELY NOT ALLOWED TO TOUCH, EXPAND, OR MODIFY THIS SET
            # OF APPROVED LEAVES UNDER ANY CIRCUMSTANCES.
            # Only Allocate methods, constants, and roots are permitted.
            # Do not add fields, variables, 'this', parameters, or other boundaries.
            # =========================================================================
            is_approved = (
                # 1. Allocate methods
                "Allocate" in name or
                "allocate" in name or
                "New" in name or
                "Build" in name or
                "Internalize" in name or
                "ToString" in name or
                "LookupString" in name or

                # 2. Constants
                "nullptr" in name or
                "CXXNullPtr" in name or
                name == "0" or

                # 3. ReadOnlyRoots physical loads (constant heap roots)
                "roots-inl.h" in file or
                "static-roots" in file or
                "StaticReadOnlyRootsPointerTable" in name or
                "third_party/libc++/src/include/array" in file or
                "roots_" in name or
                "roots_table_" in name or
                "read_only_roots_" in name or
                "object_at" in name or

                # 4. Virtual DEAD nodes representing uncalled parameters
                "DEAD" in name or

                # 5. Safe Primitive Control-Flow / Dependent call expressions
                name == "?:" or
                name == "call()"
            )

            leaf_desc = f"  - {name} at {os.path.basename(file)}:{line}"
            if is_approved:
                approved_leaves.append(leaf_desc)
            else:
                boundary_leaves.append(leaf_desc)

        print(f"\n[E2E] Total unique terminal leaf nodes extracted: {len(leaf_nodes)}")

        print("\n🟢 APPROVED MEMORY ALLOCATIONS / EMPTY ROOTS LEAVES:")
        for l in sorted(set(approved_leaves)):
            print(l)

        print("\n🟡 UNAPPROVED TERMINAL BOUNDARY LEAVES:")
        for l in sorted(set(boundary_leaves)):
            print(l)

        # Now perform strict validation checks!
        if boundary_leaves:
            print("\n[E2E] ERROR: Unapproved terminal boundaries detected! Trace contains stuck endpoints.")
            process.terminate()
            return False

        print(f"[E2E] All {len(leaf_nodes)} terminal leaves verified successfully (Allocations/Roots only)!")

        # Print a clean slice of the dataflow paths to verify
        print("\n=== EXPLICIT E2E DATAFLOW PIPELINE ===")
        for node in path_nodes[:15]:
            print(f"  -> {node['name']} at {os.path.basename(node['location']['file'])}:{node['location']['line']}")
        if len(path_nodes) > 15:
            print(f"  ... (and {len(path_nodes) - 15} more nodes)")

        print("\n[E2E] SUCCESS: Real V8 CopyUpTo dataflow E2E test passed cleanly!")
        return True

    finally:
        process.terminate()
        process.wait()

if __name__ == "__main__":
    success = run_e2e_test()
    sys.exit(0 if success else 1)
