#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Deterministic classifier, validator, and formatter for V8 security triage.

1. Map empirical symptoms (`Symptom`) + threat model scope (`ExecutionScope`)
   to base Classification, Severity, Security Impact, and Priority (`SYMPTOM_RULES`).
2. Empirically execute `d8` via `EmpiricalPocRunner`:
   - Run 1a (`run1_flagless`): `[d8_binary, sec_flag, poc_path]` -> if reproduced:
     `reproduces_with_security_poc=True`, `enabled_by_default=True`.
   - Run 1b (`run1_with_poc_helpers`): `[d8_binary, sec_flag, *helper_flags, poc_path]`
     (for standard POC trigger/verification helpers such as `--allow-natives-syntax`,
     `--expose-gc`, `--verify-heap`) -> if reproduced:
     `reproduces_with_security_poc=True`, `enabled_by_default=True`.
   - Run 2 (`run2_with_flags`): `[d8_binary, sec_flag, *extra_flags, poc_path]`
     (when non-helper feature/config flags are required) -> if reproduced:
     `reproduces_with_security_poc=True`, `enabled_by_default=False`.
   - Otherwise: `reproduces_with_security_poc=False`, `enabled_by_default=False`
     (downgrades to `Bug` in regular mode or `Not a Bug (Intended Behavior)` in sandbox mode).
3. Resolve git commits, V8 versions, and `DIR_METADATA` components, validate all
   report constraints, and emit the Buganizer Markdown report + `(User Only)` action plan.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass, field
from enum import StrEnum
import functools
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any

# ==============================================================================
# 1. DECLARATIVE TRIAGE POLICY & SYMPTOM TABLE (EDIT HERE TO ADJUST RULES)
# ==============================================================================


class Symptom(StrEnum):
  """High-level empirical symptom observed during reproduction."""

  OOB_WRITE = "oob_write"
  UAF = "uaf"
  TYPE_CONFUSION = "type_confusion"
  OOB_READ = "oob_read"
  UNINITIALIZED_READ = "uninitialized_read"
  DCHECK_CORRUPTS_STATE = "dcheck_corrupts_state"
  DCHECK_BENIGN = "dcheck_benign"
  VERIFICATION_CHECK_FAILURE = "verification_check_failure"
  PRODUCTION_CHECK_OR_TRAP = "production_check_or_trap"
  NULLPTR_DEREF = "nullptr_deref"
  HOLE_LEAK = "hole_leak"
  NOT_REPRODUCED = "not_reproduced"


# yapf: disable
SYMPTOM_EMPIRICAL_GUIDE: dict[Symptom, str] = {
    Symptom.OOB_WRITE: "Out-of-bounds memory write observed in GDB or ASan (`WRITE of size ...` or a store instruction writing past an object, array backing store, or buffer boundary).",
    Symptom.UAF: "Use-after-free observed in GDB or ASan (`heap-use-after-free` or dereferencing/writing to freed, swept, or unmapped memory after garbage collection or deallocation).",
    Symptom.TYPE_CONFUSION: "Type confusion observed in GDB or crash output (an object or pointer is interpreted with an incompatible V8 `Map`, C++ type, or Wasm signature, causing invalid field access, corrupted pointers, or indirect call mismatch).",
    Symptom.OOB_READ: "Out-of-bounds memory read without any write primitive or corrupted pointer/Map dereference observed in GDB or ASan (`READ of size ...` or a load instruction reading past an object or buffer boundary without modifying memory; if an OOB read loads a bogus Map, pointer, or call target that is subsequently dereferenced, written, or called in GDB, select `type_confusion` or `oob_write`).",
    Symptom.UNINITIALIZED_READ: "Read of uninitialized memory observed in MSan (`use-of-uninitialized-value`), GDB, or output inspection without out-of-bounds write or type confusion.",
    Symptom.DCHECK_CORRUPTS_STATE: "`DCHECK` assertion failure in a dcheck-enabled build where the unmodified POC does not directly crash a non-dcheck Release build with a specific memory violation (`oob_write`, `uaf`, `type_confusion`, `oob_read`, `nullptr_deref`), and inspecting GDB/source on a Release build shows that continuing past the compiled-out `DCHECK` corrupts downstream memory, bounds, or heap state.",
    Symptom.DCHECK_BENIGN: "`DCHECK` assertion failure in a dcheck-enabled build where inspecting GDB/source on a non-dcheck Release build shows that continuing past the compiled-out `DCHECK` does not corrupt memory or state (Release execution is either mitigated by subsequent release logic/checks or the assert is overzealous), including `DCHECK` or diagnostic failures inside developer-only printing/diagnostic helpers (e.g. `OBJECT_PRINT` / `%DebugPrint` formatting logic) where Release state is uncorrupted.",
    Symptom.VERIFICATION_CHECK_FAILURE: "`CHECK` or verification failure inside a diagnostic verification pass (`--verify-heap`, `%HeapObjectVerify`, `--verify-turboshaft`, `--verify-simplified-lowering`, `--maglev-assert`, `--assert-types`, `OBJECT_PRINT`) that is confirmed in GDB/source to detect actual prior heap, object, or IR state corruption in Release (not a bug in the diagnostic printer/verifier itself).",
    Symptom.PRODUCTION_CHECK_OR_TRAP: "Unconditional release-enabled `CHECK`, `SBXCHECK`, `UNREACHABLE`, `FATAL`, `ABORT`, builtin `Abort` (`int3`/`brk #0`/`ud2`), or hardened `libc++` bounds check (`__libcpp_verbose_abort` / `std::vector::operator[]` trap) active in default Release builds that deterministically terminates the process before any invalid memory access occurs.",
    Symptom.NULLPTR_DEREF: "Deterministic null or near-null pointer dereference in GDB (`SIGSEGV` reading or writing at address `0x0` or a small fixed offset near zero without an attacker-controlled base address).",
    Symptom.HOLE_LEAK: "Internal V8 `_Hole_` sentinel (`the_hole`, `property_cell_hole`, etc.) leaked to JavaScript variables or return values without causing memory corruption or bypassing a release check.",
    Symptom.NOT_REPRODUCED: "POC did not reproduce any crash, assertion failure, sanitizer error, or observable anomaly across tested build configurations and revisions.",
}
# yapf: enable


class ExecutionScope(StrEnum):
  """Threat model scope based on sandbox_mode and affected state."""
  REGULAR = "regular"
  SANDBOX_IN_SANDBOX_ONLY = "sandbox_in_sandbox_only"
  SANDBOX_OUT_OF_SANDBOX = "sandbox_out_of_sandbox"


class Classification(StrEnum):
  """Buganizer issue type / triage classification."""
  VULNERABILITY = "Vulnerability"
  BUG = "Bug"
  INTENDED_BEHAVIOR = "Not a Bug (Intended Behavior)"
  FAILED_TO_REPRODUCE = "Failed to Reproduce"


class SecurityImpact(StrEnum):
  """Whether the issue impacts shipping Chrome configurations."""
  YES = "Yes"
  NONE = "None"


@dataclass(frozen=True)
class SymptomRule:
  """Deterministic outcome for a (Symptom, ExecutionScope) pair."""
  classification: Classification
  severity: str | None
  security_impact: SecurityImpact
  default_priority: str
  default_impact_reason: str


@dataclass(frozen=True)
class SymptomSpec:
  """Declarative policy specification across all ExecutionScopes for a Symptom."""
  regular: SymptomRule
  in_sandbox_reason: str
  out_of_sandbox: SymptomRule | None = None


_V = Classification.VULNERABILITY
_B = Classification.BUG
_WAI = Classification.INTENDED_BEHAVIOR
_FTR = Classification.FAILED_TO_REPRODUCE
_YES = SecurityImpact.YES
_NONE = SecurityImpact.NONE
_SBX_ESCAPE_RULE = SymptomRule(
    classification=_V,
    severity="S2",
    security_impact=_YES,
    default_priority="P2",
    default_impact_reason=(
        "shipping, V8 Sandbox bypass affecting out-of-sandbox state (`v8-sandbox`)"
    ),
)
_SBX_READ_BYPASS_RULE = SymptomRule(
    classification=_B,
    severity=None,
    security_impact=_NONE,
    default_priority="P2",
    default_impact_reason=(
        "read-only V8 Sandbox bypass (out-of-sandbox read without write access"
        " is tracked as a regular bug)"),
)
_SBX_IN_CORRUPT_MSG = (
    "in-sandbox memory corruption without affecting out-of-sandbox state is"
    " working as intended under the V8 Sandbox threat model")
_SBX_READ_MSG = (
    "read-only access in V8 Sandbox mode is working as intended (arbitrary read"
    " is part of the sandbox threat model)")

