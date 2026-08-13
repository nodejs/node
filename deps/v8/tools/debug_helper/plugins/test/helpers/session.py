# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Interactive gdb / lldb session over a pty, driven by injected markers."""

import errno
import os
import pty
import re
import select
import subprocess
import time
from collections.abc import Iterable


class DebuggerSession:
  """Base class for an interactive gdb / lldb session over a pty."""

  _DEFAULT_READ_TIMEOUT = 30.0
  # Grace period for the debugger to exit after `quit` before we SIGKILL it.
  _QUIT_GRACE_TIMEOUT = 2.0
  # os.read() chunk size when draining the pty.
  _READ_CHUNK_BYTES = 4096
  # How much of the pending buffer to include in a read-timeout error.
  _ERROR_TAIL_BYTES = 512
  _SYNC_RETRY_TIMEOUT = 2.0
  _SYNC_MAX_ATTEMPTS = 5
  _STOP_BYTE_RE = None

  def __init__(self, config, target_binary, target_args="", core_path=None):
    self._config = config
    self._target = target_binary
    self._target_args = target_args
    self._core = core_path
    self._env = os.environ.copy()
    self._env["V8_DEBUG_HELPER_LIB_PATH"] = config.debug_helper_lib
    # The tests match English debugger output.
    self._env["LC_ALL"] = "C"
    self._proc = None
    self._fd = None
    self._buf = b""
    self._marker_seq = 0

  def _spawn_argv(self):
    raise NotImplementedError

  def _setup_commands(self) -> Iterable[str]:
    """Iterable of commands sent after spawn (e.g. load plugin, set args)."""
    return ()

  def _make_marker_cmd(self, token):
    """Return a debugger command that prints `token` on its own line."""
    raise NotImplementedError

  def __enter__(self):
    self._spawn()
    try:
      for cmd in self._setup_commands():
        self.send(cmd)
      # Drain banner + setup output by syncing on a marker before tests begin.
      self._sync()
    except Exception:
      self.close()
      raise
    return self

  def _sync(self):
    """Inject markers until one echoes back to ensure the debugger is reading
    commands again.
    """
    for attempt in range(self._SYNC_MAX_ATTEMPTS):
      token = self._next_marker()
      self.send(self._make_marker_cmd(token))
      timeout = (
          self._DEFAULT_READ_TIMEOUT if attempt == self._SYNC_MAX_ATTEMPTS -
          1 else self._SYNC_RETRY_TIMEOUT)
      try:
        self._read_until(re.escape(token.encode()), timeout=timeout)
        return
      except TimeoutError:
        continue
    raise TimeoutError(
        f"debugger did not acknowledge {self._SYNC_MAX_ATTEMPTS} sync markers")

  def __exit__(self, *_exc):
    self.close()

  def _spawn(self):
    master, slave = pty.openpty()
    # Feed commands through a pipe to prevent the prompt redraws from
    # racing against input written to the pty.
    self._proc = subprocess.Popen(
        self._spawn_argv(),
        stdin=subprocess.PIPE,
        stdout=slave,
        stderr=slave,
        env=self._env,
        close_fds=True)
    os.close(slave)
    self._fd = master

  def close(self):
    if self._proc is None:
      return
    try:
      self.send("quit")
    except Exception:
      pass
    # Closing the command pipe both releases it and sends EOF, which is a
    # second quit signal in case `quit` was not processed.
    if self._proc.stdin is not None:
      try:
        self._proc.stdin.close()
      except OSError:
        pass
    try:
      self._proc.wait(timeout=self._QUIT_GRACE_TIMEOUT)
    except subprocess.TimeoutExpired:
      self._proc.kill()
      self._proc.wait()
    if self._fd is not None:
      try:
        os.close(self._fd)
      except OSError:
        pass
    self._fd = None
    self._proc = None

  def send(self, line):
    assert self._proc is not None and self._proc.stdin is not None
    self._proc.stdin.write((line + "\n").encode())
    self._proc.stdin.flush()

  def _read_until(self, pattern_bytes, timeout=None):
    """Read until `pattern_bytes` appears in the buffer.

    Returns the text *before* the match (decoded), with everything from the
    match onward discarded.
    """
    if timeout is None:
      timeout = self._DEFAULT_READ_TIMEOUT
    assert self._fd is not None
    deadline = time.monotonic() + timeout
    while True:
      m = re.search(pattern_bytes, self._buf)
      if m:
        before = self._buf[:m.start()]
        self._buf = self._buf[m.end():]
        return before.decode(errors="replace")
      remaining = deadline - time.monotonic()
      if remaining <= 0:
        raise TimeoutError(
            f"timeout waiting for {pattern_bytes!r}; "
            f"tail of buffer: {self._buf[-self._ERROR_TAIL_BYTES:]!r}")
      ready, _, _ = select.select([self._fd], [], [], remaining)
      if not ready:
        continue
      try:
        chunk = os.read(self._fd, self._READ_CHUNK_BYTES)
      except OSError as e:
        if e.errno == errno.EIO:
          # PTY master sees EIO when the child closes its end.
          raise EOFError(self._buf.decode(errors="replace"))
        raise
      if not chunk:
        raise EOFError(self._buf.decode(errors="replace"))
      self._buf += chunk

  def _next_marker(self):
    self._marker_seq += 1
    return f"V8DBG_SYNC_{self._marker_seq:04d}"

  def run_command(self, cmd, timeout=None):
    """Send `cmd`, return its captured output up to the next marker."""
    token = self._next_marker()
    self.send(cmd)
    self.send(self._make_marker_cmd(token))
    text = self._read_until(re.escape(token.encode()), timeout=timeout)
    # Strip the debugger prompt that prefixes each command. Input arrives
    # through a pipe so it is not echoed, but the prompt itself still leaks
    # into captured output (and breaks `^0x...` head-line regexes).
    return re.sub(r"^\((?:gdb|lldb)\)\s?", "", text, flags=re.MULTILINE)

  def run_to_abort(self):
    """Send `run` and wait for the program to stop at a signal/breakpoint."""
    self.send("run")
    self._read_until(self._STOP_BYTE_RE, timeout=self._DEFAULT_READ_TIMEOUT)
    self._sync()

  def v8_inspect(self, address):
    """Run `v8 inspect <address>` and return its captured output."""
    return self.run_command(f"v8 inspect {address}")

  def frame_trailer(self, function_name):
    """Parse the `(this=0xADDR, argc=N)` trailer for one JS frame."""
    bt = self.run_command("bt")
    pattern = re.compile(rf"\[{re.escape(function_name)}[^\[\]]*\][^\n]*"
                         rf"this=(0x[0-9a-f]+), argc=(\d+)")
    m = pattern.search(bt)
    if not m:
      raise AssertionError(f"no `(this=..., argc=...)` trailer for frame "
                           f"{function_name!r} in backtrace:\n{bt}")
    return m.group(1), int(m.group(2))

  def frame_receiver(self, function_name):
    """Get the receiver address for the JS frame named `function_name`."""
    return self.frame_trailer(function_name)[0]


