#!/usr/bin/env python
# Copyright 2013 the V8 project authors. All rights reserved.
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

import datetime
import os
import re
import subprocess
import sys
import textwrap
import time
import urllib2

PERSISTFILE_BASENAME = "PERSISTFILE_BASENAME"
TEMP_BRANCH = "TEMP_BRANCH"
BRANCHNAME = "BRANCHNAME"
DOT_GIT_LOCATION = "DOT_GIT_LOCATION"
VERSION_FILE = "VERSION_FILE"
CHANGELOG_FILE = "CHANGELOG_FILE"
CHANGELOG_ENTRY_FILE = "CHANGELOG_ENTRY_FILE"
COMMITMSG_FILE = "COMMITMSG_FILE"
PATCH_FILE = "PATCH_FILE"


def TextToFile(text, file_name):
  with open(file_name, "w") as f:
    f.write(text)


def AppendToFile(text, file_name):
  with open(file_name, "a") as f:
    f.write(text)


def LinesInFile(file_name):
  with open(file_name) as f:
    for line in f:
      yield line


def FileToText(file_name):
  with open(file_name) as f:
    return f.read()


def MSub(rexp, replacement, text):
  return re.sub(rexp, replacement, text, flags=re.MULTILINE)


def Fill80(line):
  # Replace tabs and remove surrounding space.
  line = re.sub(r"\t", r"        ", line.strip())

  # Format with 8 characters indentation and line width 80.
  return textwrap.fill(line, width=80, initial_indent="        ",
                       subsequent_indent="        ")


def GetLastChangeLogEntries(change_log_file):
  result = []
  for line in LinesInFile(change_log_file):
    if re.search(r"^\d{4}-\d{2}-\d{2}:", line) and result: break
    result.append(line)
  return "".join(result)


def MakeComment(text):
  return MSub(r"^( ?)", "#", text)


def StripComments(text):
  # Use split not splitlines to keep terminal newlines.
  return "\n".join(filter(lambda x: not x.startswith("#"), text.split("\n")))


def MakeChangeLogBody(commit_messages, auto_format=False):
  result = ""
  added_titles = set()
  for (title, body, author) in commit_messages:
    # TODO(machenbach): Better check for reverts. A revert should remove the
    # original CL from the actual log entry.
    title = title.strip()
    if auto_format:
      # Only add commits that set the LOG flag correctly.
      log_exp = r"^[ \t]*LOG[ \t]*=[ \t]*(?:(?:Y(?:ES)?)|TRUE)"
      if not re.search(log_exp, body, flags=re.I | re.M):
        continue
      # Never include reverts.
      if title.startswith("Revert "):
        continue
      # Don't include duplicates.
      if title in added_titles:
        continue

    # Add and format the commit's title and bug reference. Move dot to the end.
    added_titles.add(title)
    raw_title = re.sub(r"(\.|\?|!)$", "", title)
    bug_reference = MakeChangeLogBugReference(body)
    space = " " if bug_reference else ""
    result += "%s\n" % Fill80("%s%s%s." % (raw_title, space, bug_reference))

    # Append the commit's author for reference if not in auto-format mode.
    if not auto_format:
      result += "%s\n" % Fill80("(%s)" % author.strip())

    result += "\n"
  return result


def MakeChangeLogBugReference(body):
  """Grep for "BUG=xxxx" lines in the commit message and convert them to
  "(issue xxxx)".
  """
  crbugs = []
  v8bugs = []

  def AddIssues(text):
    ref = re.match(r"^BUG[ \t]*=[ \t]*(.+)$", text.strip())
    if not ref:
      return
    for bug in ref.group(1).split(","):
      bug = bug.strip()
      match = re.match(r"^v8:(\d+)$", bug)
      if match: v8bugs.append(int(match.group(1)))
      else:
        match = re.match(r"^(?:chromium:)?(\d+)$", bug)
        if match: crbugs.append(int(match.group(1)))

  # Add issues to crbugs and v8bugs.
  map(AddIssues, body.splitlines())

  # Filter duplicates, sort, stringify.
  crbugs = map(str, sorted(set(crbugs)))
  v8bugs = map(str, sorted(set(v8bugs)))

  bug_groups = []
  def FormatIssues(prefix, bugs):
    if len(bugs) > 0:
      plural = "s" if len(bugs) > 1 else ""
      bug_groups.append("%sissue%s %s" % (prefix, plural, ", ".join(bugs)))

  FormatIssues("", v8bugs)
  FormatIssues("Chromium ", crbugs)

  if len(bug_groups) > 0:
    return "(%s)" % ", ".join(bug_groups)
  else:
    return ""