_SYMPTOM_SPECS: dict[Symptom, SymptomSpec] = {
    Symptom.OOB_WRITE:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S1",
                _YES,
                "P1",
                "shipping, web-exploitable memory corruption in renderer",
            ),
            in_sandbox_reason=_SBX_IN_CORRUPT_MSG,
            out_of_sandbox=_SBX_ESCAPE_RULE,
        ),
    Symptom.UAF:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S1",
                _YES,
                "P1",
                "shipping, web-exploitable use-after-free in renderer",
            ),
            in_sandbox_reason=_SBX_IN_CORRUPT_MSG,
            out_of_sandbox=_SBX_ESCAPE_RULE,
        ),
    Symptom.TYPE_CONFUSION:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S1",
                _YES,
                "P1",
                "shipping, web-exploitable type confusion in renderer",
            ),
            in_sandbox_reason=(
                "in-sandbox type confusion without affecting out-of-sandbox"
                " state is working as intended under the V8 Sandbox threat"
                " model"),
            out_of_sandbox=_SBX_ESCAPE_RULE,
        ),
    Symptom.OOB_READ:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S3",
                _YES,
                "P2",
                "shipping, in-renderer out-of-bounds read",
            ),
            in_sandbox_reason=_SBX_READ_MSG,
            out_of_sandbox=_SBX_READ_BYPASS_RULE,
        ),
    Symptom.UNINITIALIZED_READ:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S3",
                _YES,
                "P2",
                "shipping, in-renderer uninitialized memory read",
            ),
            in_sandbox_reason=_SBX_READ_MSG,
            out_of_sandbox=_SBX_READ_BYPASS_RULE,
        ),
    Symptom.DCHECK_CORRUPTS_STATE:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S1",
                _YES,
                "P1",
                "shipping; DCHECK is compiled out in release builds and"
                " execution continues into state/memory corruption",
            ),
            in_sandbox_reason=(
                "in-sandbox state corruption hitting a DCHECK without"
                " affecting out-of-sandbox state"),
            out_of_sandbox=_SBX_ESCAPE_RULE,
        ),
    Symptom.DCHECK_BENIGN:
        SymptomSpec(
            regular=SymptomRule(
                _B,
                None,
                _NONE,
                "P2",
                "benign DCHECK mitigated by subsequent release logic",
            ),
            in_sandbox_reason=(
                "in-sandbox corruption hitting a debug assertion without"
                " sandbox escape"),
        ),
    Symptom.VERIFICATION_CHECK_FAILURE:
        SymptomSpec(
            regular=SymptomRule(
                _V,
                "S1",
                _YES,
                "P1",
                "shipping; verification CHECK failure indicates prior heap"
                " corruption",
            ),
            in_sandbox_reason=(
                "in-sandbox heap verification failure without affecting"
                " out-of-sandbox state"),
            out_of_sandbox=_SBX_ESCAPE_RULE,
        ),
    Symptom.PRODUCTION_CHECK_OR_TRAP:
        SymptomSpec(
            regular=SymptomRule(
                _B,
                None,
                _NONE,
                "P2",
                "unconditional production CHECK/SBXCHECK/trap safely"
                " terminates execution",
            ),
            in_sandbox_reason=(
                "safe termination via SBXCHECK/CHECK or hardened libc++ bounds"
                " check"),
        ),
    Symptom.NULLPTR_DEREF:
        SymptomSpec(
            regular=SymptomRule(
                _B,
                None,
                _NONE,
                "P2",
                "deterministic nullptr/near-null dereference crash",
            ),
            in_sandbox_reason=(
                "harmless nullptr dereference inside sandbox testing"
                " environment"),
        ),
    Symptom.HOLE_LEAK:
        SymptomSpec(
            regular=SymptomRule(
                _B,
                None,
                _NONE,
                "P2",
                "unexploited internal Hole sentinel leak (segregated in"
                " read-only space)",
            ),
            in_sandbox_reason=(
                "unexploited Hole sentinel leak inside sandbox testing"
                " environment"),
        ),
    Symptom.NOT_REPRODUCED:
        SymptomSpec(
            regular=SymptomRule(
                _FTR,
                None,
                _NONE,
                "P3",
                "could not be reproduced locally",
            ),
            in_sandbox_reason="could not be reproduced locally",
        ),
}

SYMPTOM_RULES: dict[tuple[Symptom, ExecutionScope], SymptomRule] = {}
for _s, _spec in _SYMPTOM_SPECS.items():
  SYMPTOM_RULES[(_s, ExecutionScope.REGULAR)] = _spec.regular
  _in_rule = SymptomRule(
      classification=_FTR if _s == Symptom.NOT_REPRODUCED else _WAI,
      severity=None,
      security_impact=_NONE,
      default_priority="P3",
      default_impact_reason=_spec.in_sandbox_reason,
  )
  SYMPTOM_RULES[(_s, ExecutionScope.SANDBOX_IN_SANDBOX_ONLY)] = _in_rule
  SYMPTOM_RULES[(_s, ExecutionScope.SANDBOX_OUT_OF_SANDBOX)] = (
      _spec.out_of_sandbox if _spec.out_of_sandbox is not None else _in_rule)

HOTLIST_SECURITY_IMPACT_HEAD = "5432216"
HOTLIST_SECURITY_IMPACT_BETA = "5433097"
HOTLIST_SECURITY_IMPACT_STABLE = "5432902"
HOTLIST_SECURITY_IMPACT_EXTENDED = "5432548"
HOTLIST_SECURITY_IMPACT_NONE = "5433277"
HOTLIST_V8_SANDBOX = "4802478"
HOTLIST_V8_UNSUPPORTED_CHROME = "8384111"
RELEASE_CHANNEL_IMPACT_HOTLISTS = (
    HOTLIST_SECURITY_IMPACT_HEAD,
    HOTLIST_SECURITY_IMPACT_BETA,
    HOTLIST_SECURITY_IMPACT_STABLE,
    HOTLIST_SECURITY_IMPACT_EXTENDED,
)
CUSTOM_FIELD_RELEASE_BLOCK = "1223086"
DEVTOOLS_COMPONENT_PATH = "Platform > DevTools"
DEVTOOLS_COMPONENT_ID = "1457055"


def _norm_comp_key(text: str) -> str:
  return re.sub(r"\s*>\s*", " > ", text.strip().lower())


def _parse_dir_metadata_file(md_file: Path) -> tuple[str | None, str | None]:
  try:
    text = md_file.read_text(encoding="utf-8", errors="replace")
  except OSError:
    return (None, None)
  m_id = re.search(r"component_id\s*:\s*(\d+)", text)
  m_comp = re.search(r'component\s*:\s*"([^"]+)"', text)
  comp_id = m_id.group(1) if m_id else None
  comp_path = (
      re.sub(r"\s*>\s*", " > ",
             m_comp.group(1).strip()) if m_comp else None)
  if comp_id == DEVTOOLS_COMPONENT_ID and not comp_path:
    comp_path = DEVTOOLS_COMPONENT_PATH
  return (comp_path, comp_id)


@functools.lru_cache(maxsize=8)
def _scan_dir_metadata_root(
    root: Path,) -> tuple[dict[str, str], dict[str, tuple[str, str]]]:
  """Scans DIR_METADATA files in a repository root (cached)."""
  id_to_path: dict[str, str] = {}
  key_to_comp: dict[str, tuple[str, str]] = {}
  md_files: list[Path] = []
  try:
    out = subprocess.run(
        ["git", "-C",
         str(root), "ls-files", "DIR_METADATA", "*/DIR_METADATA"],
        capture_output=True,
        text=True,
        check=True,
        timeout=5,
    ).stdout.splitlines()
    md_files = [root / rel.strip() for rel in out if rel.strip()]
  except (subprocess.SubprocessError, OSError):
    for pattern in ("DIR_METADATA", "*/DIR_METADATA", "src/*/DIR_METADATA",
                    "src/*/*/DIR_METADATA"):
      md_files.extend(sorted(root.glob(pattern)))

  for md_file in md_files:
    c_path, c_id = _parse_dir_metadata_file(md_file)
    if c_id and c_path and c_id not in id_to_path:
      id_to_path[c_id] = c_path
  for md_file in md_files:
    c_path, c_id = _parse_dir_metadata_file(md_file)
    if not c_id:
      continue
    c_path = c_path or id_to_path.get(c_id, "")
    pair = (c_path, c_id)
    key_to_comp[c_id] = pair
    if c_path:
      key_to_comp.setdefault(_norm_comp_key(c_path), pair)
      key_to_comp.setdefault(c_path.rsplit(">", 1)[-1].strip().lower(), pair)
    rel_dir = md_file.parent.relative_to(root).as_posix().lower()
    if rel_dir and rel_dir != ".":
      key_to_comp.setdefault(rel_dir, pair)
      if rel_dir.startswith("src/"):
        key_to_comp.setdefault(rel_dir[4:], pair)
  return (id_to_path, key_to_comp)


def _scan_dir_metadata(
    repo_root: Path | None,
) -> tuple[dict[str, str], dict[str, tuple[str, str]]]:
  roots = [
      r.resolve()
      for r in (repo_root, _detect_default_repo_root())
      if r is not None and r.is_dir() and (
          (r / "DIR_METADATA").is_file() or (r / "src").is_dir())
  ]
  for root in dict.fromkeys(roots):
    id_to_path, key_to_comp = _scan_dir_metadata_root(root)
    if id_to_path or key_to_comp:
      return (id_to_path, key_to_comp)
  return ({}, {})


def _lookup_path_in_dir_metadata(
    raw_token: str, repo_root: Path | None) -> tuple[str, str] | None:
  """Walks up parent directories of a V8 source path to find DIR_METADATA."""
  roots = [
      r.resolve()
      for r in (repo_root, _detect_default_repo_root())
      if r is not None and r.is_dir()
  ]
  for root in dict.fromkeys(roots):
    candidate = (root / raw_token.strip()).resolve()
    try:
      candidate.relative_to(root)
    except ValueError:
      continue
    if not candidate.exists():
      continue
    cur = candidate if candidate.is_dir() else candidate.parent
    while True:
      md_file = cur / "DIR_METADATA"
      if md_file.is_file():
        c_path, c_id = _parse_dir_metadata_file(md_file)
        if c_id:
          return (c_path or "", c_id)
      if cur == root or cur.parent == cur:
        break
      cur = cur.parent
  return None


