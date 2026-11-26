#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Terminal-handling matrix for garage (TODO 10.3, and the item left open
under 5.4).

Phase 1.5 pointed fd 0 at the controlling terminal so a piped trace and a
keyboard can coexist, and src/tty.rs documents three routes to that terminal:
ttyname(stdout), ttyname(stderr), and /dev/tty. Only the macOS half was ever
measured; the Linux half was *reasoned* about. This script measures it.

Why Python and not shell like the rest of tools/: driving a TUI needs a real
pty with a controllable window size and TERM, and needs to send a keystroke
only once the program is up. `script(1)` offers none of that portably — its
stdin forwarding differs between util-linux and BSD, and it cannot set the
window size at all. `pty` + `termios` + `fcntl` are Python stdlib, present on
macOS and on GitHub's runners.

    tools/check-terminal.py [path/to/garage] [--trace FILE]

Exits non-zero if any case fails. Every case is time-bounded, so a hang is a
reported failure rather than a wedged terminal.
"""

from __future__ import annotations

import argparse
import errno
import fcntl
import os
import pty
import select
import selectors
import shutil
import signal
import struct
import sys
import termios
import threading
import time

# Generous: a cold binary on a loaded CI runner still starts well inside this,
# and the only cost of a high ceiling is how long a genuine hang takes to fail.
TIMEOUT = 25.0
# How long to wait for the first byte of output before deciding the TUI is up
# and a keystroke will land in a reader rather than a buffer.
STARTUP = 10.0


class Case:
  """One row of the matrix. `run` returns None on success, else a reason."""

  def __init__(self, name: str, why: str, run):
    self.name = name
    self.why = why
    self.run = run


def set_winsize(fd: int, rows: int, cols: int) -> None:
  fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def read_or_eof(fd: int) -> bytes:
  """Read, treating a hung-up pty as EOF.

    Once the last slave closes, reading the master raises EIO rather than
    returning b"" — on Linux and macOS both. That is the normal end of every
    case here (the child exited), so it must not read as a harness error.
    """
  try:
    return os.read(fd, 65536)
  except OSError as e:
    if e.errno in (errno.EIO, errno.EBADF):
      return b""
    raise


def write_ignoring_hangup(fd: int, data: bytes) -> None:
  """Write to a pty master that the child may already have abandoned."""
  try:
    os.write(fd, data)
  except OSError as e:
    if e.errno not in (errno.EIO, errno.EPIPE, errno.EBADF):
      raise


def feed_pipe(fd: int, data: bytes) -> threading.Thread:
  """Feed `data` into `fd` from a thread, then close it.

    Must not be a synchronous write: a trace is bigger than a pipe buffer
    (64 KB on macOS), so writing inline would block until the child drained
    it — and would raise EPIPE for the cases where the child is *supposed* to
    exit before reading anything, like having no terminal to draw on.
    """

  def pump():
    try:
      os.write(fd, data)
    except OSError:
      pass  # child exited early; that is a result, not an error
    finally:
      try:
        os.close(fd)
      except OSError:
        pass

  t = threading.Thread(target=pump, daemon=True)
  t.start()
  return t


def drive_in_pty(
    argv: list[str],
    *,
    rows: int = 24,
    cols: int = 80,
    term: str = "xterm-256color",
    stdin_data: bytes | None = None,
    redirect_stdout: bool = False,
    redirect_stderr: bool = False,
    keys: bytes = b"q",
    new_session: bool = False,
) -> tuple[int | None, bytes, str | None]:
  """Run argv with a pty as the controlling terminal.

    `stdin_data`, when given, is fed through a pipe on fd 0 — the primary
    `d8 … | garage` shape, where the terminal must be reached some other way.
    `redirect_stdout`/`redirect_stderr` send that stream to a pipe instead of
    the pty, which is how the ttyname(stdout) → ttyname(stderr) → /dev/tty
    ladder in src/tty.rs gets walked one rung at a time.

    Returns (exit_status, pty_output, error). exit_status is None on timeout.
    """
  master, slave = pty.openpty()
  set_winsize(master, rows, cols)

  data_r = data_w = None
  if stdin_data is not None:
    data_r, data_w = os.pipe()

  # Any stream we redirect goes to a pipe we keep the read end of, so the
  # child cannot block on a full pipe buffer and we can report what it said.
  out_r = out_w = None
  if redirect_stdout or redirect_stderr:
    out_r, out_w = os.pipe()

  pid = os.fork()
  if pid == 0:  # child
    try:
      os.close(master)
      if new_session:
        # Detach from the controlling terminal entirely: /dev/tty then
        # cannot be opened, which is the "no keyboard anywhere" case.
        os.setsid()
      else:
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

      os.dup2(data_r if stdin_data is not None else slave, 0)
      os.dup2(out_w if redirect_stdout else slave, 1)
      os.dup2(out_w if redirect_stderr else slave, 2)

      # Keep one descriptor on the slave unless the point of the case is
      # to have no controlling terminal. When every standard stream is
      # redirected, nothing else would hold the pty open, and macOS
      # revokes the session's controlling terminal as soon as the last
      # descriptor closes — /dev/tty then fails with ENXIO and we would
      # be measuring that teardown rather than garage's fallback. A real
      # session always has a terminal holder: the shell.
      keep = set()
      if not new_session:
        holder = os.dup(slave)
        # PEP 446: descriptors Python creates are close-on-exec, so
        # this one would vanish at execvpe and take the controlling
        # terminal with it — the very thing it exists to hold.
        os.set_inheritable(holder, True)
        keep.add(holder)

      for fd in {slave, data_r, data_w, out_r, out_w} - {None} - keep:
        if fd > 2:
          try:
            os.close(fd)
          except OSError:
            pass

      env = dict(os.environ, TERM=term)
      env.pop("GARAGE_CONFIG", None)
      os.execvpe(argv[0], argv, env)
    except BaseException:
      os._exit(127)

  # parent
  os.close(slave)
  if out_w is not None:
    os.close(out_w)
  if data_r is not None:
    os.close(data_r)

  if stdin_data is not None:
    feed_pipe(data_w, stdin_data)

  sel = selectors.DefaultSelector()
  sel.register(master, selectors.EVENT_READ)
  if out_r is not None:
    sel.register(out_r, selectors.EVENT_READ)

  seen = bytearray()
  redirected = bytearray()
  sent = False
  deadline = time.monotonic() + TIMEOUT
  startup_deadline = time.monotonic() + STARTUP
  status: int | None = None

  try:
    while True:
      if status is None:
        waited, code = os.waitpid(pid, os.WNOHANG)
        if waited == pid:
          status = code
          # Drain whatever is still buffered, then stop.
          for _ in range(64):
            events = sel.select(timeout=0.05)
            if not events:
              break
            for key, _mask in events:
              chunk = read_or_eof(key.fd)
              if not chunk:
                continue
              (seen if key.fd == master else redirected).extend(chunk)
          break

      now = time.monotonic()
      if now > deadline:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
        return None, bytes(seen + redirected), "timed out"

      for key, _mask in sel.select(timeout=0.1):
        chunk = read_or_eof(key.fd)
        if chunk:
          (seen if key.fd == master else redirected).extend(chunk)

      # Send the quit key once the TUI has drawn, so we are testing a
      # live reader rather than the pty's input buffer. If nothing is
      # ever drawn (a redirected-output case), fall back to the deadline.
      if not sent and (seen or now > startup_deadline):
        write_ignoring_hangup(master, keys)
        sent = True
  finally:
    sel.close()
    os.close(master)
    if out_r is not None:
      try:
        os.close(out_r)
      except OSError:
        pass

  return status, bytes(seen + redirected), None


def exited_ok(status: int | None) -> bool:
  return status is not None and os.WIFEXITED(status) and os.WEXITSTATUS(
      status) == 0


def describe(status: int | None) -> str:
  if status is None:
    return "no exit (timed out)"
  if os.WIFSIGNALED(status):
    return f"killed by signal {os.WTERMSIG(status)}"
  return f"exit {os.WEXITSTATUS(status)}"


def dev_tty_only(garage: str, trace_bytes: bytes):
  """The case 5.4 left open: /dev/tty is the only remaining route.

    The expectation is deliberately platform-split, because src/tty.rs says it
    is: on darwin a descriptor from opening /dev/tty cannot be registered with
    kqueue, so crossterm's reader never initialises (module comment, "Why
    /dev/tty is not enough on macOS"); on Linux epoll takes it and the fallback
    genuinely works. Asserting one behaviour on both would either hide the
    macOS limitation or invent a Linux bug.

    On darwin what is checked is that the limitation stays *graceful*: a named
    error and a non-zero exit. Not a panic, and specifically not a hang — the
    same module comment records that crossterm's cursor-position retry can spin
    at 100% CPU forever, which a timeout here would catch.
    """

  def run():
    status, out, err = drive_in_pty(
        [garage],
        stdin_data=trace_bytes,
        redirect_stdout=True,
        redirect_stderr=True,
    )
    text = out.decode("utf-8", "replace")
    if os.uname().sysname != "Darwin":
      if err:
        return f"{err}; output: {text[-300:]!r}"
      if not exited_ok(status):
        return f"{describe(status)}; output: {text[-400:]!r}"
      return None
    # darwin: a clean, named refusal is the documented outcome.
    if err:
      return f"{err} — the documented macOS failure must be prompt, not a hang"
    if status is None:
      return "no exit"
    if os.WIFSIGNALED(status):
      return f"{describe(status)} — a crash, not a clean error"
    if os.WEXITSTATUS(status) == 0:
      return ("succeeded on macOS, where src/tty.rs documents kqueue "
              "rejecting a /dev/tty descriptor — if this now works, that "
              "comment and this case are both stale")
    if "garage:" not in text:
      return f"non-zero exit but no diagnostic; output: {text[-300:]!r}"
    return None

  return run


def build_cases(garage: str, trace: str, trace_bytes: bytes) -> list[Case]:

  def tui(**kw):
    """A case that must start the TUI and quit cleanly on `q`."""

    def run():
      status, out, err = drive_in_pty([garage] + kw.pop("args", [trace]), **kw)
      if err:
        return f"{err}; output: {out[-200:]!r}"
      if not exited_ok(status):
        return f"{describe(status)}; output: {out[-400:]!r}"
      return None

    return run

  cases = [
      Case(
          "file arg, stdin is the terminal",
          "the simplest shape: no fd juggling at all",
          tui(),
      ),
      Case(
          "piped trace, stdout is the terminal",
          "the primary `d8 … | garage` invocation; terminal via ttyname(stdout)",
          tui(args=[], stdin_data=trace_bytes),
      ),
      Case(
          "piped trace, stdout redirected, stderr is the terminal",
          "second rung of the ladder: terminal via ttyname(stderr)",
          tui(args=[], stdin_data=trace_bytes, redirect_stdout=True),
      ),
      Case(
          "piped trace, neither stdout nor stderr is a terminal",
          "the /dev/tty fallback — the case 5.4 called out by name",
          dev_tty_only(garage, trace_bytes),
      ),
      Case(
          "80x24 @ TERM=xterm (16 colour)",
          "colour-depth floor from the 5.4 matrix",
          tui(term="xterm"),
      ),
      Case(
          "40x10 (small viewport)",
          "layout must survive a viewport smaller than the chrome",
          tui(rows=10, cols=40),
      ),
      Case(
          "200x50 (large viewport)",
          "the other end of the 5.4 matrix",
          tui(rows=50, cols=200),
      ),
  ]

  def resize():
    """Resize mid-run, then quit. Exercises the SIGWINCH path."""
    master, slave = pty.openpty()
    set_winsize(master, 24, 80)
    pid = os.fork()
    if pid == 0:
      try:
        os.close(master)
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        os.dup2(slave, 0)
        os.dup2(slave, 1)
        os.dup2(slave, 2)
        if slave > 2:
          os.close(slave)
        os.execvpe(garage, [garage, trace],
                   dict(os.environ, TERM="xterm-256color"))
      except BaseException:
        os._exit(127)
    os.close(slave)

    deadline = time.monotonic() + TIMEOUT
    seen = bytearray()
    stage = 0
    sizes = [(10, 40), (50, 200), (24, 80)]
    status = None
    try:
      while True:
        waited, code = os.waitpid(pid, os.WNOHANG)
        if waited == pid:
          status = code
          break
        if time.monotonic() > deadline:
          os.kill(pid, signal.SIGKILL)
          os.waitpid(pid, 0)
          return "timed out during resize"
        r, _, _ = select.select([master], [], [], 0.15)
        if r:
          seen.extend(read_or_eof(master))
        if seen:
          if stage < len(sizes):
            set_winsize(master, *sizes[stage])
            try:
              os.kill(pid, signal.SIGWINCH)
            except ProcessLookupError:
              pass
            stage += 1
            time.sleep(0.3)
          else:
            write_ignoring_hangup(master, b"q")
            stage += 1
    finally:
      os.close(master)
    if not exited_ok(status):
      return f"{describe(status)} after resizes"
    return None

  cases.append(
      Case(
          "resize mid-run (80x24 → 40x10 → 200x50 → 80x24)",
          "SIGWINCH handling from the 5.4 matrix",
          resize,
      ))

  def no_terminal():
    """No controlling terminal anywhere: must be a clean error, not a panic.

        This is the promise at the bottom of open_controlling_terminal(): a
        message naming the problem, and a non-zero exit.
        """
    status, out, err = drive_in_pty(
        [garage],
        stdin_data=trace_bytes,
        redirect_stdout=True,
        redirect_stderr=True,
        new_session=True,
    )
    if err:
      return f"{err} (expected a prompt clean failure)"
    if status is None:
      return "no exit"
    if os.WIFSIGNALED(status):
      return f"{describe(status)} — a crash, not a clean error"
    if os.WEXITSTATUS(status) == 0:
      return "exited 0 with no terminal to draw on"
    text = out.decode("utf-8", "replace")
    if "garage:" not in text:
      return f"non-zero exit but no diagnostic; output: {text[-300:]!r}"
    return None

  cases.append(
      Case(
          "no controlling terminal at all",
          "must fail with a message, never a panic",
          no_terminal,
      ))
  return cases


def main() -> int:
  repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  ap = argparse.ArgumentParser(description=__doc__)
  ap.add_argument(
      "garage",
      nargs="?",
      default=os.path.join(repo, "target/release/garage"),
      help="path to the garage binary (default: target/release/garage)",
  )
  ap.add_argument(
      "--trace",
      default=os.path.join(repo,
                           "fixtures/x64-v16.0.0/maglev-graphs.simple.log"),
      help="trace file to open",
  )
  args = ap.parse_args()

  garage = shutil.which(args.garage) or args.garage
  if not os.path.exists(garage):
    print(
        f"no garage binary at {garage} (cargo build --release)",
        file=sys.stderr)
    return 2
  if not os.path.exists(args.trace):
    print(f"no trace at {args.trace}", file=sys.stderr)
    return 2

  with open(args.trace, "rb") as fh:
    trace_bytes = fh.read()

  print(f"garage: {garage}")
  print(f"trace:  {args.trace} ({len(trace_bytes)} bytes)")
  print(f"host:   {os.uname().sysname} {os.uname().machine}\n")

  failures = []
  for case in build_cases(garage, args.trace, trace_bytes):
    print(f"  {case.name} ... ", end="", flush=True)
    try:
      reason = case.run()
    except Exception as e:  # a broken harness must not read as a pass
      reason = f"harness error: {e!r}"
    if reason is None:
      print("ok")
    else:
      print("FAIL")
      failures.append((case, reason))

  print()
  if failures:
    for case, reason in failures:
      print(f"FAIL  {case.name}")
      print(f"      why it matters: {case.why}")
      print(f"      {reason}\n")
    print(f"{len(failures)} of the terminal matrix failed")
    return 1

  print("terminal matrix ok")
  return 0


if __name__ == "__main__":
  sys.exit(main())
