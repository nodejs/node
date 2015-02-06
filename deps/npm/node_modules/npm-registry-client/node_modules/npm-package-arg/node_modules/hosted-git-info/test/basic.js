"use strict"
var HostedGit = require("../index")
var test = require("tap").test

test("basic", function (t) {
   t.is(HostedGit.fromUrl("https://google.com"), undefined, "null on failure")

   t.end()
})