def resolve_component(
    raw_comp: Any,
    current_issue: dict[str, Any] | None = None,
    repo_root: Path | None = None,
) -> tuple[str, str, bool] | None:
  """Resolves `proposed_component` (ID, source path, or dict) to `(path, id, already_set)`."""
  cur_comp_id = str((current_issue or {}).get("component_id") or "").strip()

  if isinstance(raw_comp, int):
    raw_comp = str(raw_comp)
  if isinstance(raw_comp, str) and raw_comp.strip():
    token = raw_comp.strip()
    if not token.isdigit():
      by_dir = _lookup_path_in_dir_metadata(token, repo_root)
      if by_dir is not None and by_dir[0]:
        c_path, c_id = by_dir
        return (c_path, c_id, bool(cur_comp_id and cur_comp_id == c_id))
    id_to_path, key_to_comp = _scan_dir_metadata(repo_root)
    if token.isdigit():
      return (id_to_path.get(token, ""), token,
              bool(cur_comp_id and cur_comp_id == token))
    if by_dir is not None:
      c_path, c_id = by_dir
      return (c_path or id_to_path.get(c_id, ""), c_id,
              bool(cur_comp_id and cur_comp_id == c_id))
    matched = key_to_comp.get(_norm_comp_key(token))
    if matched is not None:
      c_path, c_id = matched
      return (c_path, c_id, bool(cur_comp_id and cur_comp_id == c_id))
    return None

  if isinstance(raw_comp, dict):
    raw_path = str(raw_comp.get("path") or raw_comp.get("name") or "").strip()
    raw_id = str(raw_comp.get("id") or raw_comp.get("component_id") or
                 "").strip()
    if not (raw_id and raw_path):
      id_to_path, key_to_comp = _scan_dir_metadata(repo_root)
      if raw_id and not raw_path:
        raw_path = id_to_path.get(raw_id, "")
      elif raw_path and not raw_id:
        by_dir = _lookup_path_in_dir_metadata(raw_path, repo_root)
        if by_dir is not None:
          raw_path, raw_id = (
              by_dir[0] or id_to_path.get(by_dir[1], ""),
              by_dir[1],
          )
        else:
          matched = key_to_comp.get(_norm_comp_key(raw_path))
          if matched is not None:
            raw_path, raw_id = matched
    if not raw_id or not raw_id.isdigit():
      return None
    already_set = (
        raw_comp["already_set"] if isinstance(
            raw_comp.get("already_set"), bool) else bool(cur_comp_id and
                                                         cur_comp_id == raw_id))
    return (raw_path, raw_id, already_set)
  return None


def _detect_env_session_id() -> str | None:
  for var in (
      "ANTIGRAVITY_CONVERSATION_ID",
      "INVOKER_INFO_ROOT_SESSION_ID",
      "INVOKER_INFO_SESSION_ID",
  ):
    val = os.environ.get(var, "").strip()
    if val:
      return val
  return None


def _detect_default_repo_root() -> Path:
  for p in [Path.cwd().resolve(), *Path.cwd().resolve().parents]:
    if (p / "include" / "v8-version.h").is_file():
      return p
  p = Path(__file__).resolve().parents[4]
  return p if (p / "include" /
               "v8-version.h").is_file() else Path.cwd().resolve()


DEFAULT_REPO_ROOT = _detect_default_repo_root()
DEFAULT_POC_TIMEOUT_SEC = 45.0

_SECURITY_POC_FLAGS = {"--run-as-security-poc", "--run-as-sandbox-security-poc"}
_SANDBOX_TESTING_FLAGS = {
    "--expose-memory-corruption-api",
    "--sandbox-testing",
    "--sandbox-fuzzing",
}
# POC trigger/verification helper flags that exercise shipping, enabled-by-default
# V8 code (note `--run-as-security-poc` implies `--fuzzing`, which strips unsafe
# `%`-runtime calls not in `Runtime::IsEnabledForFuzzing()`). Requiring these
# helpers does NOT make a bug `enabled_by_default=False` (`Security_Impact-None`).
_POC_HELPER_FLAGS = {
    "--allow-natives-syntax",
    "--expose-gc",
    "--verify-heap",
    "--fuzzing",
    "--jit-fuzzing",
}
# Under --run-as-[sandbox]-security-poc, V8 exits with:
# - 0: clean exit, or harmless crash caught by SandboxTesting::Mode::kForTesting
#      (`_exit(0)` in `src/sandbox/testing.cc:FilterCrash` / `SanitizerFaultHandler`).
# - 1: uncaught JS exception or invalid CLI option in d8 (`src/d8/d8.cc`).
# - 255 / 0xffffffff (`OS::ExitProcess(-1)`): flag contradiction / disallowed flag
#      (`--flag-processing-mode=exit-on-error`), `FatalOOM`, or `FatalNoSecurityImpact`.
# - < 0 (or 128 + sig in shell wrappers): fatal crash signal (`SIGSEGV`, `SIGABRT`,
#      `SIGILL`, `SIGTRAP`, `SIGBUS`, `SIGFPE`) from a genuine crash or sandbox violation.
_MINUS_ONE_EXIT_CODES = {255, 0xFFFFFFFF}


def _canonical_flag(flag: str) -> str:
  token = flag.strip()
  if not token.startswith("-"):
    return token
  name = "--" + token.split("=", 1)[0].lower().replace("_", "-").lstrip("-")
  return "--no-experimental" if name == "--noexperimental" else name


# ==============================================================================
# 2. EMPIRICAL SECURITY POC RUNNER (`EmpiricalPocRunner` — 2 RUNS, NO ALLOWLIST)
# ==============================================================================


@dataclass(frozen=True)
class PocStageRun:
  stage: str
  command: list[str]
  returncode: int | None
  timed_out: bool
  reproduced: bool
  output_excerpt: str

  def to_dict(self) -> dict[str, Any]:
    return asdict(self)


@dataclass(frozen=True)
class EmpiricalPocResult:
  reproduces_with_security_poc: bool
  enabled_by_default: bool
  matched_stage: str | None
  effective_d8_flags: list[str]
  summary: str
  stages: list[PocStageRun]

  def to_dict(self) -> dict[str, Any]:
    return asdict(self)


class EmpiricalPocRunner:
  """Executes `d8` with `--run-as-[sandbox]-security-poc` in up to 2 runs."""

  def __init__(self,
               timeout_sec: float = DEFAULT_POC_TIMEOUT_SEC,
               repo_root: Path | None = None):
    self.timeout_sec = timeout_sec
    default_root = _detect_default_repo_root()
    self.repo_root = repo_root if repo_root is not None else (
        default_root if default_root.is_dir() else None)

  def resolve_path(self, raw_path: str) -> Path:
    p = Path(raw_path)
    if p.is_absolute() or p.exists():
      return p.resolve()
    return (self.repo_root / p).resolve() if self.repo_root is not None and (
        self.repo_root / p).exists() else p.resolve()

  @staticmethod
  def has_crashed(returncode: int | None) -> bool:
    """Returns True iff d8 terminated with a crash signal (not 0, 1, or -1/255)."""
    if (returncode is None or returncode in (0, 1) or
        returncode in _MINUS_ONE_EXIT_CODES):
      return False
    return returncode < 0 or 128 < returncode < 160

  @staticmethod
  def _build_env() -> dict[str, str]:
    """Ensures sanitizers abort (`SIGABRT`) instead of exiting with code 1."""
    env = os.environ.copy()
    for var in ("ASAN_OPTIONS", "UBSAN_OPTIONS", "MSAN_OPTIONS"):
      cur = env.get(var, "").strip()
      if "abort_on_error" not in cur:
        env[var] = f"{cur}:abort_on_error=1" if cur else "abort_on_error=1"
    return env

  def _run_stage(
      self,
      stage_name: str,
      d8_exec: Path,
      stage_flags: list[str],
      poc_exec: Path,
      poc_basename: str,
  ) -> PocStageRun:
    cmd = [str(d8_exec), *stage_flags, str(poc_exec)]
    display_cmd = ["d8", *stage_flags, poc_basename]
    run_cwd = str(
        self.repo_root
    ) if self.repo_root is not None and self.repo_root.is_dir() else None
    try:
      proc = subprocess.run(
          cmd,
          cwd=run_cwd,
          env=self._build_env(),
          capture_output=True,
          text=True,
          errors="replace",
          timeout=self.timeout_sec,
          check=False)
      combined = f"{proc.stdout or ''}\n{proc.stderr or ''}".strip()
      reproduced = self.has_crashed(proc.returncode)
      lines = [l.strip() for l in combined.splitlines() if l.strip()]
      excerpt = " | ".join(lines[:3])[:240] if lines else ""
      return PocStageRun(
          stage=stage_name,
          command=display_cmd,
          returncode=proc.returncode,
          timed_out=False,
          reproduced=reproduced,
          output_excerpt=excerpt,
      )
    except subprocess.TimeoutExpired:
      return PocStageRun(
          stage=stage_name,
          command=display_cmd,
          returncode=None,
          timed_out=True,
          reproduced=False,
          output_excerpt=f"Timed out after {self.timeout_sec}s",
      )

  def run(self,
          d8_binary: str,
          poc_path: str,
          d8_flags: list[str],
          sandbox_mode: bool = False) -> EmpiricalPocResult:
    """Executes flagless Run 1a, POC-helper Run 1b (`enabled_by_default=True`), and feature-flag Run 2 (`enabled_by_default=False`)."""
    d8_exec = self.resolve_path(d8_binary)
    poc_exec = self.resolve_path(poc_path)
    poc_basename = os.path.basename(poc_path.strip())
    sec_flag = "--run-as-sandbox-security-poc" if sandbox_mode else "--run-as-security-poc"
    extra_flags = [
        f.strip()
        for f in d8_flags
        if f.strip() and _canonical_flag(f) not in _SECURITY_POC_FLAGS and
        _canonical_flag(f) not in _SANDBOX_TESTING_FLAGS
    ]
    helper_flags = [
        f for f in extra_flags if _canonical_flag(f) in _POC_HELPER_FLAGS
    ]
    non_helper_flags = [
        f for f in extra_flags if _canonical_flag(f) not in _POC_HELPER_FLAGS
    ]
    run1_flags = [sec_flag]
    r1 = self._run_stage("run1_flagless", d8_exec, run1_flags, poc_exec,
                         poc_basename)
    stages = [r1]
    if r1.reproduced:
      return EmpiricalPocResult(
          reproduces_with_security_poc=True,
          enabled_by_default=True,
          matched_stage="run1_flagless",
          effective_d8_flags=run1_flags,
          summary=(f"Reproduced in Run 1 with `{sec_flag}` and no extra flags"
                   " (default production configuration)."),
          stages=stages,
      )
    if helper_flags:
      run1b_flags = [sec_flag, *helper_flags]
      r1b = self._run_stage("run1_with_poc_helpers", d8_exec, run1b_flags,
                            poc_exec, poc_basename)
      stages.append(r1b)
      if r1b.reproduced:
        return EmpiricalPocResult(
            reproduces_with_security_poc=True,
            enabled_by_default=True,
            matched_stage="run1_with_poc_helpers",
            effective_d8_flags=run1b_flags,
            summary=(
                f"Reproduced with `{' '.join(run1b_flags)}` (default-enabled"
                f" configuration with POC helper flag(s):"
                f" `{' '.join(helper_flags)}`)."),
            stages=stages,
        )
    if non_helper_flags:
      run2_flags = [sec_flag, *extra_flags]
      r2 = self._run_stage("run2_with_flags", d8_exec, run2_flags, poc_exec,
                           poc_basename)
      stages.append(r2)
      if r2.reproduced:
        return EmpiricalPocResult(
            reproduces_with_security_poc=True,
            enabled_by_default=False,
            matched_stage="run2_with_flags",
            effective_d8_flags=run2_flags,
            summary=(
                f"Reproduced in Run 2 with `{' '.join(run2_flags)}` (requires"
                f" non-default flag(s): `{' '.join(non_helper_flags)}`)."),
            stages=stages,
        )
    return EmpiricalPocResult(
        reproduces_with_security_poc=False,
        enabled_by_default=False,
        matched_stage=None,
        effective_d8_flags=extra_flags,
        summary=f"Did not reproduce under `{sec_flag}`.",
        stages=stages,
    )


