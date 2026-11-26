#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Before-vs-After evaluation harness for the V8 minimize-reproducer skill."""

import argparse
import datetime
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import uuid
from typing import Any, Dict, List, Optional, Sequence

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_REPO_ROOT = os.path.abspath(
    os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
EVAL_PROMPT_DIR = os.path.join("agents", "prompts", "eval",
                               "minimize-reproducer")

FAKE_D8_SH = """\
#!/usr/bin/env bash
set -u
has_expose_gc=0
file=""
for arg in "$@"; do
  if [[ "$arg" == "--expose-gc" ]]; then
    has_expose_gc=1
  elif [[ "$arg" != --* ]]; then
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Error reading file" >&2
  exit 2
fi
# Basic brace balance check so partial deletions fail cleanly with SyntaxError.
if ! python3 - "$file" <<'PY'
import sys
src = open(sys.argv[1], encoding="utf-8").read()
if src.count("(") != src.count(")") or src.count("{") != src.count("}"):
    print("SyntaxError: Unexpected end of input")
    sys.exit(1)
sys.exit(0)
PY
then
  exit 1
fi
if [[ "$has_expose_gc" -eq 1 ]] && grep -q "TRIGGER_DCHECK" "$file"; then
  echo "Fatal error in ../../src/objects/js-objects.cc, line 42"
  echo "# Debug check failed: target_invariant()."
  exit 134
fi
exit 0
"""


def build_bloated_poc_js() -> str:
  """Builds a ~102-line synthetic reproducer with dead helper noise."""
  lines = [
      "// Copyright 2026 fuzzer seed - do not keep dead boilerplate.",
      "function deadHelperA(x) {",
      "  let acc = 0;",
      "  for (let i = 0; i < 4; ++i) {",
      "    acc += (x ^ i);",
      "  }",
      "  return acc;",
      "}",
      "function deadHelperB(arr) {",
      "  return arr.map((v) => v + 1).filter((v) => v > 0);",
      "}",
  ]
  for idx in range(1, 46):
    lines.append(f"const __noise_{idx} = {idx} * 2;")
  lines.extend([
      "try {",
      "  deadHelperA(12);",
      "  deadHelperB([1, 2, 3]);",
      "} catch (e) {}",
  ])
  for idx in range(46, 86):
    lines.append(f"const __pad_{idx} = ({idx} + 1) & 255;")
  lines.extend([
      "function TRIGGER_DCHECK() { return 42; }",
      "TRIGGER_DCHECK();",
  ])
  return "\n".join(lines) + "\n"


def build_semantic_d8_sh(d8_bin: str) -> str:
  """Builds a wrapper around real d8 for semantic minimization evals."""
  return f"""#!/usr/bin/env bash
set -u
D8_BIN={json.dumps(d8_bin)}
flags=()
file=""
for arg in "$@"; do
  if [[ "$arg" == --* ]]; then
    flags+=("$arg")
  else
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Error reading file" >&2
  exit 2
fi
prelude='let __gc_ran = false; if (typeof gc === "function") {{ const __orig_gc = gc; gc = function() {{ __gc_ran = true; return __orig_gc(); }}; }} function triggerTarget(n) {{ if (__gc_ran && n > 100) {{ print("Fatal error: DCHECK failed: target_invariant()"); quit(134); }} }}'
if [[ -x "$D8_BIN" ]]; then
  exec "$D8_BIN" "${{flags[@]}}" -e "$prelude" "$file"
fi
echo "Missing d8 binary at $D8_BIN" >&2
exit 2
"""


def build_semantic_poc_js() -> str:
  """Builds a bloated reproducer requiring both ddmin and semantic rewrite."""
  lines = [
      "// Copyright 2026 fuzzer seed - contains dead noise and wrapper bloat.",
      "function __deadHelper(x) {",
      "  let s = 0;",
      "  for (let i = 0; i < 4; ++i) s += x;",
      "  return s;",
      "}",
  ]
  for idx in range(1, 51):
    lines.append(f"const __noise_{idx} = {idx} * 3;")
  lines.extend([
      "__deadHelper(7);",
      "class __FuzzerEnv {",
      "  static compute(__unused_a, __unused_b, __v_x) {",
      "    try {",
      "      if (true) {",
      "        return __v_x + 0;",
      "      }",
      "    } catch (__e) {}",
      "  }",
      "}",
      "let __v_last = 0;",
      "for (let __i = 0; __i < 200; ++__i) {",
      '  __v_last = __FuzzerEnv.compute("dead", 999, __i);',
      "}",
      "if (__v_last > 100) {",
      "  gc();",
      "  triggerTarget(__v_last);",
      "}",
  ])
  return "\n".join(lines) + "\n"


def build_582176947_d8_sh(d8_bin: str) -> str:
  """Builds a wrapper around real d8 modeling historic crbug/582176947."""
  return f"""#!/usr/bin/env bash
set -u
D8_BIN={json.dumps(d8_bin)}
flags=()
file=""
for arg in "$@"; do
  if [[ "$arg" == --* ]]; then
    flags+=("$arg")
  else
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Error reading file" >&2
  exit 2
fi
if [[ ! -x "$D8_BIN" ]]; then
  echo "Missing d8 binary at $D8_BIN" >&2
  exit 2
fi
"$D8_BIN" "${{flags[@]}}" "$file" >/dev/null || exit $?
if python3 - "$file" <<'PY'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
stripped = re.sub(r"//.*|/\\*[\\s\\S]*?\\*/", "", src)
m = re.search(r"async\\s+function\\s+(\\w+)\\s*\\([^)]*\\)\\s*\\{{([\\s\\S]*)\\}}", stripped)
if not m:
    sys.exit(1)
fname, body = m.group(1), m.group(2)
loops = len(re.findall(r"\\b(?:for|while)\\b", body))
has_await = bool(re.search(r"\\bawait\\b", body))
called = bool(re.search(rf"\\b{{re.escape(fname)}}\\s*\\(", stripped.replace(m.group(0), ""))) or "%OptimizeFunctionOnNextCall" in stripped
if loops >= 2 and has_await and called:
    sys.exit(0)
sys.exit(1)
PY
then
  echo "# Fatal error in ../../src/handles/handles.h, line 202"
  echo "# Check failed: !v8::internal::v8_flags.enable_slow_asserts.value() || (IsDereferenceAllowed())."
  exit 134
fi
exit 0
"""


def build_582176947_poc_js() -> str:
  """Builds a bloated Fuzzilli-style reproducer (case 1)."""
  lines = [
      "// Copyright 2026 fuzzer seed.",
      "function __deadHelper(x) {",
      "  let acc = 0;",
      "  for (let i = 0; i < 4; ++i) acc += (x ^ i);",
      "  return acc;",
      "}",
  ]
  for idx in range(1, 36):
    lines.append(f"const __noise_{idx} = {idx} * 3;")
  lines.extend([
      "__deadHelper(9);",
      "const v3 = new Uint32Array(39);",
      "async function f4(a5, a6) {",
      "  for await (const v7 of v3) {",
      "    const v13 = {",
      '      ["Africa/Monrovia"](a11, ...a12) {},',
      "    };",
      "    for (let v14 = 0; v14 < 5; v14++) {",
      "      for (let __k = 0; __k < 2; __k++) {",
      '        for await (let [v18] of "536870888") {',
      "          class C20 {}",
      "        }",
      "      }",
      "    }",
      "  }",
      "  return a6;",
      "}",
      "const v21 = f4(1, 2);",
  ])
  return "\n".join(lines) + "\n"


def build_577269348_d8_sh(d8_bin: str) -> str:
  """Builds a wrapper around real d8 modeling historic crbug/577269348."""
  return f"""#!/usr/bin/env bash
set -u
D8_BIN={json.dumps(d8_bin)}
flags=()
file=""
has_bg_compile=0
has_old_time=0
for arg in "$@"; do
  if [[ "$arg" == "--stress-background-compile" ]]; then
    has_bg_compile=1
  elif [[ "$arg" == "--bytecode-old-time=0" ]]; then
    has_old_time=1
  fi
  if [[ "$arg" == --* ]]; then
    flags+=("$arg")
  else
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Error reading file" >&2
  exit 2
fi
if [[ ! -x "$D8_BIN" ]]; then
  echo "Missing d8 binary at $D8_BIN" >&2
  exit 2
fi
prelude='let __has_gen = false; let __flushed = false; let __profiling = false; const __flags_ok = '"$has_bg_compile"' && '"$has_old_time"'; const __orig_eval = Realm.eval; Realm.eval = function(r, s) {{ if (__flushed && __profiling && __flags_ok && String(s).includes("function*")) {{ print("# Fatal error in ../../src/codegen/compiler.cc, line 900"); print("# Debug check failed: pos.line > 0."); quit(134); }} if (String(s).includes("function*")) {{ __has_gen = true; }} return __orig_eval(r, s); }}; if (typeof gc === "function") {{ const __orig_gc = gc; gc = function() {{ if (__has_gen) __flushed = true; return __orig_gc(); }}; }} const __orig_prof = console.profile.bind(console); console.profile = function() {{ __profiling = true; return __orig_prof.apply(console, arguments); }};'
exec "$D8_BIN" "${{flags[@]}}" -e "$prelude" "$file"
"""


def build_577269348_poc_js() -> str:
  """Builds a bloated reproducer (case 2)."""
  lines = [
      "// Copyright 2026 bloated fuzzer reproducer.",
      "function __deadFuzzHelper(v) {",
      "  return [v, v + 1, v + 2].reduce((a, b) => a + b, 0);",
      "}",
  ]
  for idx in range(1, 31):
    lines.append(f"const __noise_{idx} = {idx} + 7;")
  lines.extend([
      "__deadFuzzHelper(5);",
      "const __unused_realm = Realm.current();",
      "function __unusedRun(__unused_arg) {",
      '  const __code = "(function* gen() { yield 42; })()";',
      "  try {",
      "    if (true) {",
      "      const __g = Realm.eval(__unused_realm, __code);",
      "      __g.next();",
      "      gc();",
      '      console.profile("fuzz");',
      "      Realm.eval(__unused_realm, __code);",
      "    }",
      "  } catch (__e) {}",
      "}",
      "__unusedRun(99);",
  ])
  return "\n".join(lines) + "\n"


def build_578950019_d8_sh(d8_bin: str) -> str:
  return f"""#!/usr/bin/env bash
set -uo pipefail
D8_BIN="{d8_bin}"
flags=()
file=""
for arg in "$@"; do
  if [[ "$arg" == -* ]]; then
    flags+=("$arg")
  else
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Usage: $0 [flags] <file.js>" >&2
  exit 2
fi
out="$("$D8_BIN" "${{flags[@]}}" "$file" 2>&1)"
rc=$?
if [[ $rc -ne 0 ]]; then
  printf '%s\\n' "$out" >&2
  exit $rc
fi
pre='
globalThis.__evalBigShift = false;
globalThis.__evalGcAfterShift = false;
const __origShift = Array.prototype.shift;
Array.prototype.shift = function(...args) {{
  if (this.length > 100) globalThis.__evalBigShift = true;
  return __origShift.apply(this, args);
}};
if (typeof globalThis.gc === "function") {{
  const __origGc = globalThis.gc;
  globalThis.gc = function(...args) {{
    if (globalThis.__evalBigShift) globalThis.__evalGcAfterShift = true;
    return __origGc.apply(this, args);
  }};
}}
'
post='print("EVAL_OBS:" + (globalThis.__evalBigShift && globalThis.__evalGcAfterShift));'
obs="$("$D8_BIN" "${{flags[@]}}" -e "$pre" "$file" -e "$post" 2>&1)"
obs_rc=$?
if [[ $obs_rc -eq 0 ]] && printf '%s\\n' "$obs" | grep -q "EVAL_OBS:true"; then
  if python3 - "$file" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
src = re.sub(r"//.*", "", src)
src = re.sub(r"/\\*.*?\\*/", "", src, flags=re.S)
has_arguments = ".arguments" in src
has_opt = "%PrepareFunctionForOptimization" in src and "%OptimizeFunctionOnNextCall" in src
sys.exit(0 if (has_arguments and has_opt) else 1)
PY
  then
    echo "# Fatal error in ../../src/compiler/backend/instruction-selector.cc, line 1" >&2
    echo "# Debug check failed: stale filler pointer in MaterializedObjectStore" >&2
    exit 134
  fi
fi
printf '%s\\n' "$out"
exit $rc
"""


def build_578950019_poc_js() -> str:
  lines = [
      "// Fuzz testcase.",
      "let __noise_acc = 0;",
  ]
  for i in range(30):
    lines.append(f"const __noise_{i} = {i * 11 + 3};")
  lines.extend([
      "function __deadFuzzHelper(v) {",
      "  let s = v;",
      "  for (let i = 0; i < 4; i++) s = (s * 3 + i) | 0;",
      "  return s;",
      "}",
      'let elms = ["x"];',
      "for (let i = 1; i <= 150; i++) elms.push(i);",
      "const code = `",
      "function inspect(arg) {",
      "  return bar.arguments;",
      "}",
      "function bar(arr, trigger) {",
      "  if (trigger) {",
      "    arr.shift();",
      "    gc();",
      "    inspect(arr);",
      "  }",
      "}",
      "function foo(x, trigger) {",
      "  let arr = [${elms.map(v => typeof v === 'string' ? `'${v}'` : v).join(',')}];",
      "  bar(arr, trigger);",
      "  return arr.length;",
      "}",
      "%PrepareFunctionForOptimization(foo);",
      "foo(1, false);",
      "foo(1, false);",
      "%OptimizeFunctionOnNextCall(foo);",
      "foo(1, true);",
      "`;",
      "eval(code);",
  ])
  return "\n".join(lines) + "\n"


def build_574021176_d8_sh(d8_bin: str) -> str:
  return f"""#!/usr/bin/env bash
set -uo pipefail
D8_BIN="{d8_bin}"
flags=()
file=""
for arg in "$@"; do
  if [[ "$arg" == -* ]]; then
    flags+=("$arg")
  else
    file="$arg"
  fi
done
if [[ -z "$file" || ! -f "$file" ]]; then
  echo "Usage: $0 [flags] <file.js>" >&2
  exit 2
fi
out="$("$D8_BIN" "${{flags[@]}}" "$file" 2>&1)"
rc=$?
if [[ $rc -ne 0 ]]; then
  printf '%s\\n' "$out" >&2
  exit $rc
fi
if python3 - "$file" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
src = re.sub(r"//.*", "", src)
src = re.sub(r"/\\*.*?\\*/", "", src, flags=re.S)
has_gen = bool(re.search(r"function\\s*\\*", src))
has_extends = bool(re.search(r"class\\s+[^\\n{{]*\\bextends\\b", src))
has_yield = bool(re.search(r"\\byield\\b", src))
has_throw = bool(re.search(r"\\bthrow\\b|assertThrows", src))
has_loop = bool(re.search(r"for\\s*\\(", src))
has_opt = (
    "%PrepareFunctionForOptimization" in src
    and "%OptimizeFunctionOnNextCall" in src
)
sys.exit(
    0
    if (has_gen and has_extends and has_yield and has_throw and (has_loop or has_opt))
    else 1
)
PY
then
  echo "# Fatal error in ../../src/maglev/maglev-graph-builder.cc, line 1" >&2
  echo "# Debug check failed: exposed ContextCell via leaked CLASS_SCOPE" >&2
  exit 134
fi
printf '%s\\n' "$out"
exit $rc
"""


def build_574021176_poc_js() -> str:
  lines = [
      "// Fuzz testcase.",
      "class Base {}",
  ]
  for i in range(30):
    lines.append(f"const __noise_{i} = {i * 7 + 5};")
  lines.extend([
      "function __deadFuzzHelper(a, b) {",
      "  return (a ^ b) + __noise_0;",
      "}",
      "function sink(v) {",
      "  return v;",
      "}",
      "function pass(fn) {",
      "  return fn();",
      "}",
      "const heritage = { q: Base };",
      "const receiver1 = { f(c) { return c; } };",
      "const receiver2 = { f(c) { return sink(c); } };",
      "function* trigger(x, gate, holder, receiver) {",
      "  const capture = () => x;",
      "  receiver.f(",
      "    class extends (gate(), holder.q) {",
      "      constructor() { return super.value; }",
      "    }",
      "  );",
      "  yield capture;",
      "}",
      "function gateThrow() {",
      "  throw new Error('boom');",
      "}",
      "function gateOk() {}",
      "function trainThrows(n) {",
      "  for (let i = 0; i < n; i++) {",
      "    const g = trigger(i, gateThrow, heritage, receiver1);",
      "    try { pass(() => g.next()); } catch (e) {}",
      "  }",
      "}",
      "trainThrows(2000);",
      "const g2 = trigger(1, gateOk, heritage, receiver2);",
      "const leaked = g2.next().value;",
      "typeof leaked;",
  ])
  return "\n".join(lines) + "\n"


def _run(cmd: Sequence[str],
         cwd: str,
         check: bool = True) -> subprocess.CompletedProcess:
  return subprocess.run(
      list(cmd),
      cwd=cwd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      errors="replace",
      check=check,
  )


def _hash_name(seed: str) -> str:
  return "wt_" + hashlib.sha256(seed.encode("utf-8")).hexdigest()[:12]


def _read_prompt(repo_root: str, filename: str) -> str:
  path = os.path.join(repo_root, EVAL_PROMPT_DIR, filename)
  with open(path, "r", encoding="utf-8") as f:
    return f.read().strip()


def _overlay_agents_dir(src_repo: str, dst_repo: str) -> None:
  """Copies current working-tree agents/ edits into an After eval worktree."""
  src_agents = os.path.join(src_repo, "agents")
  dst_agents = os.path.join(dst_repo, "agents")
  shutil.copytree(src_agents, dst_agents, dirs_exist_ok=True)


def _strip_eval_leaks(wt_dir: str) -> None:
  """Removes eval grader scripts, eval prompts, and checked-in reference tests."""
  shutil.rmtree(
      os.path.join(wt_dir, "agents", "prompts", "eval"), ignore_errors=True)
  for base in ("agents", ".agents"):
    for fname in ("eval_minimize_reproducer.py",
                  "eval_minimize_reproducer_test.py"):
      p = os.path.join(wt_dir, base, "skills", "minimize-reproducer", "scripts",
                       fname)
      if os.path.lexists(p):
        os.remove(p)
  for bug_id in ("582176947", "577269348", "578950019", "574021176"):
    for p in glob.glob(
        os.path.join(wt_dir, "test", "mjsunit", "**", f"*{bug_id}*"),
        recursive=True,
    ):
      if os.path.isfile(p) or os.path.islink(p):
        os.remove(p)


def _commit_clean_eval_snapshot(wt_dir: str) -> None:
  """Squashes worktree state onto an orphan HEAD so git status/diff/HEAD cannot leak."""
  _run(["git", "add", "-A"], cwd=wt_dir)
  tree = _run(["git", "write-tree"], cwd=wt_dir).stdout.strip()
  commit = _run(
      ["git", "commit-tree", tree, "-m", "eval baseline"],
      cwd=wt_dir,
  ).stdout.strip()
  _run(["git", "reset", "--soft", commit], cwd=wt_dir)


def _strip_js_comments(src: str) -> str:
  return re.sub(r"//.*|/\*[\s\S]*?\*/", "", src)


def _install_jetski(worktree_dir: str) -> None:
  _run(
      ["vpython3", "agents/scripts/install_for_jetski.py"],
      cwd=worktree_dir,
  )


def _write_fake_d8_and_poc(wt_dir: str) -> None:
  tmp_dir = os.path.join(wt_dir, "tmp")
  os.makedirs(tmp_dir, exist_ok=True)
  d8_sh = os.path.join(tmp_dir, "fake_d8.sh")
  with open(d8_sh, "w", encoding="utf-8") as f:
    f.write(FAKE_D8_SH)
  os.chmod(d8_sh, 0o755)
  poc = os.path.join(tmp_dir, "poc.js")
  with open(poc, "w", encoding="utf-8") as f:
    f.write(build_bloated_poc_js())


def _write_semantic_d8_and_poc(repo_root: str, wt_dir: str) -> None:
  tmp_dir = os.path.join(wt_dir, "tmp")
  os.makedirs(tmp_dir, exist_ok=True)
  d8_bin = os.path.join(repo_root, "out", "x64.release", "d8")
  d8_sh = os.path.join(tmp_dir, "fake_d8_semantic.sh")
  with open(d8_sh, "w", encoding="utf-8") as f:
    f.write(build_semantic_d8_sh(d8_bin))
  os.chmod(d8_sh, 0o755)
  poc = os.path.join(tmp_dir, "poc.js")
  with open(poc, "w", encoding="utf-8") as f:
    f.write(build_semantic_poc_js())


def _write_historic_fixture(repo_root: str, wt_dir: str, sh_name: str,
                            sh_body: str, poc_body: str) -> None:
  del repo_root
  tmp_dir = os.path.join(wt_dir, "tmp")
  os.makedirs(tmp_dir, exist_ok=True)
  sh_path = os.path.join(tmp_dir, sh_name)
  with open(sh_path, "w", encoding="utf-8") as f:
    f.write(sh_body)
  os.chmod(sh_path, 0o755)
  poc_path = os.path.join(tmp_dir, "poc.js")
  with open(poc_path, "w", encoding="utf-8") as f:
    f.write(poc_body)


def setup_eval(
    repo_root: str,
    base_ref: str,
    target_ref: str,
    work_dir: str,
    apply_working_tree: bool,
    variants: Sequence[str] = ("before", "after"),
    case_filter: Optional[Sequence[str]] = None,
) -> Dict[str, Any]:
  """Creates isolated Before/After worktrees and fixtures, returning manifest."""
  os.makedirs(work_dir, exist_ok=True)
  run_id = uuid.uuid4().hex[:8]
  created_worktrees: List[str] = []
  temp_branches: List[str] = []

  cases = [
      {
          "case_id": "minimize_dd",
          "prompt": _read_prompt(repo_root, "minimize_dd_prompt.md"),
      },
      {
          "case_id": "semantic_minimize",
          "prompt": _read_prompt(repo_root, "semantic_minimize_prompt.md"),
      },
      {
          "case_id": "historic_582176947",
          "prompt": _read_prompt(repo_root, "historic_582176947_prompt.md"),
      },
      {
          "case_id": "historic_577269348",
          "prompt": _read_prompt(repo_root, "historic_577269348_prompt.md"),
      },
      {
          "case_id": "historic_578950019",
          "prompt": _read_prompt(repo_root, "historic_578950019_prompt.md"),
      },
      {
          "case_id": "historic_574021176",
          "prompt": _read_prompt(repo_root, "historic_574021176_prompt.md"),
      },
  ]
  if case_filter:
    wanted = set(case_filter)
    cases = [c for c in cases if c["case_id"] in wanted]

  ref_map = {"before": base_ref, "after": target_ref}
  entries: Dict[str, Any] = {}
  for case in cases:
    cid = case["case_id"]
    entries[cid] = {"prompt": case["prompt"], "variants": {}}
    for variant in variants:
      ref = ref_map[variant]
      wt_dir = os.path.join(work_dir, _hash_name(f"{cid}:{variant}:{run_id}"))
      _run(
          ["git", "worktree", "add", "--detach", wt_dir, ref],
          cwd=repo_root,
      )
      created_worktrees.append(wt_dir)
      if variant == "after" and apply_working_tree:
        _overlay_agents_dir(repo_root, wt_dir)
      _strip_eval_leaks(wt_dir)
      _install_jetski(wt_dir)
      _strip_eval_leaks(wt_dir)
      _commit_clean_eval_snapshot(wt_dir)

      if cid == "minimize_dd":
        _write_fake_d8_and_poc(wt_dir)
      elif cid == "semantic_minimize":
        _write_semantic_d8_and_poc(repo_root, wt_dir)
      elif cid == "historic_582176947":
        d8_bin = os.path.join(repo_root, "out", "x64.release", "d8")
        _write_historic_fixture(
            repo_root,
            wt_dir,
            "repro.sh",
            build_582176947_d8_sh(d8_bin),
            build_582176947_poc_js(),
        )
      elif cid == "historic_577269348":
        d8_bin = os.path.join(repo_root, "out", "x64.release", "d8")
        _write_historic_fixture(
            repo_root,
            wt_dir,
            "repro.sh",
            build_577269348_d8_sh(d8_bin),
            build_577269348_poc_js(),
        )
      elif cid == "historic_578950019":
        d8_bin = os.path.join(repo_root, "out", "x64.release", "d8")
        _write_historic_fixture(
            repo_root,
            wt_dir,
            "repro.sh",
            build_578950019_d8_sh(d8_bin),
            build_578950019_poc_js(),
        )
      elif cid == "historic_574021176":
        d8_bin = os.path.join(repo_root, "out", "x64.release", "d8")
        _write_historic_fixture(
            repo_root,
            wt_dir,
            "repro.sh",
            build_574021176_d8_sh(d8_bin),
            build_574021176_poc_js(),
        )

      entries[cid]["variants"][variant] = {
          "worktree_dir": wt_dir,
          "ref": ref,
      }

  manifest = {
      "run_id": run_id,
      "repo_root": repo_root,
      "work_dir": work_dir,
      "base_ref": base_ref,
      "target_ref": target_ref,
      "created_worktrees": created_worktrees,
      "temp_branches": temp_branches,
      "cases": entries,
  }
  manifest_path = os.path.join(work_dir, "manifest.json")
  with open(manifest_path, "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2)
  return manifest


def parse_transcript_metrics(transcript_path: str) -> Dict[str, Any]:
  """Parses a JSONL agent transcript for efficiency and tool telemetry."""
  planner_turns = 0
  tool_calls_total = 0
  subagent_spawns_total = 0
  heavy_subagent_spawns = 0
  polling_calls = 0
  command_lines: List[str] = []
  tools_used: List[str] = []
  first_ts: Optional[datetime.datetime] = None
  last_ts: Optional[datetime.datetime] = None

  with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue
      step = json.loads(line)
      ts_str = step.get("created_at")
      if ts_str:
        ts = datetime.datetime.fromisoformat(ts_str.replace("Z", "+00:00"))
        if first_ts is None:
          first_ts = ts
        last_ts = ts

      if step.get("type") == "PLANNER_RESPONSE":
        planner_turns += 1
        for tc in step.get("tool_calls") or []:
          tool_calls_total += 1
          name = tc.get("name") or tc.get("tool_name") or ""
          tools_used.append(name)
          args = tc.get("arguments") or tc.get("args") or {}
          if isinstance(args, str):
            try:
              args = json.loads(args)
            except ValueError:
              args = {"raw": args}
          if name == "invoke_subagent":
            subagent_spawns_total += 1
            model = (args.get("Model") or "inherit").lower()
            tname = (args.get("TypeName") or "").lower()
            is_lightweight = model in (
                "flash",
                "flash_lite",
                "haiku",
                "mini",
            ) or tname in ("research-google", "explore")
            if not is_lightweight:
              heavy_subagent_spawns += 1
          if name == "run_command":
            cmd = str(args.get("CommandLine") or "")
            command_lines.append(cmd)
            if re.search(r"\bls\b.*(/brain/|scratch)", cmd):
              polling_calls += 1

  wall_seconds = 0.0
  if first_ts and last_ts:
    wall_seconds = round((last_ts - first_ts).total_seconds(), 2)

  return {
      "planner_turns": planner_turns,
      "tool_calls_total": tool_calls_total,
      "subagent_spawns_total": subagent_spawns_total,
      "heavy_subagent_spawns": heavy_subagent_spawns,
      "polling_calls": polling_calls,
      "wall_seconds": wall_seconds,
      "command_lines": command_lines,
      "tools_used": tools_used,
  }


def grade_case(
    case_id: str,
    worktree_dir: str,
    metrics: Dict[str, Any],
    response_text: str,
) -> Dict[str, Any]:
  """Evaluates deterministic outcome checks separately from minimize_dd adoption."""
  del response_text
  checks: Dict[str, bool] = {}
  cmds_joined = "\n".join(metrics.get("command_lines", []))
  used_minimize_dd = "minimize_dd.py" in cmds_joined

  if case_id == "minimize_dd":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    d8_sh = os.path.join(worktree_dir, "tmp", "fake_d8.sh")
    reproduces = False
    line_count = 999
    if os.path.exists(poc_min) and os.path.exists(d8_sh):
      with open(poc_min, "r", encoding="utf-8") as f:
        non_empty = [ln for ln in f if ln.strip()]
        line_count = len(non_empty)
      proc = _run(
          ["bash", d8_sh, "--expose-gc", poc_min],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          proc.returncode == 134 and "target_invariant" in (proc.stdout or ""))
    checks["min_file_reproduces"] = reproduces
    checks["minimal_lines"] = line_count <= 5
  elif case_id == "semantic_minimize":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    d8_sh = os.path.join(worktree_dir, "tmp", "fake_d8_semantic.sh")
    reproduces = False
    line_count = 999
    semantically_clean = False
    if os.path.exists(poc_min) and os.path.exists(d8_sh):
      with open(poc_min, "r", encoding="utf-8") as f:
        src = f.read()
      non_empty = [ln for ln in src.splitlines() if ln.strip()]
      line_count = len(non_empty)
      code_only = _strip_js_comments(src)
      semantically_clean = line_count <= 4 and not re.search(
          r"(__FuzzerEnv|\btry\b|\bcatch\b|\bfor\s*\(|__v_|__unused)",
          code_only,
      )
      proc = _run(
          ["bash", d8_sh, "--expose-gc", poc_min],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          proc.returncode == 134 and "target_invariant" in (proc.stdout or ""))
    checks["min_file_reproduces"] = reproduces
    checks["smart_semantic_simplification"] = semantically_clean
  elif case_id == "historic_582176947":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    repro_sh = os.path.join(worktree_dir, "tmp", "repro.sh")
    reproduces = False
    clean_regression = False
    if os.path.exists(poc_min) and os.path.exists(repro_sh):
      with open(poc_min, "r", encoding="utf-8") as f:
        src = f.read()
      non_empty = [ln for ln in src.splitlines() if ln.strip()]
      code_only = _strip_js_comments(src)
      clean_regression = len(non_empty) <= 25 and not re.search(
          r"(Africa/Monrovia|536870888|Uint32Array|class\s+C20|__dead|__noise_|unused)",
          code_only,
      )
      proc = _run(
          ["bash", repro_sh, "--allow-natives-syntax", poc_min],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          proc.returncode == 134 and
          "IsDereferenceAllowed" in (proc.stdout or ""))
    checks["min_file_reproduces"] = reproduces
    checks["clean_regression_test"] = clean_regression
  elif case_id == "historic_577269348":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    repro_sh = os.path.join(worktree_dir, "tmp", "repro.sh")
    reproduces = False
    clean_regression = False
    if os.path.exists(poc_min) and os.path.exists(repro_sh):
      with open(poc_min, "r", encoding="utf-8") as f:
        src = f.read()
      non_empty = [ln for ln in src.splitlines() if ln.strip()]
      code_only = _strip_js_comments(src)
      clean_regression = len(non_empty) <= 16 and not re.search(
          r"(__dead|__noise_|__unused|\btry\b|\bcatch\b)",
          code_only,
      )
      proc = _run(
          [
              "bash",
              repro_sh,
              "--stress-background-compile",
              "--expose-gc",
              "--bytecode-old-time=0",
              poc_min,
          ],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          proc.returncode == 134 and "pos.line > 0" in (proc.stdout or ""))
    checks["min_file_reproduces"] = reproduces
    checks["clean_regression_test"] = clean_regression
  elif case_id == "historic_578950019":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    repro_sh = os.path.join(worktree_dir, "tmp", "repro.sh")
    reproduces = False
    clean_regression = False
    if os.path.exists(poc_min) and os.path.exists(repro_sh):
      res = _run(
          ["bash", repro_sh, "--allow-natives-syntax", "--expose-gc", poc_min],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          res.returncode == 134 and
          "stale filler pointer in MaterializedObjectStore" in (res.stdout or
                                                                ""))
      with open(poc_min, "r", encoding="utf-8") as f:
        src = f.read()
      line_count = len([l for l in src.splitlines() if l.strip()])
      code_only = _strip_js_comments(src)
      clean_regression = (
          line_count <= 35 and
          not re.search(r"(__dead|__noise_|\beval\s*\(|<= 150)", code_only))
    checks["min_file_reproduces"] = reproduces
    checks["clean_regression_test"] = clean_regression
  elif case_id == "historic_574021176":
    poc_min = os.path.join(worktree_dir, "tmp", "poc-min.js")
    repro_sh = os.path.join(worktree_dir, "tmp", "repro.sh")
    reproduces = False
    clean_regression = False
    if os.path.exists(poc_min) and os.path.exists(repro_sh):
      res = _run(
          ["bash", repro_sh, "--allow-natives-syntax", poc_min],
          cwd=worktree_dir,
          check=False,
      )
      reproduces = (
          res.returncode == 134 and
          "exposed ContextCell via leaked CLASS_SCOPE" in (res.stdout or ""))
      with open(poc_min, "r", encoding="utf-8") as f:
        src = f.read()
      line_count = len([l for l in src.splitlines() if l.strip()])
      code_only = _strip_js_comments(src)
      clean_regression = (
          line_count <= 32 and not re.search(
              r"(trainThrows|for\s*\([^)]*2000|\bheritage\b|typeof\s+leaked|__dead|__noise_)",
              code_only,
          ))
    checks["min_file_reproduces"] = reproduces
    checks["clean_regression_test"] = clean_regression
  else:
    raise ValueError(f"Unknown case_id: {case_id}")

  passed = all(checks.values())
  return {
      "passed": passed,
      "used_minimize_dd": used_minimize_dd,
      "checks": checks,
      "metrics": {
          k: v
          for k, v in metrics.items()
          if k not in ("command_lines", "tools_used")
      },
  }


def find_transcript_path(app_data_dir: str, conv_id: str) -> str:
  return os.path.join(
      app_data_dir,
      "brain",
      conv_id,
      ".system_generated",
      "logs",
      "transcript.jsonl",
  )


def extract_response_text(transcript_path: str) -> str:
  """Extracts agent response text, including subagent send_message calls."""
  parts: List[str] = []
  last_content = ""
  with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue
      step = json.loads(line)
      if step.get("type") != "PLANNER_RESPONSE":
        continue
      content = step.get("content") or ""
      if content:
        last_content = content
      for tc in step.get("tool_calls") or []:
        name = tc.get("name") or tc.get("tool_name") or ""
        if name != "send_message":
          continue
        args = tc.get("arguments") or tc.get("args") or {}
        if isinstance(args, str):
          try:
            args = json.loads(args)
          except ValueError:
            continue
        msg = args.get("Message") or args.get("message") or ""
        if msg:
          parts.append(str(msg))
  if last_content:
    parts.append(last_content)
  return "\n".join(parts)


def grade_from_runs(
    manifests: Sequence[Dict[str, Any]],
    runs: Dict[str, Dict[str, str]],
    app_data_dir: str,
) -> Dict[str, Any]:
  """Grades all runs formatted as {'case_id:variant': {'conv_id': ..., 'response': ...}}."""
  report: Dict[str, Any] = {}
  for manifest in manifests:
    for case_id, case_info in manifest["cases"].items():
      report.setdefault(case_id, {})
      for variant, vinfo in case_info["variants"].items():
        key = f"{case_id}:{variant}"
        run_info = runs.get(key)
        if not run_info:
          continue
        tpath = find_transcript_path(app_data_dir, run_info["conv_id"])
        metrics = parse_transcript_metrics(tpath)
        response_text = run_info.get("response", "")
        if not response_text:
          response_text = extract_response_text(tpath)
        else:
          sent = extract_response_text(tpath)
          if sent and sent not in response_text:
            response_text = f"{response_text}\n{sent}"
        report[case_id][variant] = grade_case(
            case_id=case_id,
            worktree_dir=vinfo["worktree_dir"],
            metrics=metrics,
            response_text=response_text,
        )
  return report


def cleanup_eval(manifest: Dict[str, Any]) -> None:
  """Removes all eval worktrees, temporary branches, and temp work_dir."""
  repo_root = manifest["repo_root"]
  for wt in reversed(manifest.get("created_worktrees", [])):
    if os.path.exists(wt):
      _run(
          ["git", "worktree", "remove", "--force", wt],
          cwd=repo_root,
          check=False,
      )
  _run(["git", "worktree", "prune"], cwd=repo_root, check=False)
  for branch in manifest.get("temp_branches", []):
    _run(["git", "branch", "-D", branch], cwd=repo_root, check=False)
  work_dir = manifest.get("work_dir")
  if work_dir and os.path.exists(work_dir):
    shutil.rmtree(work_dir, ignore_errors=True)


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  sub = parser.add_subparsers(dest="subcommand", required=True)

  p_setup = sub.add_parser("setup", help="Create isolated eval worktrees.")
  p_setup.add_argument("--repo-root", default=DEFAULT_REPO_ROOT)
  p_setup.add_argument("--base-ref", default="origin/main")
  p_setup.add_argument("--target-ref", default="HEAD")
  p_setup.add_argument("--work-dir", required=True)
  p_setup.add_argument("--apply-working-tree", action="store_true")
  p_setup.add_argument(
      "--variant",
      choices=("before", "after", "both"),
      default="both",
      help="Set up only before, only after, or both worktree sets.",
  )
  p_setup.add_argument(
      "--cases",
      nargs="+",
      default=None,
      help="Optional subset of case IDs to set up.",
  )

  p_grade = sub.add_parser("grade", help="Grade finished eval runs.")
  p_grade.add_argument("--manifest", nargs="+", required=True)
  p_grade.add_argument("--runs-json", required=True)
  p_grade.add_argument(
      "--app-data-dir",
      default=os.environ.get("AGENT_APP_DATA_DIR",
                             os.path.expanduser("~/.gemini/jetski")),
  )

  p_clean = sub.add_parser("cleanup", help="Remove eval worktrees & branches.")
  p_clean.add_argument("--manifest", required=True)
  return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = parse_args(argv)
  if args.subcommand == "setup":
    variants = (("before", "after") if args.variant == "both" else
                (args.variant,))
    manifest = setup_eval(
        repo_root=os.path.abspath(args.repo_root),
        base_ref=args.base_ref,
        target_ref=args.target_ref,
        work_dir=os.path.abspath(args.work_dir),
        apply_working_tree=args.apply_working_tree,
        variants=variants,
        case_filter=args.cases,
    )
    print(json.dumps(manifest, indent=2))
    return 0
  if args.subcommand == "grade":
    manifests = []
    for mpath in args.manifest:
      with open(mpath, "r", encoding="utf-8") as f:
        manifests.append(json.load(f))
    with open(args.runs_json, "r", encoding="utf-8") as f:
      runs = json.load(f)
    report = grade_from_runs(
        manifests=manifests,
        runs=runs,
        app_data_dir=args.app_data_dir,
    )
    print(json.dumps(report, indent=2))
    return 0
  if args.subcommand == "cleanup":
    with open(args.manifest, "r", encoding="utf-8") as f:
      manifest = json.load(f)
    cleanup_eval(manifest)
    print("Cleaned up eval worktrees.")
    return 0
  return 1


if __name__ == "__main__":
  sys.exit(main())
