// request a tar file, and then write it
require("http").request({...}, function (resp) {
  resp.pipe(tar.createParser(function (file) {
    if (file.isDirectory()) {
      this.pause()
      return fs.mkdir(file.name, function (er) {
        if (er) return this.emit("error", er)
        this.resume()
      })
    } else if (file.isSymbolicLink()) {
      this.pause()
      return fs.symlink(file.link, file.name, function (er) {
        if (er) return this.emit("error", er)
        this.resume()
      })
    } else if (file.isFile()) {
      file.pipe(fs.createWriteStream(file.name))
    }
  }))
  // or maybe just have it do all that internally?
  resp.pipe(tar.createParser(function (file) {
    this.create("/extract/target/path", file)
  }))
})
