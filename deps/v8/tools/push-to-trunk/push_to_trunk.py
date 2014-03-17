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

import optparse
import sys
import tempfile
import urllib2

from common_includes import *

TRUNKBRANCH = "TRUNKBRANCH"
CHROMIUM = "CHROMIUM"
DEPS_FILE = "DEPS_FILE"

CONFIG = {
  BRANCHNAME: "prepare-push",
  TRUNKBRANCH: "trunk-push",
  PERSISTFILE_BASENAME: "/tmp/v8-push-to-trunk-tempfile",
  TEMP_BRANCH: "prepare-push-temporary-branch-created-by-script",
  DOT_GIT_LOCATION: ".git",
  VERSION_FILE: "src/version.cc",
  CHANGELOG_FILE: "ChangeLog",
  CHANGELOG_ENTRY_FILE: "/tmp/v8-push-to-trunk-tempfile-changelog-entry",
  PATCH_FILE: "/tmp/v8-push-to-trunk-tempfile-patch-file",
  COMMITMSG_FILE: "/tmp/v8-push-to-trunk-tempfile-commitmsg",
  DEPS_FILE: "DEPS",
}


class PushToTrunkOptions(CommonOptions):
  @staticmethod
  def MakeForcedOptions(reviewer, chrome_path):
    """Convenience wrapper."""
    class Options(object):
      pass
    options = Options()
    options.s = 0
    options.l = None
    options.f = True
    options.m = False
    options.r = reviewer
    options.c = chrome_path
    return PushToTrunkOptions(options)

  def __init__(self, options):
    super(PushToTrunkOptions, self).__init__(options, options.m)
    self.requires_editor = not options.f
    self.wait_for_lgtm = not options.f
    self.tbr_commit = not options.m
    self.l = options.l
    self.r = options.r
    self.c = options.c

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.InitialEnvironmentChecks()
    self.CommonPrepare()
    self.PrepareBranch()
    self.DeleteBranch(self.Config(TRUNKBRANCH))


class FreshBranch(Step):
  MESSAGE = "Create a fresh branch."

  def RunStep(self):
    args = "checkout -b %s svn/bleeding_edge" % self.Config(BRANCHNAME)
    if self.Git(args) is None:
      self.Die("Creating branch %s failed." % self.Config(BRANCHNAME))


class DetectLastPush(Step):
  MESSAGE = "Detect commit ID of last push to trunk."

  def RunStep(self):
    last_push = (self._options.l or
                 self.Git("log -1 --format=%H ChangeLog").strip())
    while True:
      # Print assumed commit, circumventing git's pager.
      print self.Git("log -1 %s" % last_push)
      if self.Confirm("Is the commit printed above the last push to trunk?"):
        break
      args = "log -1 --format=%H %s^ ChangeLog" % last_push
      last_push = self.Git(args).strip()
    self.Persist("last_push", last_push)
    self._state["last_push"] = last_push


class PrepareChangeLog(Step):
  MESSAGE = "Prepare raw ChangeLog entry."

  def Reload(self, body):
    """Attempts to reload the commit message from rietveld in order to allow
    late changes to the LOG flag. Note: This is brittle to future changes of
    the web page name or structure.
    """
    match = re.search(r"^Review URL: https://codereview\.chromium\.org/(\d+)$",
                      body, flags=re.M)
    if match:
      cl_url = "https://codereview.chromium.org/%s/description" % match.group(1)
      try:
        # Fetch from Rietveld but only retry once with one second delay since
        # there might be many revisions.
        body = self.ReadURL(cl_url, wait_plan=[1])
      except urllib2.URLError:
        pass
    return body

  def RunStep(self):
    self.RestoreIfUnset("last_push")

    # These version numbers are used again later for the trunk commit.
    self.ReadAndPersistVersion()

    date = self.GetDate()
    self.Persist("date", date)
    output = "%s: Version %s.%s.%s\n\n" % (date,
                                           self._state["major"],
                                           self._state["minor"],
                                           self._state["build"])
    TextToFile(output, self.Config(CHANGELOG_ENTRY_FILE))

    args = "log %s..HEAD --format=%%H" % self._state["last_push"]
    commits = self.Git(args).strip()

    # Cache raw commit messages.
    commit_messages = [
      [
        self.Git("log -1 %s --format=\"%%s\"" % commit),
        self.Reload(self.Git("log -1 %s --format=\"%%B\"" % commit)),
        self.Git("log -1 %s --format=\"%%an\"" % commit),
      ] for commit in commits.splitlines()
    ]

    # Auto-format commit messages.
    body = MakeChangeLogBody(commit_messages, auto_format=True)
    AppendToFile(body, self.Config(CHANGELOG_ENTRY_FILE))

    msg = ("        Performance and stability improvements on all platforms."
           "\n#\n# The change log above is auto-generated. Please review if "
           "all relevant\n# commit messages from the list below are included."
           "\n# All lines starting with # will be stripped.\n#\n")
    AppendToFile(msg, self.Config(CHANGELOG_ENTRY_FILE))

    # Include unformatted commit messages as a reference in a comment.
    comment_body = MakeComment(MakeChangeLogBody(commit_messages))
    AppendToFile(comment_body, self.Config(CHANGELOG_ENTRY_FILE))


