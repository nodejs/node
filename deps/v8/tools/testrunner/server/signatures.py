# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import base64
import os
import subprocess


def ReadFileAndSignature(filename):
  with open(filename, "rb") as f:
    file_contents = base64.b64encode(f.read())
  signature_file = filename + ".signature"
  if (not os.path.exists(signature_file) or
      os.path.getmtime(signature_file) < os.path.getmtime(filename)):
    private_key = "~/.ssh/v8_dtest"
    code = subprocess.call("openssl dgst -out %s -sign %s %s" %
                           (signature_file, private_key, filename),
                           shell=True)
    if code != 0: return [None, code]
  with open(signature_file) as f:
    signature = base64.b64encode(f.read())
  return [file_contents, signature]


def VerifySignature(filename, file_contents, signature, pubkeyfile):
  with open(filename, "wb") as f:
    f.write(base64.b64decode(file_contents))
  signature_file = filename + ".foreign_signature"
  with open(signature_file, "wb") as f:
    f.write(base64.b64decode(signature))
  code = subprocess.call("openssl dgst -verify %s -signature %s %s" %
                         (pubkeyfile, signature_file, filename),
                         shell=True)
  matched = (code == 0)
  if not matched:
    os.remove(signature_file)
    os.remove(filename)
  return matched