# Some commands don't like the pipe, e.g. calling vi from within the script or
# from subscripts like git cl upload.
def Command(cmd, args="", prefix="", pipe=True):
  # TODO(machenbach): Use timeout.
  cmd_line = "%s %s %s" % (prefix, cmd, args)
  print "Command: %s" % cmd_line
  try:
    if pipe:
      return subprocess.check_output(cmd_line, shell=True)
    else:
      return subprocess.check_call(cmd_line, shell=True)
  except subprocess.CalledProcessError:
    return None


# Wrapper for side effects.
class SideEffectHandler(object):
  def Call(self, fun, *args, **kwargs):
    return fun(*args, **kwargs)

  def Command(self, cmd, args="", prefix="", pipe=True):
    return Command(cmd, args, prefix, pipe)

  def ReadLine(self):
    return sys.stdin.readline().strip()

  def ReadURL(self, url, params=None):
    # pylint: disable=E1121
    url_fh = urllib2.urlopen(url, params, 60)
    try:
      return url_fh.read()
    finally:
      url_fh.close()

  def Sleep(self, seconds):
    time.sleep(seconds)

  def GetDate(self):
    return datetime.date.today().strftime("%Y-%m-%d")

DEFAULT_SIDE_EFFECT_HANDLER = SideEffectHandler()


class NoRetryException(Exception):
  pass


class CommonOptions(object):
  def __init__(self, options, manual=True):
    self.requires_editor = True
    self.wait_for_lgtm = True
    self.s = options.s
    self.force_readline_defaults = not manual
    self.force_upload = not manual
    self.manual = manual
    self.reviewer = getattr(options, 'reviewer', None)
    self.author = getattr(options, 'a', None)


