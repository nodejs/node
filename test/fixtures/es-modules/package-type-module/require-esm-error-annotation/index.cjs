function main() {
  func1(func2(func3()))
}

function func1() {
  require('./app.js')
}
function func2() {}
function func3() {}

main()
