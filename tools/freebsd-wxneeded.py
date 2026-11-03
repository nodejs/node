#!/usr/bin/env python3
"""Mark FreeBSD binaries as requiring W+X mappings, or run them after marking.

FreeBSD enforces W^X when kern.elf{32,64}.allow_wx is 0. V8 requires RWX pages
for its code range, so build-time V8 tools need the NT_FREEBSD_FCTL_WXNEEDED
feature bit set before they are executed.
"""

import os
import subprocess
import sys


def mark_wxneeded(binary):
    subprocess.check_call(['/usr/bin/elfctl', '-e', '+wxneeded', binary])


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--mark':
        mark_wxneeded(sys.argv[2])
        with open(sys.argv[3], 'w'):
            pass
        return 0

    if len(sys.argv) >= 5 and sys.argv[1] == '--exec' and sys.argv[3] == '--':
        binary_name = sys.argv[2]
        command = sys.argv[4:]
        for argument in command:
            if os.path.basename(argument) == binary_name:
                mark_wxneeded(argument)
                break
        else:
            print(f'freebsd-wxneeded.py: {binary_name} not found in command',
                  file=sys.stderr)
            return 2
        os.execvp(command[0], command)

    print('usage: freebsd-wxneeded.py --exec <name> -- <command> [args...]',
          file=sys.stderr)
    print('       freebsd-wxneeded.py --mark <binary> <stamp>',
          file=sys.stderr)
    return 2


if __name__ == '__main__':
    raise SystemExit(main())
