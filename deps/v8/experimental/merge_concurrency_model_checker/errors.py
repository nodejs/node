# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Explicit exception hierarchy and step definitions for the model checker.

All model-checking invariants and C++ DCHECK/CHECK failures raise explicit
subclasses of ``ModelCheckFailure`` instead of using Python ``assert``, so
running under ``python3 -O`` never disables verification.
"""

from enum import Enum
from typing import Tuple


class MergeModelStep(str, Enum):
  # Background merge phase steps (BeginMergeInBackground)
  BG_INIT_TOPLEVEL = "BG_INIT_TOPLEVEL"
  BG_INSPECT_SLOT = "BG_INSPECT_SLOT"
  BG_PAIR_SFI = "BG_PAIR_SFI"
  BG_RECORD_COMPILED_DATA = "BG_RECORD_COMPILED_DATA"
  BG_USE_NEW_SFI = "BG_USE_NEW_SFI"
  BG_STANDALONE_SCOPE = "BG_STANDALONE_SCOPE"
  BG_RECORD_OLD_SCOPE_INFO = "BG_RECORD_OLD_SCOPE_INFO"
  BG_FORWARD_POINTERS = "BG_FORWARD_POINTERS"

  # Foreground merge phase steps (CompleteMergeInForeground)
  FG_RESCAN_START = "FG_RESCAN_START"
  FG_DETECT_CONCURRENT_SFI = "FG_DETECT_CONCURRENT_SFI"
  FG_FORWARD_POINTERS = "FG_FORWARD_POINTERS"
  FG_RELEASE_BACKWARDS_START = "FG_RELEASE_BACKWARDS_START"
  FG_COPY_FROM = "FG_COPY_FROM"
  FG_PUBLISH_NEW_SFI = "FG_PUBLISH_NEW_SFI"


EXPECTED_BG_STEPS: Tuple[str, ...] = (
    MergeModelStep.BG_INIT_TOPLEVEL.value,
    MergeModelStep.BG_INSPECT_SLOT.value,
    MergeModelStep.BG_PAIR_SFI.value,
    MergeModelStep.BG_RECORD_COMPILED_DATA.value,
    MergeModelStep.BG_USE_NEW_SFI.value,
    MergeModelStep.BG_STANDALONE_SCOPE.value,
    MergeModelStep.BG_RECORD_OLD_SCOPE_INFO.value,
    MergeModelStep.BG_FORWARD_POINTERS.value,
)

EXPECTED_FG_STEPS: Tuple[str, ...] = (
    MergeModelStep.FG_RESCAN_START.value,
    MergeModelStep.FG_DETECT_CONCURRENT_SFI.value,
    MergeModelStep.FG_FORWARD_POINTERS.value,
    MergeModelStep.FG_RELEASE_BACKWARDS_START.value,
    MergeModelStep.FG_COPY_FROM.value,
    MergeModelStep.FG_PUBLISH_NEW_SFI.value,
)


class ModelCheckFailure(Exception):
  """Base class for genuine C++ concurrency, GC, memory-model, or invariant failures."""


class CxxDcheckFailure(ModelCheckFailure):
  """Raised when an interpreted C++ DCHECK/CHECK/SBXCHECK/UNREACHABLE fails."""


class DuplicateScopeInfoError(ModelCheckFailure):
  """Raised when VerifyCodeMerge detects duplicate ScopeInfo objects for one unique_id."""


class ConcurrentReaderError(ModelCheckFailure):
  """Raised when a concurrent background reader observes an inconsistent SFI/CP state."""


class UnsafePublicationError(ModelCheckFailure):
  """Raised when a thread dereferences/casts an object published without release/acquire."""


class DataRaceError(ModelCheckFailure):
  """Raised on a C++11 non-atomic data race (concurrent plain read/write without happens-before)."""


class DanglingHandleError(ModelCheckFailure):
  """Raised when dereferencing a Handle whose HandleScope exited or a null Handle."""


class RawPointerAcrossSafepointError(ModelCheckFailure):
  """Raised when a raw Tagged<> local pointer is dereferenced after a moving GC safepoint."""


class GcInNoGcScopeError(ModelCheckFailure):
  """Raised when a GC/safepoint executes while DisallowGarbageCollection depth > 0."""


class InvariantViolationError(ModelCheckFailure):
  """Raised when a merge postcondition (state machine, return handle, bytecode retention) fails."""


class UnsupportedCxxAstError(RuntimeError):
  """Fail-closed error raised when Clang AST contains an unsupported C++ construct."""


class ConcurrencySyncMismatchError(RuntimeError):
  """Raised when MERGE_MODEL_STEP markers do not match detected concurrency hazards or order."""


class CxxHarnessError(RuntimeError):
  """Raised for internal interpreter/harness bugs (never counted as a caught fault)."""


def check_condition(cond: bool,
                    msg: str,
                    exc_cls: type = InvariantViolationError) -> None:
  """Always-on check that cannot be stripped by ``python3 -O``."""
  if not cond:
    raise exc_cls(msg)
