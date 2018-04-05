#!/bin/bash
# This script applies the following patches
git cherry-pick c66c3d9fa3f5bab0bdfe363dd947136cf8a3907f
git cherry-pick 42a8de2ac66b6953cbc731fdb0b128b8019643b2
git cherry-pick 2eb170874aa5e84e71b62caab7ac9792fd59c10f
git cherry-pick --strategy=recursive -X theirs 664a659
