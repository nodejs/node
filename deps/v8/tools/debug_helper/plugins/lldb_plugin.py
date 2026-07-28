# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""LLDB integration for the V8 debugger bridge."""

import os
import shlex
import struct
import traceback

import lldb

from v8dbg.commands import debug_helper_lib_warning
from v8dbg.commands import dispatch_v8_command
from v8dbg.shared_bridge import get_bridge

_VERBOSE = os.environ.get("V8_DEBUG_HELPER_VERBOSE", "") != ""


def _log(message):
  if _VERBOSE:
    print(f"v8dbg: {message}")


_DEFAULT_FRAME_FORMAT = ("frame #${frame.index}:{ ${frame.no-debug}${frame.pc}}"
                         "{ ${module.file.basename}{`${function.name-with-args}"
                         "{${frame.no-debug}${function.pc-offset}}}}"
                         "{ at ${line.file.basename}:${line.number}}"
                         "{${function.is-optimized} [opt]}")


def _make_read_memory(process):
  """Return a memory-read callback bound to one lldb process."""

  def read_memory(address, byte_count):
    error = lldb.SBError()
    data = process.ReadMemory(address, byte_count, error)
    if not error.Success():
      raise RuntimeError(error.GetCString() or "unable to read memory")
    return data

  return read_memory


