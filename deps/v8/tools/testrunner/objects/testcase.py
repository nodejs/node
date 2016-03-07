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


from . import output

class TestCase(object):
  def __init__(self, suite, path, variant='default', flags=None,
               dependency=None, override_shell=None):
    self.suite = suite        # TestSuite object
    self.path = path          # string, e.g. 'div-mod', 'test-api/foo'
    self.flags = flags or []  # list of strings, flags specific to this test
    self.variant = variant    # name of the used testing variant
    self.dependency = dependency  # |path| for testcase that must be run first
    self.override_shell = override_shell
    self.outcomes = set([])
    self.output = None
    self.id = None  # int, used to map result back to TestCase instance
    self.duration = None  # assigned during execution
    self.run = 1  # The nth time this test is executed.

  def CopyAddingFlags(self, variant, flags):
    copy = TestCase(self.suite, self.path, variant, self.flags + flags,
                    self.dependency, self.override_shell)
    copy.outcomes = self.outcomes
    return copy

  def PackTask(self):
    """
    Extracts those parts of this object that are required to run the test
    and returns them as a JSON serializable object.
    """
    assert self.id is not None
    return [self.suitename(), self.path, self.variant, self.flags,
            self.dependency, self.override_shell, list(self.outcomes or []),
            self.id]

  @staticmethod
  def UnpackTask(task):
    """Creates a new TestCase object based on packed task data."""
    # For the order of the fields, refer to PackTask() above.
    test = TestCase(str(task[0]), task[1], task[2], task[3], task[4], task[5])
    test.outcomes = set(task[6])
    test.id = task[7]
    test.run = 1
    return test

  def SetSuiteObject(self, suites):
    self.suite = suites[self.suite]

  def PackResult(self):
    """Serializes the output of the TestCase after it has run."""
    self.suite.StripOutputForTransmit(self)
    return [self.id, self.output.Pack(), self.duration]

  def MergeResult(self, result):
    """Applies the contents of a Result to this object."""
    assert result[0] == self.id
    self.output = output.Output.Unpack(result[1])
    self.duration = result[2]

  def suitename(self):
    return self.suite.name

  def GetLabel(self):
    return self.suitename() + "/" + self.suite.CommonTestName(self)

  def shell(self):
    if self.override_shell:
      return self.override_shell
    return self.suite.shell()

  def __getstate__(self):
    """Representation to pickle test cases.

    The original suite won't be sent beyond process boundaries. Instead
    send the name only and retrieve a process-local suite later.
    """
    return dict(self.__dict__, suite=self.suite.name)