# ==============================================================================
# 3. GIT & V8 VERSION RESOLVER
# ==============================================================================


class GitVersionResolver:
  """Resolves git commits, Gerrit links, V8 versions, and Cr-Commit-Positions."""

  def __init__(self, repo_root: Path):
    self.repo_root = repo_root

  def _run_git(self, args: list[str]) -> str | None:
    try:
      return subprocess.run(
          ["git", "-C", str(self.repo_root), *args],
          capture_output=True,
          text=True,
          check=True,
          timeout=5).stdout.strip()
    except (subprocess.SubprocessError, OSError):
      return None

  @staticmethod
  def parse_v8_version_header(header_text: str) -> str | None:
    major = re.search(r"#define\s+V8_MAJOR_VERSION\s+(\d+)", header_text)
    minor = re.search(r"#define\s+V8_MINOR_VERSION\s+(\d+)", header_text)
    build = re.search(r"#define\s+V8_BUILD_NUMBER\s+(\d+)", header_text)
    patch = re.search(r"#define\s+V8_PATCH_LEVEL\s+(\d+)", header_text)
    if not (major and minor and build):
      return None
    ver = f"{major.group(1)}.{minor.group(1)}.{build.group(1)}"
    return f"{ver}.{patch.group(1)}" if patch and int(
        patch.group(1)) > 0 else ver

  def is_git_repo(self) -> bool:
    return self._run_git(["rev-parse", "--is-inside-work-tree"]) == "true"

  def get_v8_version_at_rev(self, rev: str = "HEAD") -> str:
    header_text = self._run_git(["show", f"{rev}:include/v8-version.h"])
    if not header_text and (rev == "HEAD" or not self.is_git_repo()):
      local_header = self.repo_root / "include" / "v8-version.h"
      if local_header.exists():
        header_text = local_header.read_text(encoding="utf-8", errors="replace")
    return (header_text and
            self.parse_v8_version_header(header_text)) or "unknown"

  def format_change_commit(self, commit_ref: str) -> str:
    full_hash = self._run_git(
        ["rev-parse", "--verify", "--quiet", f"{commit_ref}^{{commit}}"])
    short_hash = self._run_git(["rev-parse", "--short=11", commit_ref
                               ]) or commit_ref
    title = self._run_git(["log", "-n", "1", "--format=%s", commit_ref
                          ]) if full_hash else None
    reviewed_on = self._run_git([
        "log", "-n", "1", "--format=%(trailers:key=Reviewed-on,valueonly=true)",
        commit_ref
    ]) if full_hash else None
    if reviewed_on:
      reviewed_on = reviewed_on.splitlines()[-1].strip()
    url = reviewed_on or f"https://chromium.googlesource.com/v8/v8/+/{full_hash or commit_ref}"
    version = self.get_v8_version_at_rev(commit_ref)
    return f'[commit {short_hash}]({url}) ("{title}", V8 {version})' if title else f"[commit {short_hash}]({url}) (V8 {version})"

  def format_build_version(self,
                           build_variants: list[str],
                           build_commit: str = "HEAD") -> str:
    version = self.get_v8_version_at_rev(build_commit)
    position = self._run_git([
        "log", "-n", "1",
        "--format=%(trailers:key=Cr-Commit-Position,valueonly=true)",
        build_commit
    ])
    rev_str = ""
    if position:
      position = position.splitlines()[-1].strip()
      m = re.search(r"\{#(\d+)\}", position)
      rev_str = f" (r{m.group(1)})" if m else f" ({position})"
    return f"{', '.join(build_variants)}, V8 {version}{rev_str}"

  def get_commit_author_email(self, commit_ref: str) -> str | None:
    full_hash = self._run_git(
        ["rev-parse", "--verify", "--quiet", f"{commit_ref}^{{commit}}"])
    if not full_hash:
      return None
    return self._run_git(["log", "-n", "1", "--format=%ae", commit_ref])

  def get_commit_fixed_bugs(self, commit_ref: str) -> list[str]:
    full_hash = self._run_git(
        ["rev-parse", "--verify", "--quiet", f"{commit_ref}^{{commit}}"])
    if not full_hash:
      return []
    body = self._run_git(["log", "-n", "1", "--format=%B", commit_ref]) or ""
    fixed_ids: list[str] = []
    bug_ids: list[str] = []
    for line in body.splitlines():
      m_fixed = re.match(r"^\s*Fixed\s*:\s*(.+)$", line, re.IGNORECASE)
      if m_fixed:
        for b_id in re.findall(r"\b(\d{6,})\b", m_fixed.group(1)):
          if b_id not in fixed_ids:
            fixed_ids.append(b_id)
        continue
      m_bug = re.match(r"^\s*Bug\s*:\s*(.+)$", line, re.IGNORECASE)
      if m_bug:
        for b_id in re.findall(r"\b(\d{6,})\b", m_bug.group(1)):
          if b_id not in bug_ids:
            bug_ids.append(b_id)
    return fixed_ids if fixed_ids else bug_ids

  def resolve_duplicate_of(self, data: dict[str, Any]) -> str | None:
    raw_dup = data.get("duplicate_of")
    if raw_dup is not None:
      s_dup = str(raw_dup).strip()
      if s_dup and s_dup.lower() not in ("none", "null", "n/a"):
        m = re.search(r"(\d+)", s_dup)
        return m.group(1) if m else s_dup
    fixed_raw = str(data.get("fixed_in_commit") or "").strip()
    if not fixed_raw or fixed_raw.lower() in ("unknown", "none", "n/a"):
      return None
    current_id = str((data.get("current_issue") or {}).get("id") or "").strip()
    for bug_id in self.get_commit_fixed_bugs(fixed_raw):
      if bug_id != current_id:
        return bug_id
    return None

  def resolve_proposed_owner(self, data: dict[str,
                                              Any]) -> tuple[str, str] | None:
    owner = data.get("proposed_owner")
    if isinstance(owner, dict):
      email = str(owner.get("email") or "").strip()
      reason = str(owner.get("reason") or "").strip()
      if email and reason:
        return (email, reason)
      if email:
        return (email, "subsystem maintainer / commit author")
    elif isinstance(owner, str) and owner.strip():
      return (owner.strip(), "subsystem maintainer / commit author")
    for field_name, label in (
        ("fixed_in_commit", "author of fixing commit"),
        ("introduced_in_commit", "author of introducing commit"),
    ):
      ref = str(data.get(field_name) or "").strip()
      if ref and ref.lower() not in ("unknown", "none", "n/a", "head"):
        email = self.get_commit_author_email(ref)
        if email:
          return (email, label)
    return None


# ==============================================================================
# 4. LENIENT JSON PARSING & INPUT VALIDATION
# ==============================================================================

_JSON_COMMENT_RE = re.compile(
    r'("(?:\\.|[^"\\])*")|(?://[^\n]*|/\*[\s\S]*?\*/)')
