workflow "New workflow" {
  on = "push"
  resolves = ["Find Python 3 syntax errors and undefined names"]
}

action "Find Python 3 syntax errors and undefined names" {
  uses = "cclauss/Find-Python-syntax-errors-action@master"
}
