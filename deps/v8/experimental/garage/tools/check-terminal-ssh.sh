#!/usr/bin/env bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The SSH half of the terminal matrix (TODO 10.3, and the second name in the
# item left open under 5.4).
#
#   tools/check-terminal-ssh.sh [path/to/garage] [--require]
#
# What SSH adds over tools/check-terminal.py: the controlling terminal is a pty
# allocated by sshd rather than by us, there is no local terminal behind it at
# all, and ttyname() answers a /dev/pts device on the far side of a network
# connection. The fd-0 handling in src/tty.rs has to survive that, and until
# now nothing had checked that it does.
#
# Deliberately runs garage *directly* under `ssh -tt` rather than running
# check-terminal.py over ssh: nesting our own pty inside sshd's would put our
# pty back under test and leave sshd's untested, which is backwards.
#
# Without --require, an unreachable localhost sshd is a skip, so the script is
# runnable on a workstation that has no sshd. CI passes --require so that a
# broken setup cannot masquerade as a pass.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GARAGE="${1:-$REPO_ROOT/target/release/garage}"
[[ "${1:-}" == --* ]] && GARAGE="$REPO_ROOT/target/release/garage"
REQUIRE=0
for arg in "$@"; do [[ "$arg" == "--require" ]] && REQUIRE=1; done

TRACE="$REPO_ROOT/fixtures/x64-v16.0.0/maglev-graphs.simple.log"

skip_or_fail() {
  if [[ $REQUIRE -eq 1 ]]; then
    echo "FAIL: $1" >&2
    exit 1
  fi
  echo "skip: $1" >&2
  exit 0
}

[[ -x "$GARAGE" ]] || skip_or_fail "no garage binary at $GARAGE (cargo build --release)"
[[ -r "$TRACE" ]] || skip_or_fail "no trace at $TRACE"
command -v ssh >/dev/null 2>&1 || skip_or_fail "no ssh client"

# Key-based auth to ourselves. Only ever generated when absent, and only used
# for localhost.
setup_self_ssh() {
  local key="$HOME/.ssh/id_garage_check"
  mkdir -p "$HOME/.ssh"
  chmod 700 "$HOME/.ssh"
  if [[ ! -f "$key" ]]; then
    ssh-keygen -t ed25519 -N '' -f "$key" -C 'garage terminal check' >/dev/null 2>&1 || return 1
  fi
  if ! grep -qF "$(cut -d' ' -f2 "$key.pub")" "$HOME/.ssh/authorized_keys" 2>/dev/null; then
    cat "$key.pub" >> "$HOME/.ssh/authorized_keys"
  fi
  chmod 600 "$HOME/.ssh/authorized_keys"
  echo "$key"
}

KEY="$(setup_self_ssh)" || skip_or_fail "cannot set up local ssh key"

SSH_OPTS=(
  -i "$KEY"
  -o StrictHostKeyChecking=no
  -o UserKnownHostsFile=/dev/null
  -o BatchMode=yes
  -o ConnectTimeout=10
  -o LogLevel=ERROR
)

if ! ssh "${SSH_OPTS[@]}" localhost true >/dev/null 2>&1; then
  skip_or_fail "cannot ssh to localhost (is sshd running?)"
fi

REMOTE_OS="$(ssh "${SSH_OPTS[@]}" localhost 'uname -s' 2>/dev/null || echo unknown)"

echo "garage: $GARAGE"
echo "trace:  $TRACE"
echo "ssh:    $(ssh "${SSH_OPTS[@]}" localhost 'echo $USER@$(hostname)' 2>/dev/null) ($REMOTE_OS)"
echo

# timeout(1) is util-linux; macOS has neither it nor gtimeout unless coreutils
# is installed. Fall back to a watchdog so this script runs on a workstation
# too, and so a hung ssh is a reported failure rather than a wedge.
TIMEOUT_CMD=""
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT_CMD=timeout
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT_CMD=gtimeout
fi

bounded() {
  local secs="$1"
  shift
  if [[ -n "$TIMEOUT_CMD" ]]; then
    "$TIMEOUT_CMD" "$secs" "$@"
    return $?
  fi
  # `<&0` is load-bearing: a non-interactive shell points a backgrounded
  # process's stdin at /dev/null, which would silently swallow the keystrokes.
  "$@" <&0 &
  local pid=$!
  (
    sleep "$secs"
    kill -TERM "$pid" 2>/dev/null
    sleep 2
    kill -KILL "$pid" 2>/dev/null
  ) >/dev/null 2>&1 &
  local watchdog=$!
  local rc=0
  wait "$pid" || rc=$?
  kill "$watchdog" 2>/dev/null
  wait "$watchdog" 2>/dev/null
  # Report an expiry the way timeout(1) does. A process killed by any signal
  # lands here, which is close enough: none of these cases should be signalled.
  [[ $rc -ge 128 ]] && rc=124
  return $rc
}