class Step(object):
  def __init__(self, text, requires, number, config, state, options, handler):
    self._text = text
    self._requires = requires
    self._number = number
    self._config = config
    self._state = state
    self._options = options
    self._side_effect_handler = handler
    assert self._number >= 0
    assert self._config is not None
    assert self._state is not None
    assert self._side_effect_handler is not None
    assert isinstance(options, CommonOptions)

  def Config(self, key):
    return self._config[key]

  def Run(self):
    if self._requires:
      self.RestoreIfUnset(self._requires)
      if not self._state[self._requires]:
        return
    print ">>> Step %d: %s" % (self._number, self._text)
    self.RunStep()

  def RunStep(self):
    raise NotImplementedError

  def Retry(self, cb, retry_on=None, wait_plan=None):
    """ Retry a function.
    Params:
      cb: The function to retry.
      retry_on: A callback that takes the result of the function and returns
                True if the function should be retried. A function throwing an
                exception is always retried.
      wait_plan: A list of waiting delays between retries in seconds. The
                 maximum number of retries is len(wait_plan).
    """
    retry_on = retry_on or (lambda x: False)
    wait_plan = list(wait_plan or [])
    wait_plan.reverse()
    while True:
      got_exception = False
      try:
        result = cb()
      except NoRetryException, e:
        raise e
      except Exception:
        got_exception = True
      if got_exception or retry_on(result):
        if not wait_plan:
          raise Exception("Retried too often. Giving up.")
        wait_time = wait_plan.pop()
        print "Waiting for %f seconds." % wait_time
        self._side_effect_handler.Sleep(wait_time)
        print "Retrying..."
      else:
        return result

  def ReadLine(self, default=None):
    # Don't prompt in forced mode.
    if self._options.force_readline_defaults and default is not None:
      print "%s (forced)" % default
      return default
    else:
      return self._side_effect_handler.ReadLine()

  def Git(self, args="", prefix="", pipe=True, retry_on=None):
    cmd = lambda: self._side_effect_handler.Command("git", args, prefix, pipe)
    return self.Retry(cmd, retry_on, [5, 30])

  def SVN(self, args="", prefix="", pipe=True, retry_on=None):
    cmd = lambda: self._side_effect_handler.Command("svn", args, prefix, pipe)
    return self.Retry(cmd, retry_on, [5, 30])

  def Editor(self, args):
    if self._options.requires_editor:
      return self._side_effect_handler.Command(os.environ["EDITOR"], args,
                                               pipe=False)

  def ReadURL(self, url, params=None, retry_on=None, wait_plan=None):
    wait_plan = wait_plan or [3, 60, 600]
    cmd = lambda: self._side_effect_handler.ReadURL(url, params)
    return self.Retry(cmd, retry_on, wait_plan)

  def GetDate(self):
    return self._side_effect_handler.GetDate()

  def Die(self, msg=""):
    if msg != "":
      print "Error: %s" % msg
    print "Exiting"
    raise Exception(msg)

  def DieNoManualMode(self, msg=""):
    if not self._options.manual:
      msg = msg or "Only available in manual mode."
      self.Die(msg)

  def Confirm(self, msg):
    print "%s [Y/n] " % msg,
    answer = self.ReadLine(default="Y")
    return answer == "" or answer == "Y" or answer == "y"

  def DeleteBranch(self, name):
    git_result = self.Git("branch").strip()
    for line in git_result.splitlines():
      if re.match(r".*\s+%s$" % name, line):
        msg = "Branch %s exists, do you want to delete it?" % name
        if self.Confirm(msg):
          if self.Git("branch -D %s" % name) is None:
            self.Die("Deleting branch '%s' failed." % name)
          print "Branch %s deleted." % name
        else:
          msg = "Can't continue. Please delete branch %s and try again." % name
          self.Die(msg)

  def Persist(self, var, value):
    value = value or "__EMPTY__"
    TextToFile(value, "%s-%s" % (self._config[PERSISTFILE_BASENAME], var))

  def Restore(self, var):
    value = FileToText("%s-%s" % (self._config[PERSISTFILE_BASENAME], var))
    value = value or self.Die("Variable '%s' could not be restored." % var)
    return "" if value == "__EMPTY__" else value

  def RestoreIfUnset(self, var_name):
    if self._state.get(var_name) is None:
      self._state[var_name] = self.Restore(var_name)

  def InitialEnvironmentChecks(self):
    # Cancel if this is not a git checkout.
    if not os.path.exists(self._config[DOT_GIT_LOCATION]):
      self.Die("This is not a git checkout, this script won't work for you.")

    # Cancel if EDITOR is unset or not executable.
    if (self._options.requires_editor and (not os.environ.get("EDITOR") or
        Command("which", os.environ["EDITOR"]) is None)):
      self.Die("Please set your EDITOR environment variable, you'll need it.")

  def CommonPrepare(self):
    # Check for a clean workdir.
    if self.Git("status -s -uno").strip() != "":
      self.Die("Workspace is not clean. Please commit or undo your changes.")

    # Persist current branch.
    current_branch = ""
    git_result = self.Git("status -s -b -uno").strip()
    for line in git_result.splitlines():
      match = re.match(r"^## (.+)", line)
      if match:
        current_branch = match.group(1)
        break
    self.Persist("current_branch", current_branch)

    # Fetch unfetched revisions.
    if self.Git("svn fetch") is None:
      self.Die("'git svn fetch' failed.")

  def PrepareBranch(self):
    # Get ahold of a safe temporary branch and check it out.
    self.RestoreIfUnset("current_branch")
    if self._state["current_branch"] != self._config[TEMP_BRANCH]:
      self.DeleteBranch(self._config[TEMP_BRANCH])
      self.Git("checkout -b %s" % self._config[TEMP_BRANCH])

    # Delete the branch that will be created later if it exists already.
    self.DeleteBranch(self._config[BRANCHNAME])

  def CommonCleanup(self):
    self.RestoreIfUnset("current_branch")
    self.Git("checkout -f %s" % self._state["current_branch"])
    if self._config[TEMP_BRANCH] != self._state["current_branch"]:
      self.Git("branch -D %s" % self._config[TEMP_BRANCH])
    if self._config[BRANCHNAME] != self._state["current_branch"]:
      self.Git("branch -D %s" % self._config[BRANCHNAME])

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config[PERSISTFILE_BASENAME])

  def ReadAndPersistVersion(self, prefix=""):
    def ReadAndPersist(var_name, def_name):
      match = re.match(r"^#define %s\s+(\d*)" % def_name, line)
      if match:
        value = match.group(1)
        self.Persist("%s%s" % (prefix, var_name), value)
        self._state["%s%s" % (prefix, var_name)] = value
    for line in LinesInFile(self._config[VERSION_FILE]):
      for (var_name, def_name) in [("major", "MAJOR_VERSION"),
                                   ("minor", "MINOR_VERSION"),
                                   ("build", "BUILD_NUMBER"),
                                   ("patch", "PATCH_LEVEL")]:
        ReadAndPersist(var_name, def_name)

  def RestoreVersionIfUnset(self, prefix=""):
    for v in ["major", "minor", "build", "patch"]:
      self.RestoreIfUnset("%s%s" % (prefix, v))

  def WaitForLGTM(self):
    print ("Please wait for an LGTM, then type \"LGTM<Return>\" to commit "
           "your change. (If you need to iterate on the patch or double check "
           "that it's sane, do so in another shell, but remember to not "
           "change the headline of the uploaded CL.")
    answer = ""
    while answer != "LGTM":
      print "> ",
      answer = self.ReadLine(None if self._options.wait_for_lgtm else "LGTM")
      if answer != "LGTM":
        print "That was not 'LGTM'."

  def WaitForResolvingConflicts(self, patch_file):
    print("Applying the patch \"%s\" failed. Either type \"ABORT<Return>\", "
          "or resolve the conflicts, stage *all* touched files with "
          "'git add', and type \"RESOLVED<Return>\"")
    self.DieNoManualMode()
    answer = ""
    while answer != "RESOLVED":
      if answer == "ABORT":
        self.Die("Applying the patch failed.")
      if answer != "":
        print "That was not 'RESOLVED' or 'ABORT'."
      print "> ",
      answer = self.ReadLine()

  # Takes a file containing the patch to apply as first argument.
  def ApplyPatch(self, patch_file, reverse_patch=""):
    args = "apply --index --reject %s \"%s\"" % (reverse_patch, patch_file)
    if self.Git(args) is None:
      self.WaitForResolvingConflicts(patch_file)