_JSON_TRAILING_COMMA_RE = re.compile(r'("(?:\\.|[^"\\])*")|,(?=\s*[\]}])')
_LOCAL_PATH_PATTERNS = (
    re.compile(r"/usr/local/"),
    re.compile(r"/home/[a-zA-Z0-9_-]+/"),
    re.compile(r"/google/src/"),
    re.compile(r"/tmp/"),
    re.compile(r"file://"),
)
_COMMIT_REF_RE = re.compile(
    r"^(?:HEAD|[0-9a-fA-F]{7,40}|refs/[^\s~^]+)(?:~[0-9]*|\^[0-9]*)*$")
_MAX_LINE_FIELD_CHARS = 350
_MAX_RATIONALE_CHARS = 700
_ALLOWED_TOP_KEYS = {
    "session_id",
    "symptom",
    "sandbox_mode",
    "affects_out_of_sandbox_state",
    "reproduction",
    "introduced_in_commit",
    "fixed_in_commit",
    "duplicate_of",
    "rationale",
    "proposed_owner",
    "proposed_component",
    "proposed_title",
    "current_title",
    "current_issue",
}
_ALLOWED_REPRO_KEYS = {
    "d8_binary",
    "poc_path",
    "d8_flags",
    "unsupported_port",
    "build_variants",
    "build_commit",
    "result",
    "verified_impact",
    "gdb_backtrace",
}


def _resolve_session_id(data: dict[str, Any],
                        env_session_id: str | None = None) -> str:
  raw = str(data.get("session_id") or "").strip()
  if raw and not (raw.startswith("<") and raw.endswith(">")):
    return raw
  return (env_session_id if env_session_id is not None else
          (_detect_env_session_id() or "")).strip()


def _resolve_current_title(data: dict[str, Any]) -> str:
  cur_issue = data.get("current_issue")
  if isinstance(cur_issue, dict) and cur_issue.get("title"):
    return str(cur_issue["title"]).strip()
  return str(data.get("current_title") or "").strip()


def load_lenient_json(raw_text: str) -> dict[str, Any]:
  without_comments = _JSON_COMMENT_RE.sub(
      lambda m: m.group(1) if m.group(1) is not None else "", raw_text)
  without_trailing_commas = _JSON_TRAILING_COMMA_RE.sub(
      lambda m: m.group(1) if m.group(1) is not None else "", without_comments)
  data = json.loads(without_trailing_commas)
  if not isinstance(data, dict):
    raise ValueError("Top-level JSON value must be an object ({...}).")
  return data


def check_no_local_paths(field_name: str, value: str,
                         errors: list[str]) -> None:
  for pat in _LOCAL_PATH_PATTERNS:
    if pat.search(value):
      errors.append(
          f"`{field_name}` contains a local workstation/temp path matching `{pat.pattern}`. Never include local paths in Buganizer comment fields."
      )
      break


def validate_triage_input(
    data: dict[str, Any],
    env_session_id: str | None = None,
    repo_root: Path | None = None,
) -> list[str]:
  """Validates all schema, length, path, and empirical reproduction rules."""
  errors: list[str] = []
  unknown_keys = set(data.keys()) - _ALLOWED_TOP_KEYS
  if unknown_keys:
    errors.append(
        f"Unknown top-level JSON field(s): {sorted(unknown_keys)}. Allowed fields: {sorted(_ALLOWED_TOP_KEYS)}."
    )
  session_id = _resolve_session_id(data, env_session_id)
  if not session_id:
    errors.append(
        "`session_id` is required (set automatically via `ANTIGRAVITY_CONVERSATION_ID` / `INVOKER_INFO_SESSION_ID` or pass in JSON)."
    )
  raw_symptom = data.get("symptom")
  valid_symptoms = [s.value for s in Symptom]
  symptom = Symptom(raw_symptom) if raw_symptom in valid_symptoms else None
  if symptom is None:
    errors.append(
        f"`symptom` must be one of {valid_symptoms} (got {raw_symptom!r}).")
  raw_sandbox_mode = data.get("sandbox_mode", False)
  sandbox_mode = raw_sandbox_mode if isinstance(raw_sandbox_mode,
                                                bool) else False
  if not isinstance(raw_sandbox_mode, bool):
    errors.append("`sandbox_mode` must be a boolean (`true` or `false`).")
  raw_affects_oos = data.get("affects_out_of_sandbox_state")
  if raw_affects_oos is not None and not isinstance(raw_affects_oos, bool):
    errors.append(
        "`affects_out_of_sandbox_state` must be a boolean (`true` or `false`).")
  elif sandbox_mode and symptom != Symptom.NOT_REPRODUCED and raw_affects_oos is None:
    errors.append(
        '`affects_out_of_sandbox_state` (`true` or `false`) is required when `sandbox_mode` is `true` and `symptom != "not_reproduced"`.'
    )
  elif not sandbox_mode and raw_affects_oos is True:
    errors.append(
        "`affects_out_of_sandbox_state` cannot be `true` when `sandbox_mode` is `false`."
    )

  repro = data.get("reproduction")
  if not isinstance(repro, dict):
    errors.append("`reproduction` must be an object.")
  else:
    unknown_repro_keys = set(repro.keys()) - _ALLOWED_REPRO_KEYS
    if unknown_repro_keys:
      errors.append(
          f"Unknown `reproduction` JSON field(s): {sorted(unknown_repro_keys)}. Allowed fields: {sorted(_ALLOWED_REPRO_KEYS)}."
      )
    poc_path = repro.get("poc_path")
    if not isinstance(poc_path, str) or not poc_path.strip():
      errors.append("`reproduction.poc_path` must be a non-empty string.")
    d8_flags = repro.get("d8_flags")
    valid_d8_flags = isinstance(d8_flags, list) and all(
        isinstance(f, str) for f in d8_flags)
    if not valid_d8_flags:
      errors.append("`reproduction.d8_flags` must be a list of flag strings.")
    else:
      for flag in d8_flags:
        check_no_local_paths("reproduction.d8_flags", flag, errors)
    raw_d8_binary = repro.get("d8_binary")
    if symptom is not None and symptom != Symptom.NOT_REPRODUCED:
      if not isinstance(raw_d8_binary, str) or not raw_d8_binary.strip():
        errors.append(
            '`reproduction.d8_binary` is required and must be a non-empty string when `symptom != "not_reproduced"`.'
        )
      else:
        runner = EmpiricalPocRunner(repo_root=repo_root)
        d8_exec = runner.resolve_path(raw_d8_binary.strip())
        if not (d8_exec.is_file() and os.access(d8_exec, os.X_OK)):
          errors.append(
              f"`reproduction.d8_binary` ({raw_d8_binary!r}) does not exist or is not executable."
          )
        if isinstance(poc_path, str) and poc_path.strip():
          poc_exec = runner.resolve_path(poc_path.strip())
          if not poc_exec.is_file():
            errors.append(
                f"`reproduction.poc_path` ({poc_path!r}) does not exist.")
    elif raw_d8_binary is not None and (not isinstance(raw_d8_binary, str) or
                                        not raw_d8_binary.strip()):
      errors.append(
          "`reproduction.d8_binary` must be a non-empty string when provided.")
    if repro.get("unsupported_port") is not None and not isinstance(
        repro.get("unsupported_port"), bool):
      errors.append(
          "`reproduction.unsupported_port` must be a boolean (`true` or `false`)."
      )
    build_variants = repro.get("build_variants")
    if not isinstance(build_variants, list) or not build_variants or not all(
        isinstance(b, str) and b.strip() for b in build_variants):
      errors.append(
          '`reproduction.build_variants` must be a non-empty list of strings (e.g. `["x64.release", "x64.asan"]`).'
      )
    else:
      for b in build_variants:
        check_no_local_paths("reproduction.build_variants", b, errors)
    result_text = repro.get("result")
    if not isinstance(result_text, str) or not result_text.strip():
      errors.append("`reproduction.result` must be a non-empty string.")
    else:
      if "\n" in result_text.strip():
        errors.append("`reproduction.result` must be a single line.")
      if len(result_text.strip()) > _MAX_LINE_FIELD_CHARS:
        errors.append(
            f"`reproduction.result` must be concise (at most {_MAX_LINE_FIELD_CHARS} characters)."
        )
      check_no_local_paths("reproduction.result", result_text, errors)
    verified_impact = repro.get("verified_impact")
    if symptom != Symptom.NOT_REPRODUCED:
      if not isinstance(verified_impact, str) or not verified_impact.strip():
        errors.append(
            "`reproduction.verified_impact` is required when reproduced.")
      else:
        if "\n" in verified_impact.strip():
          errors.append("`reproduction.verified_impact` must be a single line.")
        if len(verified_impact.strip()) > _MAX_LINE_FIELD_CHARS:
          errors.append(
              f"`reproduction.verified_impact` must be concise (at most {_MAX_LINE_FIELD_CHARS} characters)."
          )
        check_no_local_paths("reproduction.verified_impact", verified_impact,
                             errors)
      gdb_bt = repro.get("gdb_backtrace")
      if not isinstance(gdb_bt, (str, list)) or not gdb_bt:
        errors.append(
            "`reproduction.gdb_backtrace` is required when reproduced (string or list of lines)."
        )
      else:
        check_no_local_paths(
            "reproduction.gdb_backtrace",
            "\n".join(gdb_bt) if isinstance(gdb_bt, list) else gdb_bt, errors)
    b_commit = repro.get("build_commit")
    if b_commit is not None and (not isinstance(b_commit, str) or
                                 not _COMMIT_REF_RE.match(b_commit.strip())):
      errors.append(
          f"`reproduction.build_commit` ({b_commit!r}) must be `HEAD` or a valid git commit revision (e.g. `deadbeef` or `deadbeef~1`)."
      )

  for commit_field in ("introduced_in_commit", "fixed_in_commit"):
    c_val = data.get(commit_field)
    if c_val is not None:
      if not isinstance(c_val, str):
        errors.append(f"`{commit_field}` must be a string or null.")
      elif c_val.strip() and c_val.strip().lower() not in (
          "unknown", "none", "n/a") and not _COMMIT_REF_RE.match(c_val.strip()):
        errors.append(
            f'`{commit_field}` ({c_val!r}) must be a valid git commit hash (7-40 hex chars), `HEAD`, or `"Unknown"`.'
        )
  raw_dup = data.get("duplicate_of")
  if raw_dup is not None and not isinstance(raw_dup, (str, int)):
    errors.append("`duplicate_of` must be a bug ID string/integer or null.")
  rationale = data.get("rationale")
  if not isinstance(rationale, str) or not rationale.strip():
    errors.append("`rationale` is required and must be a non-empty string.")
  else:
    if "\n" in rationale.strip():
      errors.append("`rationale` must be a single paragraph (no line breaks).")
    if len(rationale.strip()) > _MAX_RATIONALE_CHARS:
      errors.append(
          f"`rationale` must be concise (at most {_MAX_RATIONALE_CHARS} characters)."
      )
    check_no_local_paths("rationale", rationale, errors)
  git_resolver = GitVersionResolver(repo_root or _detect_default_repo_root())
  resolved_owner = git_resolver.resolve_proposed_owner(data)
  if resolved_owner is None:
    errors.append(
        '`proposed_owner` is required when `introduced_in_commit` / `fixed_in_commit` author cannot be resolved from git (pass an email string or `{"email": "...", "reason": "..."}`).'
    )
  else:
    check_no_local_paths("proposed_owner.email", resolved_owner[0], errors)
    check_no_local_paths("proposed_owner.reason", resolved_owner[1], errors)
  resolved_comp = resolve_component(
      data.get("proposed_component"),
      data.get("current_issue"),
      repo_root=repo_root)
  if resolved_comp is None:
    errors.append(
        '`proposed_component` must be a numeric Buganizer component ID (e.g. `"1456177"`), a V8 source directory/file with `DIR_METADATA` (e.g. `"src/maglev"`), or `{"path": "...", "id": "..."}`.'
    )
  elif resolved_comp[0]:
    check_no_local_paths("proposed_component.path", resolved_comp[0], errors)
  title = data.get("proposed_title")
  if not isinstance(title, str) or not title.strip():
    errors.append("`proposed_title` must be a non-empty string.")
  elif "\n" in title.strip():
    errors.append("`proposed_title` must be a single line.")
  else:
    check_no_local_paths("proposed_title", title, errors)
  return errors