# Keystrokes go in repeatedly rather than once. Unlike the local harness we
# cannot see when the remote TUI has drawn, and a single 'q' sent up front
# races remote shell startup, `cat` and garage's own indexing — it loses that
# race more often than not. Sending until the far side goes away costs nothing:
# extra keys land on a closed channel.
feed_keys() {
  # Ignore SIGPIPE so that ssh exiting first ends this loop through printf's
  # return value instead of killing it. Under `set -o pipefail` a signalled
  # feeder would otherwise become the pipeline's status and report 141 for a
  # case that actually passed.
  trap '' PIPE
  local n=0
  while [[ $n -lt 60 ]]; do
    printf 'q' 2>/dev/null || return 0
    sleep 0.5
    n=$((n + 1))
  done
}

failures=0

# Runs one remote shell command under an sshd-allocated pty, feeds it `q`, and
# checks the exit status. `timeout` bounds a hang; ssh -tt keeps the pty even
# though our own stdin is a pipe.
run_case() {
  local name="$1" why="$2" expect="$3" remote="$4" pty="${5:-pty}"
  printf '  %s ... ' "$name"
  local out status
  local -a tt=()
  [[ "$pty" == "pty" ]] && tt=(-tt)
  # ${arr[@]+…}: bash 3.2 (macOS) treats an empty array as unbound under `set -u`.
  out=$(feed_keys | bounded 40 ssh ${tt[@]+"${tt[@]}"} "${SSH_OPTS[@]}" localhost \
    "cd '$REPO_ROOT' && $remote" 2>&1)
  status=$?

  if [[ $status -eq 124 ]]; then
    echo "FAIL"
    printf '        why it matters: %s\n        timed out\n' "$why"
    failures=$((failures + 1))
    return
  fi

  if [[ "$expect" == "ok" && $status -ne 0 ]]; then
    echo "FAIL"
    printf '        why it matters: %s\n        exit %d: %s\n' \
      "$why" "$status" "$(printf '%s' "$out" | tail -c 300)"
    failures=$((failures + 1))
    return
  fi

  if [[ "$expect" == "clean-error" ]]; then
    if [[ $status -eq 0 ]]; then
      echo "FAIL"
      printf '        why it matters: %s\n        exited 0, expected a diagnostic\n' "$why"
      failures=$((failures + 1))
      return
    fi
    if ! printf '%s' "$out" | grep -q 'garage:'; then
      echo "FAIL"
      printf '        why it matters: %s\n        no diagnostic: %s\n' \
        "$why" "$(printf '%s' "$out" | tail -c 300)"
      failures=$((failures + 1))
      return
    fi
  fi

  echo "ok"
}

run_case "trace file, sshd pty is the terminal" \
  "the baseline: garage on a remote pty at all" \
  ok \
  "'$GARAGE' '$TRACE'"

run_case "piped trace over ssh (ttyname(stdout) = /dev/pts/N)" \
  "the primary d8 | garage invocation, remote" \
  ok \
  "cat '$TRACE' | '$GARAGE'"

run_case "piped trace, stdout redirected (ttyname(stderr))" \
  "second rung of the src/tty.rs ladder, remote" \
  ok \
  "cat '$TRACE' | '$GARAGE' > /dev/null"

# Same platform split as tools/check-terminal.py, for the same reason: on
# darwin a /dev/tty descriptor cannot be registered with kqueue, so the
# fallback is documented to fail (src/tty.rs, "Why /dev/tty is not enough on
# macOS"); on Linux epoll takes it and it must work. stderr is redirected to a
# file rather than /dev/null so the darwin diagnostic is still checkable.
if [[ "$REMOTE_OS" == "Darwin" ]]; then
  # stderr must be a *file* while garage runs, or ttyname(stderr) would find
  # the sshd pty and the /dev/tty rung would never be reached. Replayed
  # afterwards so the diagnostic is still checkable.
  run_case "piped trace, stdout and stderr redirected (/dev/tty only)" \
    "documented macOS kqueue limitation must stay a clean refusal" \
    clean-error \
    "e=\$(mktemp); cat '$TRACE' | '$GARAGE' > /dev/null 2>\"\$e\"; rc=\$?; cat \"\$e\" >&2; rm -f \"\$e\"; exit \$rc"
else
  run_case "piped trace, stdout and stderr redirected (/dev/tty only)" \
    "the /dev/tty fallback 5.4 called out, over ssh on Linux" \
    ok \
    "cat '$TRACE' | '$GARAGE' > /dev/null 2>/dev/null"
fi

# No -t: sshd allocates no pty, so the remote session has no controlling
# terminal at all. stderr stays on the ssh channel so the diagnostic comes back.
run_case "no pty allocated (ssh without -t)" \
  "must refuse with a message rather than hang or crash" \
  clean-error \
  "cat '$TRACE' | '$GARAGE' > /dev/null" \
  no-pty

echo
if [[ $failures -gt 0 ]]; then
  echo "$failures of the ssh terminal matrix failed" >&2
  exit 1
fi
echo "ssh terminal matrix ok"