class UploadStep(Step):
  MESSAGE = "Upload for code review."

  def RunStep(self):
    if self._options.reviewer:
      print "Using account %s for review." % self._options.reviewer
      reviewer = self._options.reviewer
    else:
      print "Please enter the email address of a V8 reviewer for your patch: ",
      self.DieNoManualMode("A reviewer must be specified in forced mode.")
      reviewer = self.ReadLine()
    author_option = self._options.author
    author = " --email \"%s\"" % author_option if author_option else ""
    force_flag = " -f" if self._options.force_upload else ""
    args = ("cl upload%s -r \"%s\" --send-mail%s"
            % (author, reviewer, force_flag))
    # TODO(machenbach): Check output in forced mode. Verify that all required
    # base files were uploaded, if not retry.
    if self.Git(args, pipe=False) is None:
      self.Die("'git cl upload' failed, please try again.")


def MakeStep(step_class=Step, number=0, state=None, config=None,
             options=None, side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER):
    # Allow to pass in empty dictionaries.
    state = state if state is not None else {}
    config = config if config is not None else {}

    try:
      message = step_class.MESSAGE
    except AttributeError:
      message = step_class.__name__
    try:
      requires = step_class.REQUIRES
    except AttributeError:
      requires = None

    return step_class(message, requires, number=number, config=config,
                      state=state, options=options,
                      handler=side_effect_handler)


def RunScript(step_classes,
              config,
              options,
              side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER):
  state = {}
  steps = []
  for (number, step_class) in enumerate(step_classes):
    steps.append(MakeStep(step_class, number, state, config,
                          options, side_effect_handler))

  for step in steps[options.s:]:
    step.Run()