# ==============================================================================
# 5. CLASSIFICATION ENGINE & REPORT RENDERER
# ==============================================================================


@dataclass
class TriageDecision:
  classification: Classification
  security_impact: SecurityImpact
  security_impact_rationale: str
  severity: str | None
  priority: str
  component_path: str
  component_id: str
  component_already_set: bool
  execution_scope: ExecutionScope = ExecutionScope.REGULAR
  reproduces_with_security_poc: bool = True
  enabled_by_default: bool = True
  effective_d8_flags: list[str] = field(default_factory=list)
  empirical_run: dict[str, Any] | None = None
  hotlists_to_add: list[tuple[str, str]] = field(default_factory=list)
  hotlists_to_remove: list[tuple[str, str]] = field(default_factory=list)
  clear_release_block: bool = False


def _has_enable_inspector(d8_flags: list[str]) -> bool:
  return any(_canonical_flag(f) == "--enable-inspector" for f in d8_flags)


def compute_triage_decision(
    data: dict[str, Any],
    repo_root: Path | None = None,
    poc_timeout: float = DEFAULT_POC_TIMEOUT_SEC,
) -> TriageDecision:
  """Computes the deterministic triage outcome from empirical JSON inputs."""
  symptom = Symptom(data["symptom"])
  repro = data["reproduction"]
  raw_d8_flags = [f.strip() for f in repro.get("d8_flags", []) if f.strip()]
  sandbox_mode = bool(data.get("sandbox_mode", False))
  affects_oos = bool(data.get("affects_out_of_sandbox_state", False))

  if symptom == Symptom.NOT_REPRODUCED:
    reproduces_with_security_poc = False
    enabled_by_default = False
    effective_d8_flags = [
        f for f in raw_d8_flags if _canonical_flag(f) not in _SECURITY_POC_FLAGS
    ]
    empirical_run: dict[str, Any] | None = None
  else:
    raw_d8_binary = str(repro.get("d8_binary") or "").strip()
    raw_poc_path = str(repro.get("poc_path") or "").strip()
    if not raw_d8_binary or not raw_poc_path:
      raise ValueError(
          '`reproduction.d8_binary` and `reproduction.poc_path` are required when `symptom != "not_reproduced"`.'
      )
    runner = EmpiricalPocRunner(timeout_sec=poc_timeout, repo_root=repo_root)
    # Guard against LLM confusion: even if `sandbox_mode=True` was specified,
    # first probe with `sandbox_mode=False` (`--run-as-security-poc`, which
    # does not expose the `Sandbox.*` memory corruption API). If the POC
    # already crashes without sandbox testing, treat it as a regular bug/vuln.
    regular_result = runner.run(
        raw_d8_binary, raw_poc_path, raw_d8_flags, sandbox_mode=False)
    if sandbox_mode and regular_result.reproduces_with_security_poc:
      sandbox_mode = False
      affects_oos = False
      emp_result = regular_result
    elif sandbox_mode:
      emp_result = runner.run(
          raw_d8_binary, raw_poc_path, raw_d8_flags, sandbox_mode=True)
    else:
      emp_result = regular_result
    empirical_run = emp_result.to_dict()
    reproduces_with_security_poc = emp_result.reproduces_with_security_poc
    enabled_by_default = emp_result.enabled_by_default
    effective_d8_flags = list(emp_result.effective_d8_flags)

  scope = ExecutionScope.REGULAR if not sandbox_mode else (
      ExecutionScope.SANDBOX_OUT_OF_SANDBOX
      if affects_oos else ExecutionScope.SANDBOX_IN_SANDBOX_ONLY)
  base_rule = SYMPTOM_RULES[(symptom, scope)]
  classification = base_rule.classification
  severity = base_rule.severity
  security_impact = base_rule.security_impact
  priority = base_rule.default_priority
  impact_reasons: list[str] = []
  current_issue = data.get("current_issue") or {}
  resolved_comp = resolve_component(
      data.get("proposed_component"), current_issue, repo_root=repo_root)
  if resolved_comp is None:
    raise ValueError("Invalid `proposed_component`.")
  component_path, component_id, component_already_set = resolved_comp
  hotlists_to_add = []
  hotlists_to_remove = []
  current_hotlists = {str(h) for h in current_issue.get("hotlists", [])}
  has_release_block = bool(current_issue.get("has_release_block", False))
  unsupported_port = bool(repro.get("unsupported_port", False))

  if symptom != Symptom.NOT_REPRODUCED:
    if _has_enable_inspector(effective_d8_flags):
      classification = Classification.BUG
      severity = None
      security_impact = SecurityImpact.NONE
      priority = "P2"
      reproduces_with_security_poc = False
      enabled_by_default = False
      effective_d8_flags = [
          f for f in effective_d8_flags
          if _canonical_flag(f) not in _SECURITY_POC_FLAGS
      ]
      if component_id != DEVTOOLS_COMPONENT_ID:
        component_path = DEVTOOLS_COMPONENT_PATH
        component_id = DEVTOOLS_COMPONENT_ID
        component_already_set = False
      impact_reasons.append(
          "requires `--enable-inspector` (not representative of production DevTools; use `inspector-test`)"
      )
    elif not reproduces_with_security_poc:
      if sandbox_mode:
        classification = Classification.INTENDED_BEHAVIOR
        severity = None
        security_impact = SecurityImpact.NONE
        priority = "P3"
        impact_reasons.append(
            "did not reproduce with `--run-as-sandbox-security-poc`")
      else:
        if classification == Classification.VULNERABILITY:
          classification = Classification.BUG
          priority = "P2"
        else:
          impact_reasons.append(base_rule.default_impact_reason)
        severity = None
        security_impact = SecurityImpact.NONE
        impact_reasons.append(
            "did not reproduce with `--run-as-security-poc`; requires experimental, developer-only, unsafe, or d8-only configuration"
        )
    elif classification == Classification.VULNERABILITY:
      if not enabled_by_default:
        security_impact = SecurityImpact.NONE
        priority = "P2" if severity == "S1" else "P3"
        impact_reasons.append("requires feature flag not enabled by default")
      if unsupported_port:
        security_impact = SecurityImpact.NONE
        priority = "P2" if severity == "S1" else "P3"
        impact_reasons.append("targets unsupported Chromium port/project")
        if HOTLIST_V8_UNSUPPORTED_CHROME not in current_hotlists:
          hotlists_to_add.append(
              (HOTLIST_V8_UNSUPPORTED_CHROME, "v8-unsupported-chrome"))

  if sandbox_mode and classification == Classification.VULNERABILITY and HOTLIST_V8_SANDBOX not in current_hotlists:
    hotlists_to_add.append((HOTLIST_V8_SANDBOX, "v8-sandbox"))
  if not impact_reasons:
    impact_reasons.append(base_rule.default_impact_reason)

  clear_release_block = False
  if security_impact == SecurityImpact.NONE and symptom != Symptom.NOT_REPRODUCED:
    if HOTLIST_SECURITY_IMPACT_NONE not in current_hotlists:
      hotlists_to_add.append(
          (HOTLIST_SECURITY_IMPACT_NONE, "Security_Impact-None"))
    for hl_id in RELEASE_CHANNEL_IMPACT_HOTLISTS:
      if not current_hotlists or hl_id in current_hotlists:
        hotlists_to_remove.append((hl_id, f"Security_Impact channel ({hl_id})"))
    if has_release_block:
      clear_release_block = True
  elif security_impact == SecurityImpact.YES and HOTLIST_SECURITY_IMPACT_NONE in current_hotlists:
    hotlists_to_remove.append(
        (HOTLIST_SECURITY_IMPACT_NONE, "Security_Impact-None"))

  return TriageDecision(
      classification=classification,
      security_impact=security_impact,
      security_impact_rationale="; ".join(impact_reasons),
      severity=severity,
      priority=priority,
      component_path=component_path,
      component_id=component_id,
      component_already_set=component_already_set,
      execution_scope=scope,
      reproduces_with_security_poc=reproduces_with_security_poc,
      enabled_by_default=enabled_by_default,
      effective_d8_flags=effective_d8_flags,
      empirical_run=empirical_run,
      hotlists_to_add=hotlists_to_add,
      hotlists_to_remove=hotlists_to_remove,
      clear_release_block=clear_release_block,
  )


