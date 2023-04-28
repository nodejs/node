# Copyright (c) 2019 Refael Ackeramnn<refack@gmail.com>. All rights reserved.
# Use of this source code is governed by an MIT-style license.
def DoMain(args):
  format_str = args.pop(0)
  return ' '.join(format_str % a for a in args)
