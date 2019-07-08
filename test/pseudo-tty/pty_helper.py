import errno
import os
import pty
import select
import signal
import sys
import termios

STDIN = 0
STDOUT = 1
STDERR = 2


def pipe(sfd, dfd):
  try:
    data = os.read(sfd, 256)
  except OSError as e:
    if e.errno != errno.EIO:
      raise
    return True  # EOF

  if not data:
    return True  # EOF

  if dfd == STDOUT:
    # Work around platform quirks. Some platforms echo ^D as \x04
    # (AIX, BSDs) and some don't (Linux).
    filt = lambda c: ord(c) > 31 or c in '\t\n\r\f'
    data = filter(filt, data)

  while data:
    try:
      n = os.write(dfd, data)
    except OSError as e:
      if e.errno != errno.EIO:
        raise
      return True  # EOF
    data = data[n:]


if __name__ == '__main__':
  argv = sys.argv[1:]

  # Make select() interruptable by SIGCHLD.
  signal.signal(signal.SIGCHLD, lambda nr, _: None)

  master_fd, slave_fd = pty.openpty()
  assert master_fd > STDIN

  mode = termios.tcgetattr(slave_fd)
  # Don't translate \n to \r\n.
  mode[1] = mode[1] & ~termios.ONLCR  # oflag
  # Disable ECHOCTL. It's a BSD-ism that echoes e.g. \x04 as ^D but it
  # doesn't work on platforms like AIX and Linux. I checked Linux's tty
  # driver and it's a no-op, the driver is just oblivious to the flag.
  mode[3] = mode[3] & ~termios.ECHOCTL  # lflag
  termios.tcsetattr(slave_fd, termios.TCSANOW, mode)

  pid = os.fork()
  if not pid:
    os.setsid()
    os.close(master_fd)

    # Ensure the pty is a controlling tty.
    name = os.ttyname(slave_fd)
    fd = os.open(name, os.O_RDWR)
    os.dup2(fd, slave_fd)
    os.close(fd)

    os.dup2(slave_fd, STDIN)
    os.dup2(slave_fd, STDOUT)
    os.dup2(slave_fd, STDERR)

    if slave_fd > STDERR:
      os.close(slave_fd)

    os.execve(argv[0], argv, os.environ)
    raise Exception('unreachable')

  os.close(slave_fd)

  fds = [STDIN, master_fd]
  while fds:
    try:
      rfds, _, _ = select.select(fds, [], [])
    except select.error as e:
      if e[0] != errno.EINTR:
        raise
      if pid == os.waitpid(pid, os.WNOHANG)[0]:
        break

    if STDIN in rfds:
      if pipe(STDIN, master_fd):
        fds.remove(STDIN)

    if master_fd in rfds:
      if pipe(master_fd, STDOUT):
        break
