"use strict"
var HostedGit = require("../index")
var test = require("tap").test


test("fromUrl(gitlab url)", function (t) {
  function verify(host, label, branch) {
    var hostinfo = HostedGit.fromUrl(host)
    var hash = branch ? "#" + branch : ""
    t.ok(hostinfo, label)
    if (! hostinfo) return
    t.is( hostinfo.https(), "https://gitlab.com/111/222.git" + hash, label + " -> https" )
    t.is( hostinfo.browse(), "https://gitlab.com/111/222" + (branch ? "/tree/" + branch : ""), label + " -> browse" )
    t.is( hostinfo.docs(), "https://gitlab.com/111/222" + (branch ? "/tree/" + branch : "") + "#README", label + " -> docs" )
    t.is( hostinfo.ssh(), "git@gitlab.com:111/222.git" + hash, label + " -> ssh" )
    t.is( hostinfo.sshurl(), "git+ssh://git@gitlab.com/111/222.git" + hash, label + " -> sshurl" )
    t.is( (""+hostinfo), "git+ssh://git@gitlab.com/111/222.git" + hash, label + " -> stringify" )
    t.is( hostinfo.file("C"), "https://gitlab.com/111/222/raw/"+(branch||"master")+"/C", label + " -> file" )
  }

  require('./lib/standard-tests')(verify, "gitlab.com", "gitlab")

  t.end()
})