class GdbSession(DebuggerSession):
  """gdb-flavored interactive session."""

  _STOP_BYTE_RE = rb"received signal SIG"

  def _spawn_argv(self):
    init_commands = [
        "set debuginfod enabled off",
        "set pagination off",
        "set confirm off",
        "set style enabled off",
        f"source {self._config.gdbinit_path}",
        f"source {self._config.plugin_path}",
    ]
    argv = [self._config.debugger_binary, "-nx", "-q"]
    for command in init_commands:
      argv += ["-iex", command]
    argv.append(self._target)
    return argv

  def _setup_commands(self):
    if self._core:
      yield f"core-file {self._core}"
    else:
      # Without the redirection the debuggee inherits the command pipe as
      # its stdin and could consume queued debugger commands.
      yield f"set args {self._target_args} </dev/null"

  def _make_marker_cmd(self, token):
    # gdb's `echo` prints its argument verbatim; \n produces the trailing
    # newline that lets us anchor the marker on its own line.
    return rf"echo {token}\n"

  def select_thread(self, index):
    """Select thread `index`. Returns False if the thread does not exist."""
    output = self.run_command(f"thread {index}")
    return "Switching to thread" in output

  def select_frame(self, index):
    """Select frame `index` on the selected thread."""
    self.run_command(f"frame {index}")

  def frame_variable(self, name):
    """Print variable `name` in the selected frame, in hex."""
    return self.run_command(f"print /x {name}")


class LldbSession(DebuggerSession):
  """lldb-flavored interactive session."""

  _STOP_BYTE_RE = rb"(?:Process \d+ stopped|stop reason = signal)"

  def _spawn_argv(self):
    return [self._config.debugger_binary, "-X"]

  def _setup_commands(self):
    yield "settings set use-color false"
    # Otherwise lldb prints each command back, that echo contains our
    # marker text and confuses the marker-based output capture.
    yield "settings set interpreter.echo-commands false"
    yield "settings set interpreter.echo-comment-commands false"
    yield f'command script import "{self._config.plugin_path}"'
    if self._core:
      yield f'target create --core "{self._core}" "{self._target}"'
    else:
      yield f'target create "{self._target}"'
      if self._target_args:
        yield f"settings set -- target.run-args {self._target_args}"

  def _make_marker_cmd(self, token):
    # lldb echoes input back through the pty even with termios ECHO off, so
    # split the token across concatenation: the literal marker only appears
    # in the print output, not in the echoed command line.
    half = len(token) // 2
    return f'script print({token[:half]!r} + {token[half:]!r})'

  def select_thread(self, index):
    """Select thread `index`. Returns False if the thread does not exist."""
    # Uses the script API rather than `thread select`, whose confirmation is
    # printed asynchronously and would race the marker-based output capture.
    output = self.run_command(
        "script print(lldb.debugger.GetSelectedTarget().GetProcess()"
        f".SetSelectedThreadByIndexID({index}))")
    return "True" in output

  def select_frame(self, index):
    """Select frame `index` on the selected thread."""
    self.run_command(f"frame select {index}")

  def frame_variable(self, name):
    """Print variable `name` in the selected frame, in hex."""
    # Uses `frame variable` rather than the expression evaluator, which is
    # more reliable for variables in optimized frames.
    return self.run_command(f"frame variable -f hex {name}")