def render_triage_report(
    data: dict[str, Any],
    decision: TriageDecision,
    git_resolver: GitVersionResolver,
    env_session_id: str | None = None,
) -> str:
  """Renders the Buganizer comment and the `(User Only)` action plan."""
  session_id = _resolve_session_id(data, env_session_id)
  repro = data["reproduction"]
  symptom = Symptom(data["symptom"])
  reproduced = symptom != Symptom.NOT_REPRODUCED
  classification_line = f"- **Classification**: `{decision.classification.value}`"
  impact_line = f"- **Security Impact**: **{decision.security_impact.value}** ({decision.security_impact_rationale})"
  current_issue = data.get("current_issue") or {}
  cur_prio = current_issue.get("priority")
  cur_sev = current_issue.get("severity")
  if decision.severity:
    sev_part = f"`{decision.severity}`" + (f" (adjust from `{cur_sev}`)"
                                           if cur_sev and
                                           cur_sev != decision.severity else "")
    prio_part = f"Priority: `{decision.priority}`" + (
        f", adjust from `{cur_prio}`"
        if cur_prio and cur_prio != decision.priority else "")
    severity_line = f"- **Proposed Severity**: {sev_part} ({prio_part})"
  else:
    prio_part = f"Priority: `{decision.priority}`"
    if cur_prio and cur_prio != decision.priority:
      prio_part += f", {'lower from' if str(cur_prio) < str(decision.priority) else 'adjust from'} `{cur_prio}`"
    severity_line = f"- **Proposed Severity**: None — `{decision.classification.value}` ({prio_part})"

  intro_raw = str(data.get("introduced_in_commit") or "").strip()
  fixed_raw = str(data.get("fixed_in_commit") or "").strip()
  has_intro = bool(intro_raw) and intro_raw.lower() not in ("unknown", "none",
                                                            "n/a")
  has_fixed = bool(fixed_raw) and fixed_raw.lower() not in ("unknown", "none",
                                                            "n/a")
  duplicate_of = git_resolver.resolve_duplicate_of(data)
  intro_str = git_resolver.format_change_commit(intro_raw) if has_intro else (
      "N/A (not reproduced)" if not reproduced else "Unknown")
  if has_fixed:
    intro_str += f"; fixed in {git_resolver.format_change_commit(fixed_raw)}"
    if duplicate_of and f"b/{duplicate_of}" not in intro_str:
      intro_str += f" (duplicate of b/{duplicate_of})"
  elif duplicate_of:
    intro_str += f" (duplicate of b/{duplicate_of})"
  introduced_line = f"- **Introduced In**: {intro_str}"
  rationale_line = f"- **Rationale**: {data['rationale'].strip()}"

  poc_basename = os.path.basename(str(repro["poc_path"]).strip())
  d8_flags = decision.effective_d8_flags
  repro_cmd = " ".join(["d8", *d8_flags, poc_basename])
  status_str = "Reproduced" if reproduced else "Not Reproduced"
  build_str = git_resolver.format_build_version(
      repro["build_variants"],
      repro.get("build_commit") or "HEAD")
  verified_impact_str = repro.get(
      "verified_impact",
      "").strip() if reproduced else "None (could not reproduce)."
  raw_bt = repro.get("gdb_backtrace") or ("N/A" if not reproduced else "")
  bt_lines = [
      l.rstrip()
      for l in (
          raw_bt if isinstance(raw_bt, list) else str(raw_bt).splitlines())
      if l.strip()
  ]
  if len(bt_lines) <= 1:
    gdb_sub_bullet = f"    - `GDB Backtrace`: {f'`{bt_lines[0]}`' if bt_lines else 'N/A'}"
  else:
    indented_bt = "\n".join(f"      {line}" for line in bt_lines)
    gdb_sub_bullet = f"    - `GDB Backtrace`:\n      ```text\n{indented_bt}\n      ```"

  finding_lines = [
      "- **Local Reproduction Findings**:",
      f"    - `Status`: `{status_str}`",
      f"    - `Reproduction`: `{repro_cmd}`",
      f"    - `Result`: {repro['result'].strip()}",
      f"    - `Build`: {build_str}",
      f"    - `Verified Impact`: {verified_impact_str}",
  ]
  if decision.empirical_run and decision.empirical_run.get("summary"):
    finding_lines.append(
        f"    - `Security POC Check`: {decision.empirical_run['summary']}")
  finding_lines.append(gdb_sub_bullet)

  owner_email, owner_reason = git_resolver.resolve_proposed_owner(data) or (
      "unknown@chromium.org",
      "subsystem maintainer",
  )
  owner_line = f"- **Proposed Owner**: `{owner_email}` ({owner_reason})"
  already_suffix = ", already set" if decision.component_already_set else ""
  if decision.component_path:
    component_line = f"- **Proposed Component**: `{decision.component_path}` (ID: `{decision.component_id}`{already_suffix})"
    move_target = f"`{decision.component_path}` (`{decision.component_id}`)"
  else:
    already_paren = " (already set)" if decision.component_already_set else ""
    component_line = f"- **Proposed Component**: ID `{decision.component_id}`{already_paren}"
    move_target = f"`{decision.component_id}`"
  prop_title = str(data["proposed_title"]).strip()
  cur_title = _resolve_current_title(data)
  title_str = f"Keep current (`{cur_title}`)" if cur_title and (
      prop_title == cur_title or
      prop_title.lower() == "keep current") else prop_title
  title_line = f"- **Proposed Title**: {title_str}"
  header = f"This analysis is AI-generated using the `v8-deterministic-security-triage` skill (Conversation ID: `{session_id}`)."
  buganizer_comment = "\n\n".join([
      header,
      classification_line,
      impact_line,
      severity_line,
      introduced_line,
      rationale_line,
      "\n".join(finding_lines),
      owner_line,
      component_line,
      title_line,
  ])

  user_only_lines = ["--- (User Only: Do Not Post to Buganizer) ---"]
  if decision.empirical_run:
    er = decision.empirical_run
    user_only_lines.extend([
        "",
        "### Empirical Security POC Verification (User Only)",
        f"- Matched Stage: `{er.get('matched_stage')}`",
        f"- `reproduces_with_security_poc`: `{er.get('reproduces_with_security_poc')}`",
        f"- `enabled_by_default`: `{er.get('enabled_by_default')}`",
        f"- Summary: {er.get('summary')}",
    ])
    for st in er.get("stages", []):
      meta = [f"returncode={st.get('returncode')}"
             ] + (["timed_out=True"] if st.get("timed_out") else [])
      user_only_lines.append(
          f"- `{st.get('stage')}`: `{' '.join(st.get('command', []))}` -> `reproduced={st.get('reproduced')}` ({', '.join(meta)})"
      )

  is_open_type = decision.classification in (Classification.VULNERABILITY,
                                             Classification.BUG)
  user_only_lines.extend([
      "",
      "### Buganizer MCP Action Plan (Execute Only After User Approval)",
      "- **Step 1 (First)**: `add_buganizer_comment`: Post the Buganizer Analysis comment above first.",
  ])
  step_idx = 2
  if duplicate_of:
    if not decision.component_already_set:
      user_only_lines.append(
          f"- **Step {step_idx}**: `safe_move_issue`: Move to component {move_target} (Note: If `safe_move_issue` fails with `FAILED_PRECONDITION` due to an unresolvable principal on the source component, skip and let the user move the component in the Buganizer UI)"
      )
      step_idx += 1
    if cur_title and prop_title and prop_title != cur_title and prop_title.lower(
    ) != "keep current":
      user_only_lines.append(
          f"- **Step {step_idx}**: `edit_title`: `{prop_title}`")
      step_idx += 1
    user_only_lines.append(
        f'- **Step {step_idx}**: `mark_issue_as_duplicate`: Mark as duplicate of `b/{duplicate_of}` (`targetIssueId="{duplicate_of}"`)'
    )
  else:
    type_action = f"- **Step {step_idx}**: `update_issue_type`: `{decision.classification.value}`" if is_open_type else f"- **Step {step_idx}**: Issue resolution: `{decision.classification.value}`"
    user_only_lines.append(type_action)
    step_idx += 1
    if decision.severity:
      user_only_lines.append(
          f"- **Step {step_idx}**: `update_issue_severity`: `{decision.severity}`"
      )
      step_idx += 1
    user_only_lines.append(
        f"- **Step {step_idx}**: `update_issue_priority`: `{decision.priority}`"
    )
    step_idx += 1
    for hl_id, hl_desc in decision.hotlists_to_add:
      user_only_lines.append(
          f"- **Step {step_idx}**: `add_issue_to_hotlist`: `{hl_id}` ({hl_desc})"
      )
      step_idx += 1
    for hl_id, hl_desc in decision.hotlists_to_remove:
      user_only_lines.append(
          f"- **Step {step_idx}**: `remove_issue_from_hotlist`: `{hl_id}` ({hl_desc})"
      )
      step_idx += 1
    if decision.clear_release_block:
      user_only_lines.append(
          f'- **Step {step_idx}**: `set_issue_custom_field`: `customFieldId="{CUSTOM_FIELD_RELEASE_BLOCK}"`, `customFieldValue=""` (clear ReleaseBlock)'
      )
      step_idx += 1
    if not decision.component_already_set:
      user_only_lines.append(
          f"- **Step {step_idx}**: `safe_move_issue`: Move to component {move_target} (Note: If `safe_move_issue` fails with `FAILED_PRECONDITION` due to an unresolvable principal on the source component, skip and let the user move the component in the Buganizer UI)"
      )
      step_idx += 1
    if cur_title and prop_title and prop_title != cur_title and prop_title.lower(
    ) != "keep current":
      user_only_lines.append(
          f"- **Step {step_idx}**: `edit_title`: `{prop_title}`")
      step_idx += 1
    user_only_lines.append(
        f"- **Step {step_idx}**: `safe_reassign_issue`: Reassign to `{owner_email}` (Note: On restricted-access issues, add current assignee as collaborator via `safe_add_collaborator` first)"
    )
  return buganizer_comment + "\n\n" + "\n".join(user_only_lines) + "\n"


