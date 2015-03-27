"use strict"
module.exports = function (verify, domain, shortname) {
  verify("https://" + domain + "/111/222", "https")
  verify("https://" + domain + "/111/222.git", "https.git")
  verify("https://" + domain + "/111/222#branch", "https#branch", "branch")
  verify("https://" + domain + "/111/222.git#branch", "https.git#branch", "branch")

  verify("git+https://" + domain + "/111/222", "git+https")
  verify("git+https://" + domain + "/111/222.git", "git+https.git")
  verify("git+https://" + domain + "/111/222#branch", "git+https#branch", "branch")
  verify("git+https://" + domain + "/111/222.git#branch", "git+https.git#branch", "branch")

  verify("git@" + domain + ":111/222", "ssh")
  verify("git@" + domain + ":111/222.git", "ssh.git")
  verify("git@" + domain + ":111/222#branch", "ssh", "branch")
  verify("git@" + domain + ":111/222.git#branch", "ssh.git", "branch")


  verify("git+ssh://git@" + domain + "/111/222", "ssh url")
  verify("git+ssh://git@" + domain + "/111/222.git", "ssh url.git")
  verify("git+ssh://git@" + domain + "/111/222#branch", "ssh url#branch", "branch")
  verify("git+ssh://git@" + domain + "/111/222.git#branch", "ssh url.git#branch", "branch")

  verify(shortname + ":111/222", "shortcut")
  verify(shortname + ":111/222.git", "shortcut.git")
  verify(shortname + ":111/222#branch", "shortcut#branch", "branch")
  verify(shortname + ":111/222.git#branch", "shortcut.git#branch", "branch")
}