class EditChangeLog(Step):
  MESSAGE = "Edit ChangeLog entry."

  def RunStep(self):
    print ("Please press <Return> to have your EDITOR open the ChangeLog "
           "entry, then edit its contents to your liking. When you're done, "
           "save the file and exit your EDITOR. ")
    self.ReadLine(default="")
    self.Editor(self.Config(CHANGELOG_ENTRY_FILE))
    handle, new_changelog = tempfile.mkstemp()
    os.close(handle)

    # Strip comments and reformat with correct indentation.
    changelog_entry = FileToText(self.Config(CHANGELOG_ENTRY_FILE)).rstrip()
    changelog_entry = StripComments(changelog_entry)
    changelog_entry = "\n".join(map(Fill80, changelog_entry.splitlines()))
    changelog_entry = changelog_entry.lstrip()

    if changelog_entry == "":
      self.Die("Empty ChangeLog entry.")

    with open(new_changelog, "w") as f:
      f.write(changelog_entry)
      f.write("\n\n\n")  # Explicitly insert two empty lines.

    AppendToFile(FileToText(self.Config(CHANGELOG_FILE)), new_changelog)
    TextToFile(FileToText(new_changelog), self.Config(CHANGELOG_FILE))
    os.remove(new_changelog)


class IncrementVersion(Step):
  MESSAGE = "Increment version number."

  def RunStep(self):
    self.RestoreIfUnset("build")
    new_build = str(int(self._state["build"]) + 1)

    if self.Confirm(("Automatically increment BUILD_NUMBER? (Saying 'n' will "
                     "fire up your EDITOR on %s so you can make arbitrary "
                     "changes. When you're done, save the file and exit your "
                     "EDITOR.)" % self.Config(VERSION_FILE))):
      text = FileToText(self.Config(VERSION_FILE))
      text = MSub(r"(?<=#define BUILD_NUMBER)(?P<space>\s+)\d*$",
                  r"\g<space>%s" % new_build,
                  text)
      TextToFile(text, self.Config(VERSION_FILE))
    else:
      self.Editor(self.Config(VERSION_FILE))

    self.ReadAndPersistVersion("new_")


class CommitLocal(Step):
  MESSAGE = "Commit to local branch."

  def RunStep(self):
    self.RestoreVersionIfUnset("new_")
    prep_commit_msg = ("Prepare push to trunk.  "
        "Now working on version %s.%s.%s." % (self._state["new_major"],
                                              self._state["new_minor"],
                                              self._state["new_build"]))
    self.Persist("prep_commit_msg", prep_commit_msg)

    # Include optional TBR only in the git command. The persisted commit
    # message is used for finding the commit again later.
    review = "\n\nTBR=%s" % self._options.r if self._options.tbr_commit else ""
    if self.Git("commit -a -m \"%s%s\"" % (prep_commit_msg, review)) is None:
      self.Die("'git commit -a' failed.")


class CommitRepository(Step):
  MESSAGE = "Commit to the repository."

  def RunStep(self):
    self.WaitForLGTM()
    # Re-read the ChangeLog entry (to pick up possible changes).
    # FIXME(machenbach): This was hanging once with a broken pipe.
    TextToFile(GetLastChangeLogEntries(self.Config(CHANGELOG_FILE)),
               self.Config(CHANGELOG_ENTRY_FILE))

    if self.Git("cl presubmit", "PRESUBMIT_TREE_CHECK=\"skip\"") is None:
      self.Die("'git cl presubmit' failed, please try again.")

    if self.Git("cl dcommit -f --bypass-hooks",
                retry_on=lambda x: x is None) is None:
      self.Die("'git cl dcommit' failed, please try again.")


