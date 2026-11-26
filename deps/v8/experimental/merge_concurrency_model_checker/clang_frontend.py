# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Clang C++20 AST frontend, validated PCH cache, and fail-closed AST converter."""

from dataclasses import dataclass, field
import hashlib
import json
import os
import re
import stat
import subprocess
import tempfile
from typing import Any, Dict, FrozenSet, List, Optional, Tuple

from errors import UnsupportedCxxAstError

_SCALAR_DESUGARED_TYPES: FrozenSet[str] = frozenset({
    "bool",
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "unsigned",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "float",
    "double",
})


def _node_qual_type(node: Dict[str, Any], default: str = "") -> str:
  """Returns Clang type string, desugaring typedef'd C++ scalar integer types."""
  t = node.get("type")
  if not isinstance(t, dict):
    return default
  dq = t.get("desugaredQualType", "")
  qt = t.get("qualType", "")
  if dq:
    stripped = dq.strip()
    for prefix in ("const ", "volatile ", "constexpr "):
      while stripped.startswith(prefix):
        stripped = stripped[len(prefix):].strip()
    for suffix in (" const", " volatile", "&"):
      while stripped.endswith(suffix):
        stripped = stripped[:-len(suffix)].strip()
    if stripped in _SCALAR_DESUGARED_TYPES:
      return dq
  return qt or dq or default


V8_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def _find_clang_cxx() -> str:
  bundled = os.path.join(V8_ROOT, "third_party", "llvm-build",
                         "Release+Asserts", "bin", "clang++")
  if os.path.isfile(bundled):
    return bundled
  return "clang++"


CLANG_CXX_BIN = _find_clang_cxx()

_CLANG_COMMON_FLAGS: Tuple[str, ...] = (
    "-std=c++20",
    "-Wno-invalid-offsetof",
    "-DDEBUG",
    "-I.",
    "-Iinclude",
    "-Ithird_party/abseil-cpp",
    "-Ithird_party/simdutf",
    "-DV8_COMPRESS_POINTERS",
    "-DV8_31BIT_SMIS_ON_64BIT_ARCH",
    "-DV8_TARGET_ARCH_X64",
    "-DV8_ENABLE_SANDBOX",
    "-DV8_EXTERNAL_CODE_SPACE",
)

# Real V8 headers are included via V8_METAGEN_GENERATION_PASS so Torque-generated
# files are not required, while still type-checking against real V8 declarations.
CLANG_TU_STUB_HEADER = r"""
#define V8_METAGEN_GENERATION_PASS
#include "src/objects/instance-type.h"
namespace v8::internal {
constexpr InstanceType ATOM_REG_EXP_DATA_TYPE = static_cast<InstanceType>(1);
constexpr InstanceType BYTECODE_ARRAY_TYPE = static_cast<InstanceType>(2);
constexpr InstanceType CODE_TYPE = static_cast<InstanceType>(3);
constexpr InstanceType DEBUG_INFO_TYPE = static_cast<InstanceType>(4);
constexpr InstanceType INTERPRETER_DATA_TYPE = static_cast<InstanceType>(5);
constexpr InstanceType IR_REG_EXP_DATA_TYPE = static_cast<InstanceType>(6);
constexpr InstanceType UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE = static_cast<InstanceType>(7);
constexpr InstanceType UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE = static_cast<InstanceType>(8);
constexpr InstanceType UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE = static_cast<InstanceType>(9);
constexpr InstanceType UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE = static_cast<InstanceType>(10);
constexpr InstanceType WASM_CAPI_FUNCTION_DATA_TYPE = static_cast<InstanceType>(11);
constexpr InstanceType WASM_DISPATCH_TABLE_TYPE = static_cast<InstanceType>(12);
constexpr InstanceType WASM_EXPORTED_FUNCTION_DATA_TYPE = static_cast<InstanceType>(13);
constexpr InstanceType WASM_FUNCTION_DATA_TYPE = static_cast<InstanceType>(14);
constexpr InstanceType WASM_INTERNAL_FUNCTION_TYPE = static_cast<InstanceType>(15);
constexpr InstanceType WASM_SUSPENDER_OBJECT_TYPE = static_cast<InstanceType>(16);
constexpr InstanceType WASM_TRUSTED_INSTANCE_DATA_TYPE = static_cast<InstanceType>(17);
class CodeWrapper;
class AccessorPair;
class BytecodeWrapper;
}  // namespace v8::internal

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/small-vector.h"
#include "src/common/globals.h"
#include "src/common/assert-scope.h"
#include "src/flags/flags.h"
#include "src/objects/tagged.h"
#include "src/objects/smi.h"
#include "src/objects/casting.h"
#include "src/objects/objects.h"
#include "src/objects/heap-object.h"
#include "src/objects/trusted-object.h"
#include "src/objects/slots.h"
#include "src/objects/compressed-slots.h"
#include "src/roots/roots.h"
#undef BUILTINS_WITH_SFI_LIST_GENERATOR
#define BUILTINS_WITH_SFI_LIST_GENERATOR(ADAPTER, V)
#include "src/handles/handles.h"
#include "src/handles/handles-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/handles/local-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate.h"
#include "src/heap/local-heap.h"
#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/objects/oddball.h"
#include "src/objects/hole.h"
#include "src/objects/fixed-array.h"
#include "src/objects/scope-info.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/literal-objects.h"
#include "src/objects/debug-objects.h"
#include "src/codegen/compiler.h"
#include "src/codegen/background-merge-task.h"

#undef DCHECK
#undef DCHECK_EQ
#undef DCHECK_NE
#undef DCHECK_GT
#undef DCHECK_GE
#undef DCHECK_LT
#undef DCHECK_LE
#undef DCHECK_NOT_NULL
#undef DCHECK_IMPLIES
#undef CHECK
#undef CHECK_EQ
#undef CHECK_NE
#undef SLOW_DCHECK
#undef SBXCHECK
#undef UNREACHABLE
#undef SYNCHRONIZATION_POINT
#undef SYNCHRONIZATION_POINT_TEST_ONLY
#undef SYNCHRONIZATION_POINT_WITH_SAFEPOINT
#undef SYNCHRONIZATION_POINT_WITH_SAFEPOINT_TEST_ONLY
#undef MERGE_MODEL_STEP
#undef MERGE_MODEL_STEP_WITH_SAFEPOINT
#undef DEF_GETTER
#undef DEF_ACQUIRE_GETTER

namespace v8::internal {

template <typename... Args> void DCHECK(Args&&...) {}
template <typename... Args> void DCHECK_EQ(Args&&...) {}
template <typename... Args> void DCHECK_NE(Args&&...) {}
template <typename... Args> void DCHECK_GT(Args&&...) {}
template <typename... Args> void DCHECK_GE(Args&&...) {}
template <typename... Args> void DCHECK_LT(Args&&...) {}
template <typename... Args> void DCHECK_LE(Args&&...) {}
template <typename... Args> void DCHECK_NOT_NULL(Args&&...) {}
#define DCHECK_IMPLIES(v1, v2) DCHECK(!(v1) || (v2))
template <typename... Args> void CHECK(Args&&...) {}
template <typename... Args> void CHECK_EQ(Args&&...) {}
template <typename... Args> void CHECK_NE(Args&&...) {}
template <typename... Args> void SLOW_DCHECK(Args&&...) {}
template <typename... Args> void SBXCHECK(Args&&...) {}
inline void UNREACHABLE() {}
template <typename T> void Print(T) {}
inline void PrintF(const char*, ...) {}

inline void __SYNCHRONIZATION_POINT(const char*, bool) {}
#define SYNCHRONIZATION_POINT(name) __SYNCHRONIZATION_POINT(name, false)
#define SYNCHRONIZATION_POINT_TEST_ONLY(name) __SYNCHRONIZATION_POINT(name, false)
#define SYNCHRONIZATION_POINT_WITH_SAFEPOINT(name) __SYNCHRONIZATION_POINT(name, true)
#define SYNCHRONIZATION_POINT_WITH_SAFEPOINT_TEST_ONLY(name) __SYNCHRONIZATION_POINT(name, true)

inline void __MERGE_MODEL_STEP(const char*) {}
inline void __MERGE_MODEL_STEP_WITH_SAFEPOINT(const char*) {}
#define MERGE_MODEL_STEP(step) __MERGE_MODEL_STEP(#step)
#define MERGE_MODEL_STEP_WITH_SAFEPOINT(step) __MERGE_MODEL_STEP_WITH_SAFEPOINT(#step)

#define DEF_GETTER(cls, name, ...) __VA_ARGS__ cls::name() const
#define DEF_ACQUIRE_GETTER(cls, name, ...) __VA_ARGS__ cls::name(AcquireLoadTag tag) const

template <typename T>
struct CastTraits {
  static inline bool AllowFrom(Tagged<Object>) { return true; }
  static inline bool AllowFrom(Tagged<HeapObject>) { return true; }
};

ReadOnlyRoots GetReadOnlyRoots();
IsolateForSandbox GetCurrentIsolateForSandbox();
void VerifyCodeMerge(Isolate*, DirectHandle<Script>);

}  // namespace v8::internal
"""


