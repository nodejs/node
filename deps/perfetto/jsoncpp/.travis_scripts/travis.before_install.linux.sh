set -vex

# Preinstalled versions of python are dependent on which Ubuntu distribution
# you are running. The below version needs to be updated whenever we roll
# the Ubuntu version used in Travis.
# https://docs.travis-ci.com/user/languages/python/

pyenv global 3.7.1