class StragglerCommits(Step):
  MESSAGE = ("Fetch straggler commits that sneaked in since this script was "
             "started.")

  def RunStep(self):
    if self.Git("svn fetch") is None:
      self.Die("'git svn fetch' failed.")
    self.Git("checkout svn/bleeding_edge")
    self.RestoreIfUnset("prep_commit_msg")
    args = "log -1 --format=%%H --grep=\"%s\"" % self._state["prep_commit_msg"]
    prepare_commit_hash = self.Git(args).strip()
    self.Persist("prepare_commit_hash", prepare_commit_hash)


class SquashCommits(Step):
  MESSAGE = "Squash commits into one."

  def RunStep(self):
    # Instead of relying on "git rebase -i", we'll just create a diff, because
    # that's easier to automate.
    self.RestoreIfUnset("prepare_commit_hash")
    args = "diff svn/trunk %s" % self._state["prepare_commit_hash"]
    TextToFile(self.Git(args), self.Config(PATCH_FILE))

    # Convert the ChangeLog entry to commit message format.
    self.RestoreIfUnset("date")
    text = FileToText(self.Config(CHANGELOG_ENTRY_FILE))

    # Remove date and trailing white space.
    text = re.sub(r"^%s: " % self._state["date"], "", text.rstrip())

    # Retrieve svn revision for showing the used bleeding edge revision in the
    # commit message.
    args = "svn find-rev %s" % self._state["prepare_commit_hash"]
    svn_revision = self.Git(args).strip()
    self.Persist("svn_revision", svn_revision)
    text = MSub(r"^(Version \d+\.\d+\.\d+)$",
                "\\1 (based on bleeding_edge revision r%s)" % svn_revision,
                text)

    # Remove indentation and merge paragraphs into single long lines, keeping
    # empty lines between them.
    def SplitMapJoin(split_text, fun, join_text):
      return lambda text: join_text.join(map(fun, text.split(split_text)))
    strip = lambda line: line.strip()
    text = SplitMapJoin("\n\n", SplitMapJoin("\n", strip, " "), "\n\n")(text)

    if not text:
      self.Die("Commit message editing failed.")
    TextToFile(text, self.Config(COMMITMSG_FILE))
    os.remove(self.Config(CHANGELOG_ENTRY_FILE))


class NewBranch(Step):
  MESSAGE = "Create a new branch from trunk."

  def RunStep(self):
    if self.Git("checkout -b %s svn/trunk" % self.Config(TRUNKBRANCH)) is None:
      self.Die("Checking out a new branch '%s' failed." %
               self.Config(TRUNKBRANCH))


class ApplyChanges(Step):
  MESSAGE = "Apply squashed changes."

  def RunStep(self):
    self.ApplyPatch(self.Config(PATCH_FILE))
    Command("rm", "-f %s*" % self.Config(PATCH_FILE))


class SetVersion(Step):
  MESSAGE = "Set correct version for trunk."

  def RunStep(self):
    self.RestoreVersionIfUnset()
    output = ""
    for line in FileToText(self.Config(VERSION_FILE)).splitlines():
      if line.startswith("#define MAJOR_VERSION"):
        line = re.sub("\d+$", self._state["major"], line)
      elif line.startswith("#define MINOR_VERSION"):
        line = re.sub("\d+$", self._state["minor"], line)
      elif line.startswith("#define BUILD_NUMBER"):
        line = re.sub("\d+$", self._state["build"], line)
      elif line.startswith("#define PATCH_LEVEL"):
        line = re.sub("\d+$", "0", line)
      elif line.startswith("#define IS_CANDIDATE_VERSION"):
        line = re.sub("\d+$", "0", line)
      output += "%s\n" % line
    TextToFile(output, self.Config(VERSION_FILE))