def find_matching_brace(s: str, open_idx: int) -> int:
  """Returns the index of the '}' matching '{' at ``open_idx``, skipping strings/comments."""
  depth = 0
  in_str = False
  in_char = False
  in_line = False
  in_block = False
  i = open_idx
  n = len(s)
  while i < n:
    c = s[i]
    if in_line:
      if c == "\n":
        in_line = False
    elif in_block:
      if c == "*" and i + 1 < n and s[i + 1] == "/":
        in_block = False
        i += 1
    elif in_str:
      if c == "\\" and i + 1 < n:
        i += 1
      elif c == '"':
        in_str = False
    elif in_char:
      if c == "\\" and i + 1 < n:
        i += 1
      elif c == "'":
        in_char = False
    else:
      if c == "/" and i + 1 < n and s[i + 1] == "/":
        in_line = True
        i += 1
      elif c == "/" and i + 1 < n and s[i + 1] == "*":
        in_block = True
        i += 1
      elif c == '"':
        in_str = True
      elif c == "'":
        in_char = True
      elif c == "{":
        depth += 1
      elif c == "}":
        depth -= 1
        if depth == 0:
          return i
    i += 1
  return -1


def read_repo_file(
    rel_path: str,
    *,
    git_rev: Optional[str] = None,
    override_path: Optional[str] = None,
    content_override: Optional[str] = None,
) -> str:
  """Reads a V8 repository file from override content, explicit path, git rev, or V8_ROOT."""
  if content_override is not None:
    return content_override
  if git_rev is not None:
    try:
      return subprocess.check_output(
          [
              "git", "-c", "core.fsmonitor=false", "show",
              f"{git_rev}:{rel_path}"
          ],
          cwd=V8_ROOT,
          text=True,
          encoding="utf-8",
          stderr=subprocess.PIPE,
      )
    except subprocess.CalledProcessError as e:
      raise FileNotFoundError(
          f"Failed to fetch {git_rev}:{rel_path} from git: {e.stderr.strip()}"
      ) from e
  path = override_path or os.path.join(V8_ROOT, rel_path)
  with open(path, "r", encoding="utf-8") as f:
    return f.read()


_CLANG_VERSION_CACHE: Optional[str] = None


def _get_clang_version() -> str:
  global _CLANG_VERSION_CACHE
  if _CLANG_VERSION_CACHE is None:
    proc = subprocess.run(
        [CLANG_CXX_BIN, "--version"],
        capture_output=True,
        text=True,
        check=True,
    )
    _CLANG_VERSION_CACHE = proc.stdout.strip()
  return _CLANG_VERSION_CACHE


def _get_private_pch_dir() -> str:
  """Creates/validates a private 0700 per-user PCH cache directory."""
  base = os.path.join(tempfile.gettempdir(), f"v8_mcmc_pch_{os.getuid()}")
  try:
    os.makedirs(base, mode=0o700, exist_ok=True)
  except OSError:
    return tempfile.mkdtemp(prefix="v8_mcmc_pch_priv_")
  st = os.lstat(base)
  if (not stat.S_ISDIR(st.st_mode) or stat.S_ISLNK(st.st_mode) or
      st.st_uid != os.getuid() or (st.st_mode & 0o077) != 0):
    raise RuntimeError(
        f"Unsafe PCH cache directory {base!r} (mode={oct(st.st_mode)}, uid={st.st_uid})"
    )
  return base


def _parse_depfile(dep_text: str) -> List[str]:
  """Parses a Make-format ``-MD`` depfile into a list of dependency paths."""
  # Join backslash-escaped line continuations.
  joined = re.sub(r"\\\r?\n", " ", dep_text)
  if ":" not in joined:
    return []
  _, rhs = joined.split(":", 1)
  deps: List[str] = []
  cur: List[str] = []
  i = 0
  n = len(rhs)
  while i < n:
    c = rhs[i]
    if c == "\\" and i + 1 < n and rhs[i + 1] == " ":
      cur.append(" ")
      i += 2
    elif c.isspace():
      if cur:
        deps.append("".join(cur))
        cur = []
      i += 1
    else:
      cur.append(c)
      i += 1
  if cur:
    deps.append("".join(cur))
  return deps


def _stat_sig(path: str) -> Optional[List[int]]:
  try:
    st = os.stat(path)
    return [st.st_mtime_ns, st.st_size]
  except OSError:
    return None


def _pch_meta_valid(meta_path: str, pch_path: str) -> bool:
  if not os.path.isfile(pch_path) or not os.path.isfile(meta_path):
    return False
  try:
    with open(meta_path, "r", encoding="utf-8") as f:
      meta = json.load(f)
  except (OSError, json.JSONDecodeError):
    return False
  deps = meta.get("deps")
  if not isinstance(deps, dict) or not deps:
    return False
  for dep_path, expected_sig in deps.items():
    if _stat_sig(dep_path) != expected_sig:
      return False
  return True


