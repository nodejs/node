var test = require("tap").test

var vacuum = require("../vacuum.js")

test("vacuum errors when base is set and path is not under it", function (t) {
  vacuum("/a/made/up/path", {base : "/root/elsewhere"}, function (er) {
    t.ok(er, "got an error")
    t.equal(
      er.message,
      "/a/made/up/path is not a child of /root/elsewhere",
      "got the expected error message"
    )

    t.end()
  })
})
