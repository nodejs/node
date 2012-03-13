var envs = []
for (var e in process.env) {
  if (e.match(/npm|^path$/i)) envs.push(e + '=' + process.env[e])
}
envs.sort(function (a, b) {
  return a === b ? 0 : a > b ? -1 : 1
}).forEach(function (e) {
  console.log(e)
})