def _atomic_write_text(path: str, content: str, directory: str) -> None:
  fd, tmp_path = tempfile.mkstemp(dir=directory, prefix=".tmp_mcmc_")
  try:
    os.fchmod(fd, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
      f.write(content)
    os.replace(tmp_path, path)
  except BaseException:
    try:
      os.unlink(tmp_path)
    except OSError:
      pass
    raise


def _get_clang_pch_path(
    override_headers: Optional[Dict[str, str]] = None,
) -> Tuple[str, Optional[str]]:
  """Builds and validates a precompiled header (.pch) for CLANG_TU_STUB_HEADER.

  Returns ``(pch_path, override_include_dir_or_None)``. When ``override_headers``
  is provided (e.g. for ``--git-rev``), those headers are materialized into a
  stable private directory prepended via ``-I`` ahead of ``-I.``.
  """
  pch_dir = _get_private_pch_dir()
  clang_ver = _get_clang_version()

  override_items = sorted((override_headers or {}).items())
  hasher = hashlib.sha256()
  hasher.update(CLANG_CXX_BIN.encode("utf-8"))
  hasher.update(b"\0")
  hasher.update(clang_ver.encode("utf-8"))
  hasher.update(b"\0")
  hasher.update(V8_ROOT.encode("utf-8"))
  hasher.update(b"\0")
  for fl in _CLANG_COMMON_FLAGS:
    hasher.update(fl.encode("utf-8"))
    hasher.update(b"\0")
  hasher.update(CLANG_TU_STUB_HEADER.encode("utf-8"))
  hasher.update(b"\0")
  for rel_path, text in override_items:
    hasher.update(rel_path.encode("utf-8"))
    hasher.update(b"\0")
    hasher.update(text.encode("utf-8"))
    hasher.update(b"\0")
  digest = hasher.hexdigest()[:24]

  override_dir: Optional[str] = None
  if override_items:
    override_dir = os.path.join(pch_dir, f"inc_{digest}")
    for rel_path, text in override_items:
      dst = os.path.join(override_dir, rel_path)
      os.makedirs(os.path.dirname(dst), mode=0o700, exist_ok=True)
      if not os.path.isfile(dst):
        _atomic_write_text(dst, text, os.path.dirname(dst))

  hdr_path = os.path.join(pch_dir, f"hdr_{digest}.h")
  pch_path = os.path.join(pch_dir, f"hdr_{digest}.pch")
  meta_path = os.path.join(pch_dir, f"hdr_{digest}.meta.json")

  if _pch_meta_valid(meta_path, pch_path):
    return pch_path, override_dir

  if not os.path.isfile(hdr_path):
    _atomic_write_text(hdr_path, CLANG_TU_STUB_HEADER, pch_dir)

  fd_pch, tmp_pch = tempfile.mkstemp(
      dir=pch_dir, prefix=".tmp_pch_", suffix=".pch")
  os.close(fd_pch)
  fd_dep, tmp_dep = tempfile.mkstemp(
      dir=pch_dir, prefix=".tmp_dep_", suffix=".d")
  os.close(fd_dep)
  try:
    cmd = [CLANG_CXX_BIN, "-x", "c++-header"]
    if override_dir:
      cmd.append(f"-I{override_dir}")
    cmd.extend(_CLANG_COMMON_FLAGS)
    cmd.extend(["-MD", "-MF", tmp_dep, hdr_path, "-o", tmp_pch])
    proc = subprocess.run(
        cmd,
        cwd=V8_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
      raise RuntimeError(
          f"Clang PCH build failed (exit {proc.returncode}):\n{proc.stderr}")

    with open(tmp_dep, "r", encoding="utf-8") as f:
      dep_paths = _parse_depfile(f.read())
    dep_sigs: Dict[str, List[int]] = {}
    for dp in dep_paths:
      abs_dp = dp if os.path.isabs(dp) else os.path.normpath(
          os.path.join(V8_ROOT, dp))
      sig = _stat_sig(abs_dp)
      if sig is not None:
        dep_sigs[abs_dp] = sig
    os.chmod(tmp_pch, 0o600)
    os.replace(tmp_pch, pch_path)
    _atomic_write_text(
        meta_path,
        json.dumps({
            "clang_version": clang_ver,
            "deps": dep_sigs
        }),
        pch_dir,
    )
  finally:
    for p in (tmp_pch, tmp_dep):
      if os.path.exists(p):
        try:
          os.unlink(p)
        except OSError:
          pass

  return pch_path, override_dir


def _extract_balanced_template_args(text: str, open_lt: int) -> Optional[str]:
  """Extracts the substring inside balanced ``<...>`` starting at ``open_lt``."""
  if open_lt < 0 or open_lt >= len(text) or text[open_lt] != "<":
    return None
  depth = 0
  i = open_lt
  n = len(text)
  while i < n:
    c = text[i]
    if c == "<":
      depth += 1
    elif c == ">":
      depth -= 1
      if depth == 0:
        return text[open_lt + 1:i].strip()
    i += 1
  return None


def _strip_leading_ns(type_name: str) -> str:
  s = type_name.strip()
  for prefix in ("v8::internal::", "i::"):
    if s.startswith(prefix):
      s = s[len(prefix):]
  return s


def _split_top_level_args(args_str: str) -> List[str]:
  parts: List[str] = []
  cur: List[str] = []
  depth = 0
  for c in args_str:
    if c in "<([":
      depth += 1
      cur.append(c)
    elif c in ">)]":
      depth -= 1
      cur.append(c)
    elif c == "," and depth == 0:
      parts.append("".join(cur).strip())
      cur = []
    else:
      cur.append(c)
  if cur:
    parts.append("".join(cur).strip())
  return parts


class ClangAstConverter:
  """Fail-closed converter from Clang JSON AST into structured interpreter IR."""

  _TRANSPARENT_WRAPPERS = frozenset({
      "ExprWithCleanups",
      "MaterializeTemporaryExpr",
      "CXXBindTemporaryExpr",
      "ParenExpr",
      "SubstNonTypeTemplateParmExpr",
      "CXXRewrittenBinaryOperator",
  })

  _BINARY_OP_MAP = {
      "+": "+",
      "-": "-",
      "*": "*",
      "/": "/",
      "%": "%",
      "<<": "<<",
      ">>": ">>",
      "&": "&",
      "|": "|",
      "^": "^",
      "==": "==",
      "!=": "!=",
      "<": "<",
      "<=": "<=",
      ">": ">",
      ">=": ">=",
      "&&": "&&",
      "||": "||",
      ",": ",",
  }

  _COMPOUND_OPS = frozenset({
      "+=",
      "-=",
      "*=",
      "/=",
      "%=",
      "&=",
      "|=",
      "^=",
      "<<=",
      ">>=",
  })

  _file_cache: Dict[str, str] = {}

  def __init__(self, tu_source: str):
    self.tu_source = tu_source

  def _source_slice_for_node(self, node: Dict[str, Any]) -> Optional[str]:
    """Returns exact source text for ``node`` ONLY if file identity is verified."""
    rng = node.get("range")
    if not isinstance(rng, dict):
      return None
    begin = rng.get("begin", {})
    end = rng.get("end", {})
    if not isinstance(begin, dict) or not isinstance(end, dict):
      return None

    # Prefer spellingLoc when spelled in <stdin> (e.g., macro arguments inside
    # DCHECK_IMPLIES(...)). Fall back to expansionLoc when spellingLoc lives in
    # a macro definition inside an external header (e.g., offsetof).
    def _pick_loc(
        loc: Dict[str, Any]) -> Tuple[Optional[str], Optional[int], int]:
      sl = loc.get("spellingLoc")
      el = loc.get("expansionLoc")
      if isinstance(sl, dict) and sl.get(
          "file", "<stdin>") == "<stdin>" and "offset" in sl:
        return "<stdin>", sl["offset"], sl.get("tokLen", 0)
      if isinstance(el, dict) and el.get(
          "file", "<stdin>") == "<stdin>" and "offset" in el:
        return "<stdin>", el["offset"], el.get("tokLen", 0)
      if isinstance(sl, dict) and "offset" in sl:
        return sl.get("file"), sl["offset"], sl.get("tokLen", 0)
      return loc.get("file"), loc.get("offset"), loc.get("tokLen", 0)

    b_file, b_off, _ = _pick_loc(begin)
    e_file, e_off, e_len = _pick_loc(end)
    if b_off is None or e_off is None:
      return None
    if b_file != e_file and not (b_file is None or e_file is None):
      return None
    target_file = b_file or e_file or "<stdin>"
    if target_file == "<stdin>":
      buf = self.tu_source
    else:
      abs_path = (
          target_file if os.path.isabs(target_file) else os.path.join(
              V8_ROOT, target_file))
      if abs_path not in self._file_cache:
        if not os.path.isfile(abs_path):
          return None
        with open(abs_path, "r", encoding="utf-8") as f:
          self._file_cache[abs_path] = f.read()
      buf = self._file_cache[abs_path]
    end_idx = e_off + max(e_len, 1)
    if 0 <= b_off <= end_idx <= len(buf):
      return buf[b_off:end_idx]
    return None

  def _extract_template_type_arg(
      self,
      call_node: Dict[str, Any],
      callee_node: Dict[str, Any],
      fn_name: str,
  ) -> Optional[str]:
    # 1. Try verified source slice around callee or call node.
    for candidate in (callee_node, call_node):
      src = self._source_slice_for_node(candidate)
      if src:
        idx = src.find(fn_name)
        if idx != -1:
          lt = src.find("<", idx + len(fn_name))
          if lt != -1:
            inner = _extract_balanced_template_args(src, lt)
            if inner:
              args = _split_top_level_args(inner)
              if args:
                return _strip_leading_ns(args[0])
    # 2. Fallback to return type for Cast/TrustedCast/SbxCast.
    if fn_name in ("Cast", "TrustedCast", "SbxCast"):
      qt = call_node.get("type", {}).get("qualType", "")
      lt = qt.find("<")
      if lt != -1:
        inner = _extract_balanced_template_args(qt, lt)
        if inner:
          return _strip_leading_ns(inner)
      if qt:
        return _strip_leading_ns(qt)
    return None

  def unwrap(self, node: Any) -> Any:
    while isinstance(node, dict):
      kind = node.get("kind")
      if kind in self._TRANSPARENT_WRAPPERS:
        inner = node.get("inner", [])
        if not inner:
          break
        node = inner[0]
      elif kind == "ImplicitCastExpr":
        ck = node.get("castKind", "")
        inner = node.get("inner", [])
        if not inner:
          break
        # Preserve implicit integral conversions (IntegralCast) so signedness
        # and width changes (e.g. `int(-1) < unsigned(1)` -> `0`) execute
        # faithfully! Pointer/lvalue/noop/derived/constructor casts unwrap.
        if ck == "IntegralCast":
          break
        node = inner[0]
      elif kind == "ConstantExpr":
        # Keep ConstantExpr if it has a pre-evaluated value; otherwise unwrap.
        if "value" in node:
          break
        inner = node.get("inner", [])
        if not inner:
          break
        node = inner[0]
      elif (kind == "CXXMemberCallExpr" and node.get("inner") and
            node["inner"][0].get("kind") == "MemberExpr" and
            node["inner"][0].get("name", "").startswith("operator ") and
            len(node.get("inner", [])) == 1 and node["inner"][0].get("inner")):
        op_name = node["inner"][0].get("name", "")
        if op_name == "operator bool":
          break
        # Transparent conversion operators (e.g. DirectHandle -> Tagged, Tagged<Sub> -> Tagged<Base>).
        node = node["inner"][0]["inner"][0]
      else:
        break
    return node

  def convert_compound_stmt(self, node: Dict[str, Any]) -> Dict[str, Any]:
    stmts: List[Dict[str, Any]] = []
    for child in node.get("inner", []):
      conv = self.convert_stmt(child)
      if conv is not None:
        if isinstance(conv, list):
          stmts.extend(conv)
        else:
          stmts.append(conv)
    return {"type": "Block", "body": stmts}

  def _as_block(self,
                raw_node: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    if raw_node is None:
      return None
    if raw_node.get("kind") == "CompoundStmt":
      return self.convert_compound_stmt(raw_node)
    conv = self.convert_stmt(raw_node)
    if conv is None:
      return {"type": "Block", "body": []}
    if isinstance(conv, list):
      return {"type": "Block", "body": conv}
    if conv.get("type") == "Block":
      return conv
    return {"type": "Block", "body": [conv]}

  def convert_stmt(self, node: Any) -> Any:
    if not isinstance(node, dict):
      return None
    kind = node.get("kind")
    if kind is None:
      return None
    if kind == "NullStmt":
      return None
    if kind == "StaticAssertDecl":
      return None
    if kind == "CompoundStmt":
      return self.convert_compound_stmt(node)

    if kind == "DeclStmt":
      decls = []
      for child in node.get("inner", []):
        ck = child.get("kind")
        if ck == "VarDecl":
          d = self.convert_var_decl(child)
          if d is not None:
            decls.append(d)
        elif ck in ("StaticAssertDecl", "TypedefDecl", "TypeAliasDecl",
                    "UsingDecl", "UsingDirectiveDecl"):
          continue
        else:
          raise UnsupportedCxxAstError(
              f"Unsupported DeclStmt child kind: {ck!r} (name={child.get('name')!r})"
          )
      if not decls:
        return None
      return decls[0] if len(decls) == 1 else decls

    if kind == "IfStmt":
      inner = list(node.get("inner", []))
      init_stmt = None
      cond_var = None
      if node.get("hasInit"):
        init_stmt = self.convert_stmt(inner.pop(0))
      if node.get("hasVar"):
        cond_var = self.convert_stmt(inner.pop(0))
      cond_expr = self.convert_expr(inner.pop(0))
      then_block = self._as_block(inner.pop(0))
      else_block = None
      if inner:
        else_raw = inner.pop(0)
        if else_raw.get("kind") == "IfStmt":
          else_block = self.convert_stmt(else_raw)
        else:
          else_block = self._as_block(else_raw)
      return {
          "type": "If",
          "init": init_stmt,
          "cond_var": cond_var,
          "cond": cond_expr,
          "body": then_block,
          "orelse": else_block,
      }

    if kind == "ForStmt":
      # Clang ForStmt always has 5 children: [init, condVar, cond, inc, body]
      # where omitted parts are empty dicts {} (no "kind").
      inner = node.get("inner", [])
      if len(inner) != 5:
        raise UnsupportedCxxAstError(
            f"Unexpected ForStmt children count: {len(inner)}")
      init_raw, cond_var_raw, cond_raw, inc_raw, body_raw = inner
      init_stmt = self.convert_stmt(init_raw) if init_raw.get("kind") else None
      cond_var = (
          self.convert_stmt(cond_var_raw) if cond_var_raw.get("kind") else None)
      cond_expr = self.convert_expr(cond_raw) if cond_raw.get("kind") else None
      step_expr = self.convert_expr(inc_raw) if inc_raw.get("kind") else None
      body_block = self._as_block(body_raw)
      return {
          "type": "For",
          "init": init_stmt,
          "cond_var": cond_var,
          "cond": cond_expr,
          "step": step_expr,
          "body": body_block,
      }

    if kind == "CXXForRangeStmt":
      inner = node.get("inner", [])
      range_expr = None
      loop_var = None
      loop_var_type = ""
      loop_var_is_ref = False
      body_node = None
      for child in inner:
        ck = child.get("kind")
        if ck == "DeclStmt" and child.get("inner"):
          vd = child["inner"][0]
          vname = vd.get("name", "")
          if vname.startswith("__range"):
            if vd.get("inner"):
              range_expr = self.convert_expr(vd["inner"][0])
          elif not vname.startswith("__"):
            loop_var = vname
            loop_var_type = vd.get("type", {}).get("qualType", "")
            loop_var_is_ref = "&" in loop_var_type and "&&" not in loop_var_type
        elif ck in ("CompoundStmt", "IfStmt", "ForStmt", "WhileStmt", "DoStmt",
                    "SwitchStmt", "ExprWithCleanups", "CallExpr",
                    "CXXMemberCallExpr", "BinaryOperator",
                    "CompoundAssignOperator", "UnaryOperator", "ReturnStmt"):
          body_node = child
      if loop_var is None or range_expr is None:
        raise UnsupportedCxxAstError(
            f"Failed to lower CXXForRangeStmt: var={loop_var!r}, range={range_expr!r}"
        )
      return {
          "type": "RangeFor",
          "var": loop_var,
          "var_type": loop_var_type,
          "is_ref": loop_var_is_ref,
          "iterable": range_expr,
          "body": self._as_block(body_node),
      }

    if kind == "WhileStmt":
      inner = list(node.get("inner", []))
      cond_var = None
      if node.get("hasVar"):
        cond_var = self.convert_stmt(inner.pop(0))
      cond_expr = self.convert_expr(inner.pop(0))
      body_block = self._as_block(inner.pop(0))
      return {
          "type": "While",
          "cond_var": cond_var,
          "cond": cond_expr,
          "body": body_block,
      }

    if kind == "DoStmt":
      inner = node.get("inner", [])
      if len(inner) != 2:
        raise UnsupportedCxxAstError(
            f"Unexpected DoStmt children: {len(inner)}")
      return {
          "type": "DoWhile",
          "body": self._as_block(inner[0]),
          "cond": self.convert_expr(inner[1]),
      }

    if kind == "SwitchStmt":
      inner = list(node.get("inner", []))
      init_stmt = None
      cond_var = None
      if node.get("hasInit"):
        init_stmt = self.convert_stmt(inner.pop(0))
      if node.get("hasVar"):
        cond_var = self.convert_stmt(inner.pop(0))
      cond_expr = self.convert_expr(inner.pop(0))
      body_raw = inner.pop(0)
      # Flatten CaseStmt / DefaultStmt entries in order so fallthrough works.
      items: List[Dict[str, Any]] = []

      def _collect_switch_items(raw_list: List[Dict[str, Any]]) -> None:
        for s in raw_list:
          sk = s.get("kind")
          if sk == "CaseStmt":
            c_inner = s.get("inner", [])
            case_val = self.convert_expr(c_inner[0])
            items.append({"type": "CaseLabel", "value": case_val})
            if len(c_inner) > 1:
              _collect_switch_items([c_inner[-1]])
          elif sk == "DefaultStmt":
            items.append({"type": "DefaultLabel"})
            d_inner = s.get("inner", [])
            if d_inner:
              _collect_switch_items([d_inner[-1]])
          else:
            conv = self.convert_stmt(s)
            if conv is not None:
              if isinstance(conv, list):
                items.extend(conv)
              else:
                items.append(conv)

      if body_raw.get("kind") == "CompoundStmt":
        _collect_switch_items(body_raw.get("inner", []))
      else:
        _collect_switch_items([body_raw])
      return {
          "type": "Switch",
          "init": init_stmt,
          "cond_var": cond_var,
          "cond": cond_expr,
          "items": items,
      }

    if kind == "BreakStmt":
      return {"type": "Break"}
    if kind == "ContinueStmt":
      return {"type": "Continue"}
    if kind == "ReturnStmt":
      inner = node.get("inner", [])
      val = self.convert_expr(inner[0]) if inner else None
      return {"type": "Return", "value": val}

    # Expression statement:
    expr = self.convert_expr(node)
    if expr is None:
      return None
    if expr.get("type") in ("MergeModelStep", "SyncPoint"):
      return expr
    return {"type": "ExprStmt", "expr": expr}

  def convert_var_decl(self, node: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    name = node.get("name", "")
    qual_type = _node_qual_type(node)
    is_ref = "&" in qual_type and "&&" not in qual_type
    inner = node.get("inner", [])

    if not inner:
      return {
          "type": "Decl",
          "name": name,
          "qual_type": qual_type,
          "is_ref": is_ref,
          "value": None,
      }

    init_node = self.unwrap(inner[0])
    init_kind = init_node.get("kind")
    if init_kind == "CXXConstructExpr":
      args = [
          self.convert_expr(a)
          for a in init_node.get("inner", [])
          if a.get("kind") != "CXXDefaultArgExpr"
      ]
      ctor_type = _strip_leading_ns(
          init_node.get("type", {}).get("qualType", qual_type))
      # Copy/move/converting constructor of a single value (e.g. `std::vector<int> b = a;`
      # or `Handle<T> h = ...` or `Tagged<T> x = ...` or `auto it = ...`).
      if len(args) == 1 and not self._is_raii_scope_or_forwarder(ctor_type):
        val = {
            "type": "Construct",
            "ctor_type": ctor_type,
            "args": args,
            "is_copy_or_conv": True,
        }
      else:
        val = {
            "type": "Construct",
            "ctor_type": ctor_type,
            "args": args,
            "is_copy_or_conv": False,
        }
    else:
      val = self.convert_expr(inner[0])

    return {
        "type": "Decl",
        "name": name,
        "qual_type": qual_type,
        "is_ref": is_ref,
        "value": val,
    }

  @staticmethod
  def _is_raii_scope_or_forwarder(ctor_type: str) -> bool:
    base = ctor_type.split("<")[0].strip()
    return base in (
        "DisallowGarbageCollection",
        "DisallowHeapAllocation",
        "DisallowSafepoints",
        "HandleScope",
        "LocalHandleScope",
        "ConstantPoolPointerForwarder",
        "PersistentHandles",
    )

  def _convert_qualified_decl_ref(self, node: Dict[str, Any]) -> Dict[str, Any]:
    ref = node.get("referencedDecl", {})
    name = ref.get("name", "")
    # Preserve scope qualifiers like `ScopeType::SCRIPT_SCOPE`, `Builtin::kCompileLazy`,
    # `Smi::zero`, `Script::InitLineEnds`, `SharedFunctionInfo::kAgeSize`.
    src = self._source_slice_for_node(node)
    if src and "::" in src:
      clean_src = src.strip()
      if clean_src.startswith("v8::internal::"):
        clean_src = clean_src[len("v8::internal::"):]
      elif clean_src.startswith("i::"):
        clean_src = clean_src[len("i::"):]
      # Strip trailing template arguments if any.
      if "<" in clean_src:
        clean_src = clean_src[:clean_src.find("<")].strip()
      if "::" in clean_src:
        return {
            "type": "QualifiedName",
            "qualifier": clean_src.rsplit("::", 1)[0],
            "name": clean_src.rsplit("::", 1)[1],
            "qual_type": node.get("type", {}).get("qualType", ""),
        }
    return {
        "type": "Name",
        "name": name,
        "qual_type": node.get("type", {}).get("qualType", ""),
    }

  def convert_expr(self, node: Any) -> Any:
    if node is None:
      return None
    node = self.unwrap(node)
    if not isinstance(node, dict):
      return None
    kind = node.get("kind")

    if kind == "ImplicitCastExpr":
      ck = node.get("castKind", "")
      inner = node.get("inner", [])
      if not inner:
        raise UnsupportedCxxAstError(f"Empty ImplicitCastExpr ({ck})")
      if ck == "IntegralCast":
        target_type = _strip_leading_ns(_node_qual_type(node))
        return {
            "type": "CastExpr",
            "cast_kind": "ImplicitIntegralCast",
            "target_type": target_type,
            "expr": self.convert_expr(inner[0]),
        }
      return self.convert_expr(inner[0])

    if kind == "ConstantExpr":
      val = node.get("value")
      qt = _node_qual_type(node)
      if val is not None:
        if qt == "bool":
          return {
              "type": "Literal",
              "value": val in ("true", "1", True),
              "qual_type": qt
          }
        try:
          return {"type": "Literal", "value": int(val), "qual_type": qt}
        except (ValueError, TypeError):
          pass
      inner = node.get("inner", [])
      if inner:
        return self.convert_expr(inner[0])
      raise UnsupportedCxxAstError(f"Empty ConstantExpr: {node}")

    if kind == "IntegerLiteral":
      qt = _node_qual_type(node, "int")
      return {"type": "Literal", "value": int(node["value"]), "qual_type": qt}

    if kind == "FloatingLiteral":
      return {
          "type": "Literal",
          "value": float(node["value"]),
          "qual_type": node.get("type", {}).get("qualType", "double"),
      }

    if kind == "CXXBoolLiteralExpr":
      return {
          "type": "Literal",
          "value": bool(node["value"]),
          "qual_type": "bool"
      }

    if kind == "StringLiteral":
      raw = node.get("value", '""')
      if len(raw) >= 2 and raw[0] == '"' and raw[-1] == '"':
        val = bytes(raw[1:-1], "utf-8").decode("unicode_escape")
      else:
        val = raw
      return {"type": "Literal", "value": val, "qual_type": "const char*"}

    if kind == "CharacterLiteral":
      return {
          "type": "Literal",
          "value": int(node["value"]),
          "qual_type": "char",
      }

    if kind == "CXXNullPtrLiteralExpr":
      return {"type": "Literal", "value": None, "qual_type": "std::nullptr_t"}

    if kind == "CXXThisExpr":
      return {"type": "This"}

    if kind == "DeclRefExpr":
      return self._convert_qualified_decl_ref(node)

    if kind == "MemberExpr":
      inner = node.get("inner", [])
      base_raw = self.unwrap(inner[0]) if inner else None
      is_this = isinstance(base_raw,
                           dict) and base_raw.get("kind") == "CXXThisExpr"
      base = self.convert_expr(inner[0]) if inner else {"type": "This"}
      return {
          "type": "Member",
          "base": base,
          "member": node.get("name", ""),
          "is_arrow": bool(node.get("isArrow", False)),
          "is_this_member": is_this,
          "qual_type": node.get("type", {}).get("qualType", ""),
      }

    if kind == "ArraySubscriptExpr":
      inner = node.get("inner", [])
      return {
          "type": "Subscript",
          "base": self.convert_expr(inner[0]),
          "index": self.convert_expr(inner[1]),
          "qual_type": node.get("type", {}).get("qualType", ""),
      }

    if kind == "InitListExpr":
      vals = [self.convert_expr(c) for c in node.get("inner", [])]
      return {
          "type":
              "InitList",
          "values":
              vals,
          "qual_type":
              _strip_leading_ns(node.get("type", {}).get("qualType", "")),
      }

    if kind in ("CXXConstructExpr", "CXXTemporaryObjectExpr"):
      args = [
          self.convert_expr(a)
          for a in node.get("inner", [])
          if a.get("kind") != "CXXDefaultArgExpr"
      ]
      ctor_type = _strip_leading_ns(node.get("type", {}).get("qualType", ""))
      return {
          "type":
              "Construct",
          "ctor_type":
              ctor_type,
          "args":
              args,
          "is_copy_or_conv":
              len(args) == 1
              and not self._is_raii_scope_or_forwarder(ctor_type),
      }

    if kind == "CXXFunctionalCastExpr":
      inner = node.get("inner", [])
      target_type = _strip_leading_ns(_node_qual_type(node))
      if not inner:
        return {
            "type": "Construct",
            "ctor_type": target_type,
            "args": [],
            "is_copy_or_conv": False
        }
      sub = self.unwrap(inner[0])
      if sub.get("kind") == "CXXConstructExpr":
        return self.convert_expr(sub)
      return {
          "type": "CastExpr",
          "target_type": target_type,
          "expr": self.convert_expr(inner[0]),
      }

    if kind in ("CStyleCastExpr", "CXXStaticCastExpr", "CXXReinterpretCastExpr",
                "CXXConstCastExpr", "CXXDynamicCastExpr"):
      inner = node.get("inner", [])
      if not inner:
        raise UnsupportedCxxAstError(f"Empty cast expression: {kind}")
      target_type = _strip_leading_ns(_node_qual_type(node))
      return {
          "type": "CastExpr",
          "cast_kind": kind,
          "target_type": target_type,
          "expr": self.convert_expr(inner[0]),
      }

    if kind == "OffsetOfExpr":
      src = self._source_slice_for_node(node) or ""
      # When offsetof(...) comes from the <stddef.h> macro, Clang sets both
      # begin and end expansionLoc to just the `offsetof` token. Scan balanced
      # parens forward from begin in the verified source buffer if needed.
      if not re.search(r"(?:__builtin_)?offsetof\s*\(", src):
        rng = node.get("range", {})
        begin = rng.get("begin", {}) if isinstance(rng, dict) else {}
        el = begin.get("expansionLoc") if isinstance(begin, dict) else None
        if isinstance(el, dict) and el.get(
            "file", "<stdin>") == "<stdin>" and "offset" in el:
          off = el["offset"]
          lp = self.tu_source.find("(", off)
          if lp != -1:
            depth = 0
            rp = -1
            for idx in range(lp, len(self.tu_source)):
              ch = self.tu_source[idx]
              if ch == "(":
                depth += 1
              elif ch == ")":
                depth -= 1
                if depth == 0:
                  rp = idx
                  break
            if rp != -1:
              src = self.tu_source[off:rp + 1]
      m = re.search(
          r"(?:__builtin_)?offsetof\s*\(\s*([^,]+?)\s*,\s*([^)]+?)\s*\)", src)
      if not m:
        raise UnsupportedCxxAstError(
            f"Could not extract record/field from OffsetOfExpr: {src!r}")
      return {
          "type": "OffsetOf",
          "record": _strip_leading_ns(m.group(1).strip()),
          "field": m.group(2).strip(),
      }

    if kind == "UnaryExprOrTypeTraitExpr":
      trait = node.get("name", "")
      arg_type = _strip_leading_ns(node.get("argType", {}).get("qualType", ""))
      return {
          "type": "TypeTrait",
          "trait": trait,
          "arg_type": arg_type,
      }

    if kind == "CallExpr":
      inner = node.get("inner", [])
      if not inner:
        raise UnsupportedCxxAstError("CallExpr without callee")
      callee_raw = self.unwrap(inner[0])
      raw_args = [a for a in inner[1:] if a.get("kind") != "CXXDefaultArgExpr"]
      args = [self.convert_expr(a) for a in raw_args]

      if callee_raw.get("kind") == "DeclRefExpr":
        ref_name = callee_raw.get("referencedDecl", {}).get("name", "")
        if ref_name == "__MERGE_MODEL_STEP":
          step_lit = args[0]["value"] if args and args[0].get(
              "type") == "Literal" else ""
          return {
              "type": "MergeModelStep",
              "step": step_lit,
              "allow_safepoint": False
          }
        if ref_name == "__MERGE_MODEL_STEP_WITH_SAFEPOINT":
          step_lit = args[0]["value"] if args and args[0].get(
              "type") == "Literal" else ""
          return {
              "type": "MergeModelStep",
              "step": step_lit,
              "allow_safepoint": True
          }
        if ref_name == "__SYNCHRONIZATION_POINT":
          sp_name = args[0]["value"] if args and args[0].get(
              "type") == "Literal" else ""
          allow_sp = bool(args[1]["value"]) if len(args) > 1 and args[1].get(
              "type") == "Literal" else False
          return {
              "type": "SyncPoint",
              "name": sp_name,
              "allow_safepoint": allow_sp
          }

        if ref_name in ("Cast", "Is", "TryCast", "TrustedCast", "SbxCast"):
          targ = self._extract_template_type_arg(node, callee_raw, ref_name)
          # For TryCast(val, &out) with deduced T, inspect the second argument's type!
          if targ is None and ref_name == "TryCast" and len(raw_args) >= 2:
            out_qt = raw_args[1].get("type", {}).get("qualType", "")
            lt = out_qt.find("<")
            if lt != -1:
              targ = _strip_leading_ns(
                  _extract_balanced_template_args(out_qt, lt) or "")
          if not targ:
            raise UnsupportedCxxAstError(
                f"Could not resolve template type argument for {ref_name}()")
          return {
              "type": "TemplateCall",
              "name": ref_name,
              "template_arg": targ,
              "args": args,
          }

        if ref_name == "move" and len(args) == 1:
          return args[0]

      callee = self.convert_expr(inner[0])
      return {
          "type": "Call",
          "callee": callee,
          "args": args,
          "qual_type": node.get("type", {}).get("qualType", ""),
      }

    if kind == "CXXMemberCallExpr":
      inner = node.get("inner", [])
      if not inner:
        raise UnsupportedCxxAstError("CXXMemberCallExpr without callee")
      callee_raw = self.unwrap(inner[0])
      raw_args = [a for a in inner[1:] if a.get("kind") != "CXXDefaultArgExpr"]
      args = [self.convert_expr(a) for a in raw_args]
      if (callee_raw.get("kind") == "MemberExpr" and
          callee_raw.get("name") == "operator bool" and not args):
        return {
            "type": "UnaryOp",
            "op": "bool",
            "operand": self.convert_expr(callee_raw["inner"][0]),
            "is_postfix": False,
        }
      callee = self.convert_expr(inner[0])
      return {
          "type": "Call",
          "callee": callee,
          "args": args,
          "qual_type": node.get("type", {}).get("qualType", ""),
      }

    if kind == "CXXOperatorCallExpr":
      inner = node.get("inner", [])
      if len(inner) < 2:
        raise UnsupportedCxxAstError(
            f"CXXOperatorCallExpr with <2 children: {len(inner)}")
      op_node = self.unwrap(inner[0])
      op_str = op_node.get("referencedDecl", {}).get("name", "")
      if op_str.startswith("operator"):
        op = op_str[len("operator"):].strip()
      else:
        op = op_str
      operands_raw = [
          a for a in inner[1:] if a.get("kind") != "CXXDefaultArgExpr"
      ]

      if op == "->":
        return self.convert_expr(operands_raw[0])
      if op == "()":
        return {
            "type": "Call",
            "callee": self.convert_expr(operands_raw[0]),
            "args": [self.convert_expr(a) for a in operands_raw[1:]],
            "qual_type": node.get("type", {}).get("qualType", ""),
        }
      if op == "[]":
        return {
            "type": "Subscript",
            "base": self.convert_expr(operands_raw[0]),
            "index": self.convert_expr(operands_raw[1]),
            "qual_type": node.get("type", {}).get("qualType", ""),
        }
      if op in ("++", "--"):
        # C++ operator++(int) has a dummy int arg when postfix.
        is_postfix = len(operands_raw) >= 2
        return {
            "type": "UnaryOp",
            "op": op,
            "operand": self.convert_expr(operands_raw[0]),
            "is_postfix": is_postfix,
        }
      if op == "=":
        return {
            "type": "Assign",
            "op": "=",
            "target": self.convert_expr(operands_raw[0]),
            "value": self.convert_expr(operands_raw[1]),
        }
      if op in self._COMPOUND_OPS:
        return {
            "type": "Assign",
            "op": op,
            "target": self.convert_expr(operands_raw[0]),
            "value": self.convert_expr(operands_raw[1]),
        }
      if len(operands_raw) == 1 and op in ("*", "!", "~", "+", "-"):
        return {
            "type": "UnaryOp",
            "op": op,
            "operand": self.convert_expr(operands_raw[0]),
            "is_postfix": False,
        }
      if len(operands_raw) == 2 and op in self._BINARY_OP_MAP:
        return {
            "type": "BinOp",
            "op": self._BINARY_OP_MAP[op],
            "left": self.convert_expr(operands_raw[0]),
            "right": self.convert_expr(operands_raw[1]),
            "qual_type": node.get("type", {}).get("qualType", ""),
        }
      raise UnsupportedCxxAstError(
          f"Unsupported CXXOperatorCallExpr operator {op!r} with {len(operands_raw)} operands"
      )

    if kind == "BinaryOperator":
      op = node.get("opcode", "")
      inner = node.get("inner", [])
      if op == "=":
        return {
            "type": "Assign",
            "op": "=",
            "target": self.convert_expr(inner[0]),
            "value": self.convert_expr(inner[1]),
        }
      if op in self._BINARY_OP_MAP:
        return {
            "type": "BinOp",
            "op": self._BINARY_OP_MAP[op],
            "left": self.convert_expr(inner[0]),
            "right": self.convert_expr(inner[1]),
            "qual_type": _node_qual_type(node),
        }
      raise UnsupportedCxxAstError(f"Unsupported BinaryOperator opcode: {op!r}")

    if kind == "CompoundAssignOperator":
      op = node.get("opcode", "")
      if op not in self._COMPOUND_OPS:
        raise UnsupportedCxxAstError(
            f"Unsupported CompoundAssignOperator opcode: {op!r}")
      inner = node.get("inner", [])
      return {
          "type": "Assign",
          "op": op,
          "target": self.convert_expr(inner[0]),
          "value": self.convert_expr(inner[1]),
      }

    if kind == "UnaryOperator":
      op = node.get("opcode", "")
      if op not in ("!", "-", "+", "~", "*", "&", "++", "--"):
        raise UnsupportedCxxAstError(
            f"Unsupported UnaryOperator opcode: {op!r}")
      inner = node.get("inner", [])
      return {
          "type": "UnaryOp",
          "op": op,
          "operand": self.convert_expr(inner[0]),
          "is_postfix": bool(node.get("isPostfix", False)),
          "qual_type": _node_qual_type(node),
      }

    if kind == "ConditionalOperator":
      inner = node.get("inner", [])
      return {
          "type": "Ternary",
          "cond": self.convert_expr(inner[0]),
          "then": self.convert_expr(inner[1]),
          "else": self.convert_expr(inner[2]),
      }

    if kind == "LambdaExpr":
      return self._convert_lambda_expr(node)

    raise UnsupportedCxxAstError(
        f"Unsupported Clang AST expression kind: {kind!r} (type={node.get('type')})"
    )

  def _convert_lambda_expr(self, node: Dict[str, Any]) -> Dict[str, Any]:
    inner = node.get("inner", [])
    if not inner or inner[0].get("kind") != "CXXRecordDecl":
      raise UnsupportedCxxAstError("Malformed LambdaExpr without CXXRecordDecl")
    rec = inner[0]
    field_decls = [
        c for c in rec.get("inner", []) if c.get("kind") == "FieldDecl"
    ]
    params_info: List[Tuple[str, str, bool]] = []
    body_stmts: List[Dict[str, Any]] = []
    is_mutable = False
    for member in rec.get("inner", []):
      if (member.get("kind") == "CXXMethodDecl" and
          member.get("name") == "operator()"):
        mtype = member.get("type", {}).get("qualType", "")
        is_mutable = not mtype.rstrip().endswith("const")
        for mchild in member.get("inner", []):
          if mchild.get("kind") == "ParmVarDecl":
            ptyp = mchild.get("type", {}).get("qualType", "")
            params_info.append((
                ptyp,
                mchild.get("name", ""),
                "&" in ptyp and "&&" not in ptyp,
            ))
          elif mchild.get("kind") == "CompoundStmt":
            body_stmts = self.convert_compound_stmt(mchild)["body"]
        break

    # Capture init expressions appear between CXXRecordDecl and the body CompoundStmt.
    capture_inits = [c for c in inner[1:] if c.get("kind") != "CompoundStmt"]
    captures: List[Dict[str, Any]] = []
    for fdecl, init_raw in zip(field_decls, capture_inits):
      fqual = fdecl.get("type", {}).get("qualType", "")
      by_ref = "&" in fqual and "&&" not in fqual
      unwrapped_init = self.unwrap(init_raw)
      # By-value captures of non-trivial class types (e.g. `[v]() mutable` on
      # `std::vector<int>`) have an unnamed FieldDecl and a copy/move
      # CXXConstructExpr wrapping the captured DeclRefExpr.
      if (not fdecl.get("name") and isinstance(unwrapped_init, dict) and
          unwrapped_init.get("kind") == "CXXConstructExpr"):
        non_default = [
            a for a in unwrapped_init.get("inner", [])
            if a.get("kind") != "CXXDefaultArgExpr"
        ]
        if len(non_default) == 1:
          cand = self.unwrap(non_default[0])
          if isinstance(cand, dict) and cand.get("kind") == "DeclRefExpr":
            unwrapped_init = cand
      if unwrapped_init.get("kind") == "CXXThisExpr":
        captures.append({"name": "this", "by_ref": True, "is_this": True})
      elif unwrapped_init.get("kind") == "DeclRefExpr":
        cname = unwrapped_init.get("referencedDecl", {}).get("name", "")
        captures.append({"name": cname, "by_ref": by_ref, "is_this": False})
      else:
        cname = fdecl.get("name", "")
        captures.append({
            "name": cname,
            "by_ref": by_ref,
            "is_this": False,
            "init": self.convert_expr(init_raw),
        })

    return {
        "type": "Lambda",
        "captures": captures,
        "is_mutable": is_mutable,
        "params": params_info,
        "body": body_stmts,
    }


@dataclass(frozen=True)
class ParsedMethod:
  name: str
  params: Tuple[Tuple[str, str, bool], ...]
  body: List[Dict[str, Any]]
  return_type: str = ""


@dataclass
class ParsedCxxBundle:
  bg_stmts: List[Dict[str, Any]]
  fg_stmts: List[Dict[str, Any]]
  fwd_methods: Dict[str, List[ParsedMethod]]
  vcm_stmts: List[Dict[str, Any]]
  vcm_helpers: Dict[str, List[ParsedMethod]]
  sfi_methods: Dict[str, List[ParsedMethod]]
  raw_compiler_cc: str = ""
  raw_sfi_inl: str = ""
  raw_sfi_cc: str = ""
  git_rev: Optional[str] = None


def extract_compiler_snippets(content: str) -> Dict[str, str]:
  """Extracts BackgroundMergeTask, ConstantPoolPointerForwarder, and VerifyCodeMerge from compiler.cc."""
  bg_m = re.search(
      r"(void\s+BackgroundMergeTask::BeginMergeInBackground\s*\([^)]*\))\s*\{",
      content,
  )
  if not bg_m:
    raise RuntimeError("BeginMergeInBackground not found in compiler.cc")
  bg_start = bg_m.end() - 1
  bg_end = find_matching_brace(content, bg_start)
  if bg_end == -1:
    raise RuntimeError("Matching brace not found for BeginMergeInBackground")
  bg_raw = content[bg_m.start():bg_end + 1]

  fg_m = re.search(
      r"(Handle<SharedFunctionInfo>\s+BackgroundMergeTask::CompleteMergeInForeground\s*\([^)]*\))\s*\{",
      content,
  )
  if not fg_m:
    raise RuntimeError("CompleteMergeInForeground not found in compiler.cc")
  fg_start = fg_m.end() - 1
  fg_end = find_matching_brace(content, fg_start)
  if fg_end == -1:
    raise RuntimeError("Matching brace not found for CompleteMergeInForeground")
  fg_raw = content[fg_m.start():fg_end + 1]

  fwd_raw = ""
  fwd_start = content.find("class ConstantPoolPointerForwarder")
  if fwd_start != -1:
    bstart = content.find("{", fwd_start)
    bend = find_matching_brace(content, bstart)
    if bend != -1:
      fwd_raw = content[fwd_start:bend + 2]

  vcm_helper_raw = ""
  vhm = re.search(
      r"template\s*<[^>]+>\s*void\s+VerifyConstantPoolEntry\s*\([^)]*\)\s*\{",
      content,
  )
  if vhm:
    hb_start = vhm.end() - 1
    hb_end = find_matching_brace(content, hb_start)
    if hb_end != -1:
      vcm_helper_raw = content[vhm.start():hb_end + 1]

  vcm_raw = ""
  vm = re.search(r"void\s+VerifyCodeMerge\s*\([^)]*\)\s*\{", content)
  if vm:
    vb_start = vm.end() - 1
    vb_end = find_matching_brace(content, vb_start)
    if vb_end != -1:
      vcm_raw = content[vm.start():vb_end + 1]

  file_decls: List[str] = []
  fgc_m = re.search(
      r"^static\s+bool\s+force_gc_during_next_merge_for_testing_\s*=\s*[^;]+;",
      content,
      re.MULTILINE,
  )
  if fgc_m:
    file_decls.append(fgc_m.group(0))

  return {
      "bg_raw": bg_raw,
      "fg_raw": fg_raw,
      "fwd_raw": fwd_raw,
      "vcm_helper_raw": vcm_helper_raw,
      "vcm_raw": vcm_raw,
      "file_decls_raw": "\n".join(file_decls),
  }


_SFI_INL_PATTERNS: Tuple[Tuple[str, str], ...] = (
    ("SetTrustedData",
     r"void\s+SharedFunctionInfo::SetTrustedData\s*\([^)]*\)\s*\{"),
    ("SetUntrustedData",
     r"void\s+SharedFunctionInfo::SetUntrustedData\s*\([^)]*\)\s*\{"),
    ("HasScopeInfo",
     r"bool\s+SharedFunctionInfo::HasScopeInfo\s*\(\s*\)\s*const\s*\{"),
    ("HasScopeInfo_acquire",
     r"bool\s+SharedFunctionInfo::HasScopeInfo\s*\(\s*AcquireLoadTag[^)]*\)\s*const\s*\{"
    ),
    ("scope_info_acquire",
     r"DEF_ACQUIRE_GETTER\(\s*SharedFunctionInfo\s*,\s*scope_info\s*,[^)]+\)\s*\{"
    ),
    ("scope_info",
     r"DEF_GETTER\(\s*SharedFunctionInfo\s*,\s*scope_info\s*,[^)]+\)\s*\{"),
    ("set_raw_scope_info",
     r"void\s+SharedFunctionInfo::set_raw_scope_info\s*\([^)]*\)\s*\{"),
    ("outer_scope_info",
     r"DEF_GETTER\(\s*SharedFunctionInfo\s*,\s*outer_scope_info\s*,[^)]+\)\s*\{"
    ),
    ("HasOuterScopeInfo",
     r"bool\s+SharedFunctionInfo::HasOuterScopeInfo\s*\(\s*\)\s*const\s*\{"),
    ("GetOuterScopeInfo",
     r"Tagged<ScopeInfo>\s+SharedFunctionInfo::GetOuterScopeInfo\s*\(\s*\)\s*const\s*\{"
    ),
    ("TryGetScopeInfoForMerge",
     r"Tagged<ScopeInfo>\s+SharedFunctionInfo::TryGetScopeInfoForMerge\s*\(\s*\)\s*const\s*\{"
    ),
    ("TryGetOuterScopeInfo",
     r"Tagged<ScopeInfo>\s+SharedFunctionInfo::TryGetOuterScopeInfo\s*\(\s*\)\s*const\s*\{"
    ),
    ("set_outer_scope_info",
     r"void\s+SharedFunctionInfo::set_outer_scope_info\s*\([^)]*\)\s*\{"),
    ("HasFeedbackMetadata",
     r"bool\s+SharedFunctionInfo::HasFeedbackMetadata\s*\(\s*\)\s*const\s*\{"),
    ("HasFeedbackMetadata_acquire",
     r"bool\s+SharedFunctionInfo::HasFeedbackMetadata\s*\(\s*AcquireLoadTag[^)]*\)\s*const\s*\{"
    ),
    ("set_feedback_metadata",
     r"void\s+SharedFunctionInfo::set_feedback_metadata\s*\([^)]*\)\s*\{"),
    ("is_compiled",
     r"bool\s+SharedFunctionInfo::is_compiled\s*\(\s*\)\s*const\s*\{"),
    ("set_bytecode_array",
     r"void\s+SharedFunctionInfo::set_bytecode_array\s*\([^)]*\)\s*\{"),
)

_SFI_PARSED_METHOD_NAMES = frozenset({
    "SetTrustedData",
    "SetUntrustedData",
    "HasScopeInfo",
    "scope_info",
    "set_raw_scope_info",
    "outer_scope_info",
    "HasOuterScopeInfo",
    "GetOuterScopeInfo",
    "TryGetScopeInfoForMerge",
    "TryGetOuterScopeInfo",
    "set_outer_scope_info",
    "HasFeedbackMetadata",
    "set_feedback_metadata",
    "is_compiled",
    "set_bytecode_array",
    "CopyFrom",
})


def extract_sfi_snippets(sfi_inl: str, sfi_cc: str) -> str:
  chunks: List[str] = []
  for _, pat in _SFI_INL_PATTERNS:
    m = re.search(pat, sfi_inl)
    if not m:
      continue
    bstart = m.end() - 1
    bend = find_matching_brace(sfi_inl, bstart)
    if bend != -1:
      chunks.append(sfi_inl[m.start():bend + 1])

  m_cf = re.search(r"void\s+SharedFunctionInfo::CopyFrom\s*\([^)]*\)\s*\{",
                   sfi_cc)
  if not m_cf:
    raise RuntimeError(
        "SharedFunctionInfo::CopyFrom not found in shared-function-info.cc")
  bstart = m_cf.end() - 1
  bend = find_matching_brace(sfi_cc, bstart)
  if bend == -1:
    raise RuntimeError(
        "Matching brace not found for SharedFunctionInfo::CopyFrom")
  chunks.append(sfi_cc[m_cf.start():bend + 1])
  return "\n\n".join(chunks)


_TU_PARSE_CACHE: Dict[Tuple[str, Optional[str]], ParsedCxxBundle] = {}


def _extract_method_decl(
    decl_node: Dict[str, Any],
    conv: ClangAstConverter,
) -> Optional[ParsedMethod]:
  params: List[Tuple[str, str, bool]] = []
  body: Optional[List[Dict[str, Any]]] = None
  for child in decl_node.get("inner", []):
    ck = child.get("kind")
    if ck == "ParmVarDecl":
      ptyp = child.get("type", {}).get("qualType", "")
      params.append((ptyp, child.get("name", ""), "&" in ptyp and
                     "&&" not in ptyp))
    elif ck == "CompoundStmt":
      body = conv.convert_compound_stmt(child)["body"]
  if body is None:
    return None
  return ParsedMethod(
      name=decl_node.get("name", ""),
      params=tuple(params),
      body=body,
      return_type=decl_node.get("type", {}).get("qualType", ""),
  )


def parse_cxx_snippet(
    code: str,
    *,
    use_v8_pch: bool = False,
) -> Dict[str, ParsedMethod]:
  """Parses a standalone C++20 snippet (for unit tests) via real Clang AST."""
  cmd = [CLANG_CXX_BIN]
  if use_v8_pch:
    pch_path, override_dir = _get_clang_pch_path()
    if override_dir:
      cmd.append(f"-I{override_dir}")
    cmd.extend(["-include-pch", pch_path])
    cmd.extend(_CLANG_COMMON_FLAGS)
  else:
    cmd.extend(["-std=c++20"])
  cmd.extend([
      "-x",
      "c++",
      "-fsyntax-only",
      "-fno-color-diagnostics",
      "-Xclang",
      "-ast-dump=json",
      "-",
  ])
  proc = subprocess.run(
      cmd,
      cwd=V8_ROOT,
      input=code,
      capture_output=True,
      text=True,
      check=False,
  )
  if proc.returncode != 0:
    raise RuntimeError(
        f"Clang C++20 snippet parse failed (exit {proc.returncode}):\n{proc.stderr}"
    )

  ast_root = json.loads(proc.stdout)
  conv = ClangAstConverter(code)
  methods: Dict[str, ParsedMethod] = {}
  cur_file = [""]

  def _update_file(n: Dict[str, Any]) -> None:
    loc = n.get("loc", {})
    if isinstance(loc, dict):
      if "file" in loc:
        cur_file[0] = loc["file"]
      elif "expansionLoc" in loc and isinstance(
          loc["expansionLoc"], dict) and "file" in loc["expansionLoc"]:
        cur_file[0] = loc["expansionLoc"]["file"]
    rng = n.get("range", {})
    if isinstance(rng, dict):
      b = rng.get("begin", {})
      if isinstance(b, dict) and "file" in b:
        cur_file[0] = b["file"]

  def _walk(nodes: List[Dict[str, Any]]) -> None:
    for n in nodes:
      _update_file(n)
      k = n.get("kind")
      if k == "NamespaceDecl":
        # Skip std:: and system header namespaces entirely.
        if cur_file[0] == "<stdin>" or n.get("name") not in ("std",
                                                             "__gnu_cxx"):
          _walk(n.get("inner", []))
      elif k in ("FunctionDecl", "CXXMethodDecl"):
        if cur_file[0] != "<stdin>":
          continue
        pm = _extract_method_decl(n, conv)
        if pm is not None:
          methods[pm.name] = pm

  _walk(ast_root.get("inner", []))
  return methods


def parse_v8_merge_bundle(
    *,
    cxx_path: Optional[str] = None,
    git_rev: Optional[str] = None,
    compiler_content: Optional[str] = None,
    sfi_inl_content: Optional[str] = None,
    sfi_cc_content: Optional[str] = None,
    bmt_h_content: Optional[str] = None,
) -> ParsedCxxBundle:
  """Parses real V8 merge C++ (compiler.cc + shared-function-info-inl.h + shared-function-info.cc)."""
  raw_compiler = read_repo_file(
      "src/codegen/compiler.cc",
      git_rev=git_rev,
      override_path=cxx_path,
      content_override=compiler_content,
  )
  raw_sfi_inl = read_repo_file(
      "src/objects/shared-function-info-inl.h",
      git_rev=git_rev,
      content_override=sfi_inl_content,
  )
  raw_sfi_cc = read_repo_file(
      "src/objects/shared-function-info.cc",
      git_rev=git_rev,
      content_override=sfi_cc_content,
  )

  override_headers: Optional[Dict[str, str]] = None
  if bmt_h_content is not None or git_rev is not None:
    override_headers = {}
    bmt_text = read_repo_file(
        "src/codegen/background-merge-task.h",
        git_rev=git_rev,
        content_override=bmt_h_content,
    )
    override_headers["src/codegen/background-merge-task.h"] = bmt_text
    if git_rev is not None:
      for extra_hdr in (
          "src/objects/shared-function-info.h",
          "src/base/synchronization-point-support.h",
          "src/common/synchronization-point-support.h",
      ):
        try:
          override_headers[extra_hdr] = read_repo_file(
              extra_hdr, git_rev=git_rev)
        except FileNotFoundError:
          pass

  snippets = extract_compiler_snippets(raw_compiler)
  sfi_raw = extract_sfi_snippets(raw_sfi_inl, raw_sfi_cc)

  tu_parts = ["namespace v8::internal {"]
  if snippets["file_decls_raw"]:
    tu_parts.append(snippets["file_decls_raw"])
  if sfi_raw:
    tu_parts.append(sfi_raw)
  if snippets["vcm_helper_raw"]:
    tu_parts.append(snippets["vcm_helper_raw"])
  if snippets["vcm_raw"]:
    tu_parts.append(snippets["vcm_raw"])
  if snippets["fwd_raw"]:
    tu_parts.append(snippets["fwd_raw"])
  tu_parts.append(snippets["bg_raw"])
  tu_parts.append(snippets["fg_raw"])
  tu_parts.append("}  // namespace v8::internal")
  tu = "\n".join(tu_parts)

  pch_path, override_dir = _get_clang_pch_path(override_headers)
  cache_key = (tu, pch_path)
  cached = _TU_PARSE_CACHE.get(cache_key)
  if cached is not None:
    return ParsedCxxBundle(
        bg_stmts=cached.bg_stmts,
        fg_stmts=cached.fg_stmts,
        fwd_methods=cached.fwd_methods,
        vcm_stmts=cached.vcm_stmts,
        vcm_helpers=cached.vcm_helpers,
        sfi_methods=cached.sfi_methods,
        raw_compiler_cc=raw_compiler,
        raw_sfi_inl=raw_sfi_inl,
        raw_sfi_cc=raw_sfi_cc,
        git_rev=git_rev,
    )

  cmd = [CLANG_CXX_BIN]
  if override_dir:
    cmd.append(f"-I{override_dir}")
  cmd.extend(["-include-pch", pch_path])
  cmd.extend(_CLANG_COMMON_FLAGS)
  cmd.extend([
      "-x",
      "c++",
      "-fsyntax-only",
      "-fno-color-diagnostics",
      "-Xclang",
      "-ast-dump=json",
      "-",
  ])
  proc = subprocess.run(
      cmd,
      cwd=V8_ROOT,
      input=tu,
      capture_output=True,
      text=True,
      check=False,
  )
  if proc.returncode != 0:
    raise RuntimeError(
        f"Clang C++20 AST parse failed (exit {proc.returncode}):\n{proc.stderr}"
    )

  ast_root = json.loads(proc.stdout)
  conv = ClangAstConverter(tu)

  bg_stmts: List[Dict[str, Any]] = []
  fg_stmts: List[Dict[str, Any]] = []
  vcm_stmts: List[Dict[str, Any]] = []
  vcm_helpers: Dict[str, List[ParsedMethod]] = {}
  fwd_methods: Dict[str, List[ParsedMethod]] = {}
  sfi_methods: Dict[str, List[ParsedMethod]] = {}

  def _collect_top(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for n in nodes:
      if n.get("kind") == "NamespaceDecl":
        out.extend(_collect_top(n.get("inner", [])))
      else:
        out.append(n)
    return out

  for node in _collect_top(ast_root.get("inner", [])):
    kind = node.get("kind")
    name = node.get("name", "")
    if kind == "FunctionDecl" and name == "VerifyCodeMerge":
      pm = _extract_method_decl(node, conv)
      if pm is not None:
        vcm_stmts = pm.body
    elif kind == "FunctionTemplateDecl" and name == "VerifyConstantPoolEntry":
      # Prefer concrete template instantiations (which have fully resolved ASTs
      # with no UnresolvedLookupExpr/dependent nodes) over the primary template.
      specs = [
          sub for sub in node.get("inner", [])
          if sub.get("kind") == "FunctionDecl"
      ]
      chosen = specs[1:] if len(specs) > 1 else specs
      for sub in chosen:
        pm = _extract_method_decl(sub, conv)
        if pm is not None:
          vcm_helpers.setdefault(name, []).append(pm)
    elif kind == "CXXMethodDecl" and name == "BeginMergeInBackground":
      pm = _extract_method_decl(node, conv)
      if pm is not None:
        bg_stmts = pm.body
    elif kind == "CXXMethodDecl" and name == "CompleteMergeInForeground":
      pm = _extract_method_decl(node, conv)
      if pm is not None:
        fg_stmts = pm.body
    elif kind == "CXXMethodDecl" and name in _SFI_PARSED_METHOD_NAMES:
      pm = _extract_method_decl(node, conv)
      if pm is not None:
        sfi_methods.setdefault(name, []).append(pm)
    elif kind == "CXXRecordDecl" and name == "ConstantPoolPointerForwarder":
      for member in node.get("inner", []):
        mk = member.get("kind")
        if mk == "CXXMethodDecl":
          pm = _extract_method_decl(member, conv)
          if pm is not None:
            fwd_methods.setdefault(pm.name, []).append(pm)
        elif mk == "FunctionTemplateDecl":
          # Prefer concrete template instantiations over the uninstantiated
          # primary template (which contains unresolved/dependent AST nodes).
          specs = [
              sub for sub in member.get("inner", [])
              if sub.get("kind") == "CXXMethodDecl"
          ]
          chosen = specs[1:] if len(specs) > 1 else specs
          for sub in chosen:
            pm = _extract_method_decl(sub, conv)
            if pm is not None:
              fwd_methods.setdefault(pm.name, []).append(pm)

  bundle = ParsedCxxBundle(
      bg_stmts=bg_stmts,
      fg_stmts=fg_stmts,
      fwd_methods=fwd_methods,
      vcm_stmts=vcm_stmts,
      vcm_helpers=vcm_helpers,
      sfi_methods=sfi_methods,
      raw_compiler_cc=raw_compiler,
      raw_sfi_inl=raw_sfi_inl,
      raw_sfi_cc=raw_sfi_cc,
      git_rev=git_rev,
  )
  _TU_PARSE_CACHE[cache_key] = bundle
  return bundle