class _LldbResolver:
  """Resolves symbols, memory, and type offsets through LLDB."""

  def __init__(self, target, process):
    self._target = target
    self._process = process
    self._ptr_size = target.GetAddressByteSize() if target else 8

  def read_pointer(self, addr):
    data = self._read_bytes(addr, self._ptr_size)
    if data is None:
      return None
    return int.from_bytes(data, "little")

  def _read_bytes(self, addr, byte_count):
    if not self._process:
      return None
    error = lldb.SBError()
    data = self._process.ReadMemory(addr, byte_count, error)
    if not error.Success() or not data or len(data) != byte_count:
      return None
    return data

  def debug_symbol_address(self, name):
    """Resolve the address of the variable `name` using debug info.

    For a TLS variable this is the address of the selected thread's copy.
    """
    if not self._target:
      return None
    # Scope the lookup to a frame so that TLS variables resolve against the
    # selected thread.
    frame = self._selected_frame()
    if frame is None:
      return None
    try:
      value = frame.GetValueForVariablePath(name)
    except Exception:
      return None
    if not value or not value.IsValid():
      return None
    error = value.GetError()
    if error and not error.Success():
      return None
    load_addr = value.GetLoadAddress()
    if load_addr == lldb.LLDB_INVALID_ADDRESS:
      return None
    return int(load_addr)

  def symbol_address(self, name):
    """Resolve the address of the variable `name` for the selected thread.

    For a TLS variable this is the address of the selected thread's copy.
    """
    addr = self.debug_symbol_address(name)
    if addr is not None:
      return addr
    if not self._target:
      return None

    # Without debug info, fall back to the minimal symbol tables.
    try:
      contexts = self._target.FindSymbols(name)
    except Exception:
      if _VERBOSE:
        traceback.print_exc()
      return None

    if not contexts:
      _log(f"no symbol table entry for {name}")
      return None

    for context in contexts:
      symbol = context.GetSymbol()
      if not symbol or not symbol.IsValid():
        continue
      start = symbol.GetStartAddress()
      section = start.GetSection()
      section_name = section.GetName() if section and section.IsValid() else ""

      if section_name in (".tdata", ".tbss"):
        addr = self._elf_tls_symbol_address(context.GetModule(),
                                            start.GetFileAddress())
        if addr:
          return addr
        _log(f"cannot resolve the ELF TLS address of {name}")
        continue
      if section_name == "__thread_vars":
        addr = self._macho_tls_symbol_address(
            context.GetModule(), start.GetLoadAddress(self._target))
        if addr:
          return addr
        _log(f"cannot resolve the Mach-O TLS address of {name}")
        continue

      # Not a TLS symbol, the load address is the variable's address.
      load_addr = start.GetLoadAddress(self._target)
      if load_addr != lldb.LLDB_INVALID_ADDRESS:
        return int(load_addr)

    _log(f"cannot resolve the address of {name}")
    return None

  def _selected_frame(self):
    if not self._process:
      return None
    thread = self._process.GetSelectedThread()
    frame = thread.GetSelectedFrame() if thread else None
    if not frame or not frame.IsValid():
      return None
    return frame

  def _symbol_load_address(self, name, module_name=None):
    # Scope the lookup to `module_name` when given to avoid conflicts.
    source = self._target
    if module_name:
      try:
        module = self._target.FindModule(lldb.SBFileSpec(module_name))
      except Exception:
        module = None
      if module and module.IsValid():
        source = module
    try:
      contexts = source.FindSymbols(name)
    except Exception:
      return None
    if not contexts:
      return None
    start = contexts[0].GetSymbol().GetStartAddress()
    addr = start.GetLoadAddress(self._target)
    return None if addr == lldb.LLDB_INVALID_ADDRESS else int(addr)

  def _elf_tls_symbol_address(self, module, tls_offset):
    """Compute the address of an ELF TLS symbol in the selected thread.

    lldb does not resolve TLS minimal symbols itself, so derive the address
    from the thread pointer register and the module's TLS segment layout.
    This currently only works with the local-exec model on Linux x86_64 and
    arm64.
    """
    triple = (self._target.GetTriple() or "") if self._target else ""
    if "linux" not in triple:
      _log(f"ELF TLS symbols are only resolved on Linux, triple is {triple}")
      return None

    # A shared library's TLS block is at a dynamic offset, skip it
    # to avoid computing a plausible-looking wrong address.
    # TODO(joyee): if we want to support libv8.so, we need to walk the DTV.
    exe_module = self._target.FindModule(self._target.GetExecutable())
    if not exe_module or not exe_module.IsValid() or module != exe_module:
      _log("TLS symbol is not in the main executable, skipping")
      return None
    frame = self._selected_frame()
    if frame is None:
      _log("no selected frame to read the thread pointer from")
      return None

    # Reconstruct the PT_TLS segment size and alignment from the sections.
    sections = [module.FindSection(name) for name in (".tdata", ".tbss")]
    sections = [s for s in sections if s and s.IsValid()]
    if not sections:
      _log("the main executable has no .tdata/.tbss sections")
      return None
    segment_start = min(s.GetFileAddress() for s in sections)
    segment_end = max(s.GetFileAddress() + s.GetByteSize() for s in sections)
    segment_size = segment_end - segment_start

    # TLS symbol values are normally plain offsets within the TLS segment,
    # but on some targets they are virtual addresses inside PT_TLS, so
    # normalize them to offsets. See
    # https://github.com/llvm/llvm-project/blob/9d2000c24/lldb/source/Plugins/DynamicLoader/POSIX-DYLD/DynamicLoaderPOSIXDYLD.cpp
    if segment_start <= tls_offset < segment_end:
      tls_offset -= segment_start

    align = 1
    for section in sections:
      try:
        align = max(align, int(section.GetAlignment()))
      except Exception:
        pass

    def align_up(value, alignment):
      return (value + alignment - 1) & ~(alignment - 1)

    if triple.startswith("x86_64"):
      thread_pointer = self._read_register(frame, ("fs_base", "fsbase"))
      if not thread_pointer:
        _log("cannot read the fs_base register")
        return None
      # For x86-64 TLS, the main module's TLS block sits immediately
      # below the thread pointer.
      return thread_pointer - align_up(segment_size, align) + tls_offset

    if triple.startswith("aarch64"):
      thread_pointer = self._read_register(frame, ("tpidr", "tpidr_el0"))
      if not thread_pointer:
        _log("cannot read the tpidr register")
        return None
      # For AArch64 TLS, the main module's TLS block sits above the
      # 16-byte TCB the thread pointer points to.
      return thread_pointer + align_up(16, align) + tls_offset
    _log(f"unsupported architecture for TLS resolution: {triple}")
    return None

  # Scan window when calibrating field offsets inside `struct pthread`.
  _PTHREAD_SCAN_BYTES = 0x400
  # Backstop when walking the pthread list, in case of corrupted links.
  _MAX_PTHREADS = 4096

  def _macho_tls_symbol_address(self, module, descriptor_addr):
    """Compute the address of a Mach-O TLS symbol in the selected thread.

    lldb does not resolve TLS minimal symbols itself, so derive the address
    from the TSD area scanned from libpthread's global thread list and
    the TLV descriptor. In a live process, fall back to calling thunk.
    """
    if descriptor_addr == lldb.LLDB_INVALID_ADDRESS:
      return None
    raw = self._read_bytes(descriptor_addr, 24)
    if raw is not None:
      # Try parsing the TLV descriptor.
      # See https://github.com/apple-oss-distributions/dyld/blob/e9da5ae/libdyld/ThreadLocalVariables.h
      #
      # The high dword of a legacy offset is zero for any real offset while
      # initialContentSize is always nonzero, which disambiguates the two.
      size = struct.unpack_from("<L", raw, 20)[0]
      if size != 0:
        key, offset = struct.unpack_from("<LL", raw, 8)
      else:
        _, key, offset = struct.unpack("<QQQ", raw)
      # 768 is the likely maximum TSD slot count, see
      # https://github.com/apple-oss-distributions/libpthread/blob/42d026d/src/types_internal.h
      # Key 0 is pthread_self and can't be assigned to a TLV.
      # An implausible key or offset means we likely failed to decode the
      # descriptor, in which case we fall back to calling thunk.
      if not 0 < key < 768 or (size and offset >= size):
        return self._macho_tls_from_thunk_call(descriptor_addr)

      tsd = self._macho_selected_pthread_tsd()
      block = self.read_pointer(tsd + key * 8) if tsd is not None else None
      if block:
        return block + offset

      # The thread never touched this image's TLS, so the variable still has
      # its initial value. In this case, point at the image's TLS template.
      if block == 0 and module and module.IsValid():
        # The blocks are instantiated from a template laid out as
        # `[__thread_data][__thread_bss]`, both mapped with the image.
        addresses = [
            subsection.GetLoadAddress(self._target)
            for section in module.sections
            for subsection in section
            if subsection.GetName() in ("__thread_data", "__thread_bss")
        ]
        base = min((a for a in addresses if a != lldb.LLDB_INVALID_ADDRESS),
                   default=None)
        if base is not None:
          return base + offset

    # Fall back to calling the thunk in the inferior, which is only safe
    # in a live process.
    return self._macho_tls_from_thunk_call(descriptor_addr)

  def _macho_selected_pthread_tsd(self):
    """Find the selected thread's TSD area by scanning libpthread's global
    thread list.
    """
    # https://github.com/apple-oss-distributions/libpthread/blob/42d026d/src/types_internal.h
    # https://github.com/apple-oss-distributions/libpthread/blob/42d026d/src/pthread.c
    if not self._process:
      return None
    thread = self._process.GetSelectedThread()
    if not thread or not thread.IsValid():
      return None
    tid = thread.GetThreadID()
    head = self._symbol_load_address("__pthread_head",
                                     "libsystem_pthread.dylib")
    if head is None:
      return None
    first = self.read_pointer(head)
    if not first:
      return None
    word_count = self._PTHREAD_SCAN_BYTES // 8

    def read_words(addr):
      raw = self._read_bytes(addr, self._PTHREAD_SCAN_BYTES)
      return struct.unpack(f"<{word_count}Q", raw) if raw else None

    first_words = read_words(first)
    if first_words is None:
      return None
    # The first element's TAILQ prev pointer points back at the list head, so
    # scanning for it recovers the link field offset.
    prev_index = next(
        (i for i, word in enumerate(first_words) if i > 0 and word == head),
        None)
    if prev_index is None:
      return None
    link_index = prev_index - 1
    pthreads = [(first, first_words)]
    node = first_words[link_index]
    while node and len(pthreads) < self._MAX_PTHREADS:
      words = read_words(node)
      if words is None:
        # If the core is truncated, the list may be incomplete.
        break
      pthreads.append((node, words))
      node = words[link_index]

    # We want most structs to agree so that e.g. a coincidental value in a
    # struct caught mid-creation can be ignored.
    majority = max(1, len(pthreads) // 2)
    # Prefer the TSD offset from `pthread_layout_offsets` which contains
    # layout description for debuggers, see
    # https://github.com/llvm/llvm-project/blob/9d2000c24/lldb/source/Plugins/SystemRuntime/MacOSX/SystemRuntimeMacOSX.cpp
    tsd_index = None
    spi = self._symbol_load_address("pthread_layout_offsets",
                                    "libsystem_pthread.dylib")
    raw = self._read_bytes(spi, 8) if spi is not None else None
    if raw is not None:
      version, base, _, entry_size = struct.unpack("<HHHH", raw)
      if (version >= 1 and entry_size == 8 and base and base % 8 == 0 and
          base < self._PTHREAD_SCAN_BYTES):
        tsd_index = base // 8
    if tsd_index is None:
      # The TSD area starts with a pthread_self() slot. Other pthread_t
      # fields can also point to themselves in some structs, so take the
      # field where the most structs agree.
      counts = [
          sum(words[i] == pthread
              for pthread, words in pthreads)
          for i in range(word_count)
      ]
      tsd_index = max(range(word_count), key=counts.__getitem__)
      if counts[tsd_index] < majority:
        return None

    # The thread id field is where the list's values line up with the ids
    # lldb reports.
    all_tids = {t.GetThreadID() for t in self._process}
    for i in range(word_count):
      matches = [pthread for pthread, words in pthreads if words[i] in all_tids]
      if len(matches) < majority:
        continue
      selected = [pthread for pthread, words in pthreads if words[i] == tid]
      if len(selected) == 1:
        return selected[0] + tsd_index * 8
    return None

  def _macho_tls_from_thunk_call(self, descriptor_addr):
    """Call the descriptor's dyld thunk in the inferior, pinned to the selected
    thread so the result belongs to it rather than whichever thread lldb picks.
    This only works in live processes.
    """
    frame = self._selected_frame()
    if frame is None:
      return None
    if not self.read_pointer(descriptor_addr):
      return None
    # `thunk(&descriptor)` returns `&tls_storage` for the calling thread.
    expr = (f"(unsigned long long)((void*(*)(void*))(*(void**)"
            f"{descriptor_addr:#x}))((void*){descriptor_addr:#x})")
    options = lldb.SBExpressionOptions()
    options.SetTryAllThreads(False)
    options.SetStopOthers(True)
    options.SetIgnoreBreakpoints(True)
    options.SetUnwindOnError(True)
    try:
      value = frame.EvaluateExpression(expr, options)
    except Exception:
      return None
    if not value or not value.IsValid():
      return None
    error = value.GetError()
    if error and not error.Success():
      return None
    return int(value.GetValueAsUnsigned(0)) or None

  def _read_register(self, frame, names):
    for name in names:
      try:
        register = frame.FindRegister(name)
      except Exception:
        continue
      if register and register.IsValid():
        value = register.GetValueAsUnsigned(0)
        if value:
          return value
    return None

  def field_offset(self, type_name, field_name):
    """Return the byte offset of `field_name` inside `type_name`."""
    if not self._target:
      return None
    try:
      sb_type = self._target.FindFirstType(type_name)
    except Exception:
      return None
    if not sb_type or not sb_type.IsValid():
      return None
    for i in range(sb_type.GetNumberOfFields()):
      field = sb_type.GetFieldAtIndex(i)
      if field.GetName() == field_name:
        return int(field.GetOffsetInBytes())
    return None


def _evaluate_address_in_lldb(target, frame, text):
  """Evaluate a debugger expression as an integer address, or None on failure."""
  if frame and frame.IsValid():
    value = frame.EvaluateExpression(text)
  elif target and target.IsValid():
    value = target.EvaluateExpression(text)
  else:
    return None
  if value is None or not value.IsValid():
    return None
  err = value.GetError()
  if not err or not err.Success():
    return None
  # Sentinel distinguishes a real Smi 0 from a silent conversion failure.
  fail_value = (1 << 64) - 1
  try:
    raw = value.GetValueAsUnsigned(fail_value)
  except Exception:
    return None
  if raw == fail_value:
    return None
  return int(raw)


def frame_annotation(frame, _unused):
  """Return the V8 JS suffix for one LLDB frame, or an empty string."""
  try:
    function_name = frame.GetFunctionName()
    if not function_name:
      symbol = frame.GetSymbol()
      if symbol and symbol.IsValid():
        function_name = symbol.GetName() or ""
    if function_name and "Builtin" not in function_name:
      return ""

    process = frame.GetThread().GetProcess()
    target = process.GetTarget()
    read_memory = _make_read_memory(process)

    fp = frame.GetFP()
    if fp == lldb.LLDB_INVALID_ADDRESS:
      return ""
    bridge = get_bridge(target.GetAddressByteSize())
    return bridge.frame_suffix(fp, read_memory)
  except Exception:
    if _VERBOSE:
      traceback.print_exc()
    return ""


def cmd_v8(debugger, command, result, _internal_dict):
  """V8 debug helpers.
  Syntax: v8 <subcommand> [options]

  Available subcommands:

    - inspect -- Inspect the heap object at <addr> with the V8 inspector.

      v8 inspect <addr> [--type T] [--depth N] [--array-length N]

      * --type T: treat the object as if it has type T (e.g. `v8::internal::JSArray`).
      * --depth N: limit recursive inspection depth to N (default: 1).
      * --array-length N: for arrays, inspect up to N elements (default: 16).

    - isolate -- Print the current Isolate address of the selected thread.

      v8 isolate
  """
  try:
    argv = shlex.split(command or "")
  except ValueError:
    argv = (command or "").split()
  target = debugger.GetSelectedTarget()
  process = target.GetProcess() if target else None
  frame = None
  if process:
    thread = process.GetSelectedThread()
    if thread:
      frame = thread.GetSelectedFrame()

  if not process or not process.IsValid():
    result.SetError("v8: no running process / loaded core to inspect")
    return

  bridge = get_bridge(target.GetAddressByteSize())
  read_memory = _make_read_memory(process)

  def eval_address(text):
    return _evaluate_address_in_lldb(target, frame, text)

  ok, text = dispatch_v8_command(
      bridge,
      argv,
      read_memory=read_memory,
      eval_address=eval_address,
      resolver=_LldbResolver(target, process),
      verbose=_VERBOSE,
  )
  if not ok:
    result.SetError(text)
  elif text:
    result.AppendMessage(text.rstrip("\n"))


def __lldb_init_module(debugger, _internal_dict):
  """Hook the LLDB frame format and register the `v8` command."""
  warning = debug_helper_lib_warning()
  if warning is not None:
    print(warning)
    return
  callback = f"${{script.frame:{__name__}.frame_annotation}}"
  debugger.HandleCommand(
      f"settings set frame-format '{_DEFAULT_FRAME_FORMAT}{callback}\\n'")
  debugger.HandleCommand(f"command script add -f {__name__}.cmd_v8 v8")
