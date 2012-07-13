var read = require("../lib/read.js")

read({prompt: "Username: ", default: "test-user" }, function (er, user) {
  read({prompt: "Password: ", default: "test-pass", silent: true }, function (er, pass) {
    read({prompt: "Enter 4 characters: ", num: 4 }, function (er, four) {
      read({prompt: "Password again: ", default: "test-pass", silent: true }, function (er, pass2) {
        console.error({user: user,
                       pass: pass,
                       verify: pass2,
                       four:four,
                       passMatch: (pass === pass2)})
        console.error("If the program doesn't end right now,\n"
                     +"then you may be experiencing this bug:\n"
                     +"https://github.com/joyent/node/issues/2257")
      })
    })
  })
})