# ==============================================================================
# 6. SELF-DOCUMENTING CLI HELP (--help)
# ==============================================================================

JSON_TEMPLATE = """\
{
  // High-level empirical symptom ("oob_write", "uaf", "type_confusion", "oob_read", "uninitialized_read",
  // "dcheck_corrupts_state", "dcheck_benign", "verification_check_failure", "production_check_or_trap", "nullptr_deref", "hole_leak", "not_reproduced")
  "symptom": "oob_write",
  "sandbox_mode": false,
  // Required only when sandbox_mode is true
  "affects_out_of_sandbox_state": false,
  "reproduction": {
    "d8_binary": "out/x64.release/d8",
    "poc_path": "tmp/poc.js",
    "d8_flags": [],
    "unsupported_port": false,
    "build_variants": ["x64.release", "x64.asan"],
    "build_commit": "HEAD",
    "result": "SIGSEGV writing out-of-bounds to JSArray elements backing store.",
    "verified_impact": "Out-of-bounds write of 8-byte attacker-controlled value in the V8 heap.",
    "gdb_backtrace": [
      "#0  0x0000555556789abc in Builtins_Maglev...",
      "#1  0x0000555556123456 in v8::internal::Execution::Call..."
    ]
  },
  "introduced_in_commit": "HEAD",
  "fixed_in_commit": null,
  // Optional: auto-derived from fixed_in_commit's Fixed:/Bug: trailer when omitted
  "duplicate_of": null,
  "rationale": "Maglev bounds check elimination elides the check across a side-effecting call. An attacker can shrink the array inside the callback to trigger an out-of-bounds write in production release builds.",
  // Optional when introduced_in_commit or fixed_in_commit author is in git; otherwise pass email string or {"email": "...", "reason": "..."}
  "proposed_owner": {"email": "owner@chromium.org", "reason": "author of introducing commit and subsystem maintainer"},
  // Component ID (e.g. "1456177"), V8 source path with DIR_METADATA (e.g. "src/maglev"), or {"path": "...", "id": "..."}
  "proposed_component": "src/maglev",
  "proposed_title": "[maglev] Preserve bounds check across side-effecting call",
  "current_issue": {
    "id": "123456789",
    "title": "Security: crash in d8",
    "component_id": "1456800",
    "priority": "P1",
    "severity": "S1",
    "hotlists": ["5432216"],
    "has_release_block": true
  }
}
"""


def format_help_epilog() -> str:
  """Generates the --help epilog with empirical guides and JSON schema."""
  return "\n".join([
      "Empirical Symptom Guide (`symptom`):",
      "  Select the single empirical symptom observed in GDB / crash output:",
      *[f'  - "{s.value}": {SYMPTOM_EMPIRICAL_GUIDE[s]}' for s in Symptom],
      "",
      "Empirical Sandbox Scope Guide (`sandbox_mode` & `affects_out_of_sandbox_state`):",
      "  - `sandbox_mode`:",
      "    - `false`: Standard V8 execution. The POC does NOT start from simulated in-sandbox memory corruption (`--sandbox-testing`, `--run-as-sandbox-security-poc`, `--expose-memory-corruption-api`, or the `Sandbox.` JavaScript API).",
      "    - `true`: V8 Sandbox testing mode. The POC starts from simulated in-sandbox memory corruption (`--sandbox-testing`, `--run-as-sandbox-security-poc`, `--expose-memory-corruption-api`, or `Sandbox.*` APIs).",
      '  - `affects_out_of_sandbox_state` (required when `sandbox_mode` is `true` and `symptom != "not_reproduced"`):',
      '    - `false`: GDB inspection shows the fault, assertion, or memory access affects only in-sandbox memory (inside the V8 Sandbox address space) or internal sandboxed V8 heap objects (including crashes caught by `--sandbox-testing` as `"Caught harmless memory access violation"` or `"Safely terminating process"`, linear OOB writes hitting sandbox guard regions, in-sandbox nullptr dereferences, or overwriting trusted objects with well-formed data of the same type without escaping the sandbox).',
      '    - `true`: GDB inspection shows execution corrupts or writes to memory or state outside the V8 Sandbox address space (e.g., TrustedSpace, ExternalPointerTable entries, C++/Blink heap, stack, or host process registers/addresses) or triggers `"V8 sandbox violation detected"`.',
      "",
      "Empirical `reproduction.d8_binary` Selection Guide:",
      "  - Pass the `d8` binary that deterministically reproduces the crash under `--run-as-security-poc` (or `--run-as-sandbox-security-poc`):",
      '    - Standard crash / corruption: `"out/x64.release/d8"`',
      '    - `symptom` is `"dcheck_corrupts_state"` or `"dcheck_benign"` and the unmodified POC only crashes a dcheck-enabled build: `"out/x64.debug/d8"` (per `docs/security/reproducing-bugs.md`, reproducing a `DCHECK` failure in a debugging build with `--run-as-security-poc` is sufficient).',
      '    - Silent out-of-bounds access or use-after-free that only crashes with sanitizer instrumentation: `"out/x64.asan/d8"` (while listing all verified build configurations in `reproduction.build_variants`).',
      "",
      "Dynamic `proposed_component` Resolution (`DIR_METADATA` / Component ID):",
      '  - Pass a V8 source path containing `DIR_METADATA` (e.g. `"src/maglev"`), a numeric Buganizer component ID (e.g. `"1456177"`), or `{"path": "...", "id": "..."}` (from `DIR_METADATA` or Buganizer MCP `list_components`).',
      '  - `triage_report.py` dynamically scans `DIR_METADATA` in the V8 repository to resolve component paths/IDs and computes `already_set` from `current_issue.component_id`.',
      "",
      "JSON Input Schema & Template (`tmp/triage_input.json`):",
      JSON_TEMPLATE.rstrip(),
  ])


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(
      description="Deterministically classify, validate, and render a V8 security triage report from a JSON evidence file.",
      formatter_class=argparse.RawDescriptionHelpFormatter,
      epilog=format_help_epilog(),
  )
  parser.add_argument(
      "input_json", help="Path to the JSON evidence file (or '-' for stdin).")
  parser.add_argument(
      "--poc-timeout",
      type=float,
      default=DEFAULT_POC_TIMEOUT_SEC,
      help=f"Timeout in seconds per d8 execution stage (default: {DEFAULT_POC_TIMEOUT_SEC}s)."
  )
  parser.add_argument(
      "--repo-root",
      default=None,
      help="Path to the V8 repository root (defaults to auto-detected root).")
  args = parser.parse_args(argv)

  try:
    raw_text = sys.stdin.read() if args.input_json == "-" else Path(
        args.input_json).read_text(encoding="utf-8")
    data = load_lenient_json(raw_text)
  except Exception as exc:  # pylint: disable=broad-except
    print(f"JSON PARSE ERROR: {exc}", file=sys.stderr)
    return 1

  repo_root = Path(args.repo_root).resolve(
  ) if args.repo_root else _detect_default_repo_root()
  env_session_id = _detect_env_session_id()
  errors = validate_triage_input(
      data, env_session_id=env_session_id, repo_root=repo_root)
  if errors:
    print("VALIDATION FAILED:", file=sys.stderr)
    for err in errors:
      print(f"  - {err}", file=sys.stderr)
    return 1

  decision = compute_triage_decision(
      data, repo_root=repo_root, poc_timeout=args.poc_timeout)
  git_resolver = GitVersionResolver(repo_root)
  print(
      render_triage_report(
          data, decision, git_resolver, env_session_id=env_session_id),
      end="",
  )
  return 0


if __name__ == "__main__":
  sys.exit(main())