class CommitTrunk(Step):
  MESSAGE = "Commit to local trunk branch."

  def RunStep(self):
    self.Git("add \"%s\"" % self.Config(VERSION_FILE))
    if self.Git("commit -F \"%s\"" % self.Config(COMMITMSG_FILE)) is None:
      self.Die("'git commit' failed.")
    Command("rm", "-f %s*" % self.Config(COMMITMSG_FILE))


class SanityCheck(Step):
  MESSAGE = "Sanity check."

  def RunStep(self):
    if not self.Confirm("Please check if your local checkout is sane: Inspect "
        "%s, compile, run tests. Do you want to commit this new trunk "
        "revision to the repository?" % self.Config(VERSION_FILE)):
      self.Die("Execution canceled.")


class CommitSVN(Step):
  MESSAGE = "Commit to SVN."

  def RunStep(self):
    result = self.Git("svn dcommit 2>&1", retry_on=lambda x: x is None)
    if not result:
      self.Die("'git svn dcommit' failed.")
    result = filter(lambda x: re.search(r"^Committed r[0-9]+", x),
                    result.splitlines())
    if len(result) > 0:
      trunk_revision = re.sub(r"^Committed r([0-9]+)", r"\1", result[0])

    # Sometimes grepping for the revision fails. No idea why. If you figure
    # out why it is flaky, please do fix it properly.
    if not trunk_revision:
      print("Sorry, grepping for the SVN revision failed. Please look for it "
            "in the last command's output above and provide it manually (just "
            "the number, without the leading \"r\").")
      self.DieNoManualMode("Can't prompt in forced mode.")
      while not trunk_revision:
        print "> ",
        trunk_revision = self.ReadLine()
    self.Persist("trunk_revision", trunk_revision)


class TagRevision(Step):
  MESSAGE = "Tag the new revision."

  def RunStep(self):
    self.RestoreVersionIfUnset()
    ver = "%s.%s.%s" % (self._state["major"],
                        self._state["minor"],
                        self._state["build"])
    if self.Git("svn tag %s -m \"Tagging version %s\"" % (ver, ver),
                retry_on=lambda x: x is None) is None:
      self.Die("'git svn tag' failed.")


class CheckChromium(Step):
  MESSAGE = "Ask for chromium checkout."

  def Run(self):
    chrome_path = self._options.c
    if not chrome_path:
      self.DieNoManualMode("Please specify the path to a Chromium checkout in "
                          "forced mode.")
      print ("Do you have a \"NewGit\" Chromium checkout and want "
          "this script to automate creation of the roll CL? If yes, enter the "
          "path to (and including) the \"src\" directory here, otherwise just "
          "press <Return>: "),
      chrome_path = self.ReadLine()
    self.Persist("chrome_path", chrome_path)


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."
  REQUIRES = "chrome_path"

  def RunStep(self):
    v8_path = os.getcwd()
    self.Persist("v8_path", v8_path)
    os.chdir(self._state["chrome_path"])
    self.InitialEnvironmentChecks()
    # Check for a clean workdir.
    if self.Git("status -s -uno").strip() != "":
      self.Die("Workspace is not clean. Please commit or undo your changes.")
    # Assert that the DEPS file is there.
    if not os.path.exists(self.Config(DEPS_FILE)):
      self.Die("DEPS file not present.")


class UpdateChromiumCheckout(Step):
  MESSAGE = "Update the checkout and create a new branch."
  REQUIRES = "chrome_path"

  def RunStep(self):
    os.chdir(self._state["chrome_path"])
    if self.Git("checkout master") is None:
      self.Die("'git checkout master' failed.")
    if self.Git("pull") is None:
      self.Die("'git pull' failed, please try again.")

    self.RestoreIfUnset("trunk_revision")
    args = "checkout -b v8-roll-%s" % self._state["trunk_revision"]
    if self.Git(args) is None:
      self.Die("Failed to checkout a new branch.")


