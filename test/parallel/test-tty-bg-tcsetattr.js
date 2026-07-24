'use strict';

// Regression test for https://github.com/nodejs/node/issues/35536
//
// When Node.js is run as a background process, its stdio reset on exit must
// not call tcsetattr() on the controlling terminal -- otherwise it clobbers
// the foreground process's termios state (e.g. breaks readline in the parent
// shell, or the pager when piping `node ... | less`).
//
// This test allocates a pty, snapshots termios, then forks/execs Node such
// that Node's process group is *not* the foreground process group of the pty.
// After Node exits, it compares termios. If termios changed, the bug is
// present.
//
// Implemented via a Python helper because Node's standard library does not
// expose pty(7) / tcgetpgrp() / setpgid() directly.

const common = require('../common');

if (common.isWindows) {
  common.skip('pty test, POSIX only');
}

const assert = require('assert');
const child_process = require('child_process');

// Probe for python3 (the helper uses the `pty` and `termios` stdlib modules).
const pythonCandidates = ['python3', 'python'];
let python = null;
for (const candidate of pythonCandidates) {
  const probe = child_process.spawnSync(candidate, ['-c', 'import pty, termios'], {
    stdio: 'ignore',
  });
  if (probe.status === 0) {
    python = candidate;
    break;
  }
}
if (python === null) {
  common.skip('python3 with pty/termios modules not available');
}

const helper = `
import os
import pty
import sys
import termios

node = sys.argv[1]

parent_fd, child_fd = pty.openpty()

# Snapshot termios on the slave side before running node.
before = termios.tcgetattr(child_fd)

pid = os.fork()
if pid == 0:
    # Child: become leader of a new session, attach the pty as our
    # controlling terminal, then place ourselves in a *new* process group
    # that is NOT the foreground process group of the tty. From the tty's
    # perspective, this child is "backgrounded".
    os.setsid()
    os.close(parent_fd)

    name = os.ttyname(child_fd)
    fd = os.open(name, os.O_RDWR)
    os.dup2(fd, child_fd)
    os.close(fd)

    os.dup2(child_fd, 0)
    os.dup2(child_fd, 1)
    os.dup2(child_fd, 2)
    if child_fd > 2:
        os.close(child_fd)

    # The session leader's pgrp is currently the tty's foreground pgrp
    # (set by the TIOCSCTTY in the open above). Move to a new pgrp so that
    # the foreground pgrp != getpgrp(). This is the configuration the bug
    # reporter hits with shell job-control 'bg'.
    os.setpgid(0, 0)
    new_pgrp = os.getpgrp()
    fg_pgrp = os.tcgetpgrp(0)
    if fg_pgrp == new_pgrp:
        # Could not background ourselves; bail loudly so the test fails.
        os._exit(99)

    # Run node. It should exit cleanly without modifying termios.
    os.execvp(node, [node, '-e', '0'])
    os._exit(98)

os.close(child_fd)
# Drain the parent_fd so the child isn't blocked on output (it shouldn't
# print anything, but be defensive).
try:
    import select
    while True:
        rfds, _, _ = select.select([parent_fd], [], [], 0.05)
        if not rfds:
            # Check whether child has exited.
            wpid, _ = os.waitpid(pid, os.WNOHANG)
            if wpid == pid:
                break
            continue
        try:
            data = os.read(parent_fd, 4096)
        except OSError:
            break
        if not data:
            break
finally:
    pass

# Make sure we have reaped the child.
try:
    wpid, status = os.waitpid(pid, 0)
except ChildProcessError:
    status = 0

after = termios.tcgetattr(child_fd)
os.close(parent_fd)

if before != after:
    sys.stderr.write('termios changed: before=%r after=%r\\n' % (before, after))
    sys.exit(1)

sys.exit(0)
`;

const result = child_process.spawnSync(python, ['-c', helper, process.execPath], {
  encoding: 'utf8',
  stdio: ['ignore', 'pipe', 'pipe'],
});

assert.strictEqual(
  result.status, 0,
  `helper failed (status=${result.status}, signal=${result.signal})\n` +
  `stdout: ${result.stdout}\nstderr: ${result.stderr}`
);
