workflow "Flake8" {
  on = "push"
  resolves = ["Find Python 3 syntax errors and undefined names"]
}

action "Find Python 3 syntax errors and undefined names" {
  uses = "cclauss/Find-Python-syntax-errors-action@master"
}

workflow "pylint" {
  on = "push"
  resolves = ["GitHub Action for pylint"]
}

action "GitHub Action for pylint" {
  uses = "cclauss/pylint/github_actions@GitHub-Action"
  args = "pylint"
}