class UploadCL(Step):
  MESSAGE = "Create and upload CL."
  REQUIRES = "chrome_path"

  def RunStep(self):
    os.chdir(self._state["chrome_path"])

    # Patch DEPS file.
    self.RestoreIfUnset("trunk_revision")
    deps = FileToText(self.Config(DEPS_FILE))
    deps = re.sub("(?<=\"v8_revision\": \")([0-9]+)(?=\")",
                  self._state["trunk_revision"],
                  deps)
    TextToFile(deps, self.Config(DEPS_FILE))

    self.RestoreVersionIfUnset()
    ver = "%s.%s.%s" % (self._state["major"],
                        self._state["minor"],
                        self._state["build"])
    if self._options.r:
      print "Using account %s for review." % self._options.r
      rev = self._options.r
    else:
      print "Please enter the email address of a reviewer for the roll CL: ",
      self.DieNoManualMode("A reviewer must be specified in forced mode.")
      rev = self.ReadLine()
    self.RestoreIfUnset("svn_revision")
    args = ("commit -am \"Update V8 to version %s "
            "(based on bleeding_edge revision r%s).\n\nTBR=%s\""
            % (ver, self._state["svn_revision"], rev))
    if self.Git(args) is None:
      self.Die("'git commit' failed.")
    force_flag = " -f" if self._options.force_upload else ""
    if self.Git("cl upload --send-mail%s" % force_flag, pipe=False) is None:
      self.Die("'git cl upload' failed, please try again.")
    print "CL uploaded."


class SwitchV8(Step):
  MESSAGE = "Returning to V8 checkout."
  REQUIRES = "chrome_path"

  def RunStep(self):
    self.RestoreIfUnset("v8_path")
    os.chdir(self._state["v8_path"])


class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    self.RestoreVersionIfUnset()
    ver = "%s.%s.%s" % (self._state["major"],
                        self._state["minor"],
                        self._state["build"])
    self.RestoreIfUnset("trunk_revision")
    self.RestoreIfUnset("chrome_path")

    if self._state["chrome_path"]:
      print("Congratulations, you have successfully created the trunk "
            "revision %s and rolled it into Chromium. Please don't forget to "
            "update the v8rel spreadsheet:" % ver)
    else:
      print("Congratulations, you have successfully created the trunk "
            "revision %s. Please don't forget to roll this new version into "
            "Chromium, and to update the v8rel spreadsheet:" % ver)
    print "%s\ttrunk\t%s" % (ver, self._state["trunk_revision"])

    self.CommonCleanup()
    if self.Config(TRUNKBRANCH) != self._state["current_branch"]:
      self.Git("branch -D %s" % self.Config(TRUNKBRANCH))


def RunPushToTrunk(config,
                   options,
                   side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER):
  step_classes = [
    Preparation,
    FreshBranch,
    DetectLastPush,
    PrepareChangeLog,
    EditChangeLog,
    IncrementVersion,
    CommitLocal,
    UploadStep,
    CommitRepository,
    StragglerCommits,
    SquashCommits,
    NewBranch,
    ApplyChanges,
    SetVersion,
    CommitTrunk,
    SanityCheck,
    CommitSVN,
    TagRevision,
    CheckChromium,
    SwitchChromium,
    UpdateChromiumCheckout,
    UploadCL,
    SwitchV8,
    CleanUp,
  ]

  RunScript(step_classes, config, options, side_effect_handler)


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("-c", "--chromium", dest="c",
                    help=("Specify the path to your Chromium src/ "
                          "directory to automate the V8 roll."))
  result.add_option("-f", "--force", dest="f",
                    help="Don't prompt the user.",
                    default=False, action="store_true")
  result.add_option("-l", "--last-push", dest="l",
                    help=("Manually specify the git commit ID "
                          "of the last push to trunk."))
  result.add_option("-m", "--manual", dest="m",
                    help="Prompt the user at every important step.",
                    default=False, action="store_true")
  result.add_option("-r", "--reviewer", dest="r",
                    help=("Specify the account name to be used for reviews."))
  result.add_option("-s", "--step", dest="s",
                    help="Specify the step where to start work. Default: 0.",
                    default=0, type="int")
  return result


def ProcessOptions(options):
  if options.s < 0:
    print "Bad step number %d" % options.s
    return False
  if not options.m and not options.r:
    print "A reviewer (-r) is required in (semi-)automatic mode."
    return False
  if options.f and options.m:
    print "Manual and forced mode cannot be combined."
    return False
  if not options.m and not options.c:
    print "A chromium checkout (-c) is required in (semi-)automatic mode."
    return False
  return True


def Main():
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    return 1
  RunPushToTrunk(CONFIG, PushToTrunkOptions(options))

if __name__ == "__main__":
  sys.exit(Main())
