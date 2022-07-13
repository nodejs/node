# Copyright (c) 2019 Ujjwal Sharma <usharma1998@gmail>. All rights reserved.
# Use of this source code is governed by an MIT-style license.

def DoMain(args):
  old = args.pop(0)
  new = args.pop(0)
  return ' '.join(a.replace(old, new) for a in args)
