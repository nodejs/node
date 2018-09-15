## Contributing to the Guideline Support Library

The Guideline Support Library (GSL) contains functions and types that are suggested for use by the
[C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines). GSL design changes are made only as a result of modifications to the Guidelines. 

GSL is accepting contributions that improve or refine any of the types in this library as well as ports to other platforms. Changes should have an issue 
tracking the suggestion that has been approved by the maintainers. Your pull request should include a link to the bug that you are fixing. If you've submitted 
a PR, please post a comment in the associated issue to avoid duplication of effort.

## Legal
You will need to complete a Contributor License Agreement (CLA). Briefly, this agreement testifies that you are granting us and the community permission to 
use the submitted change according to the terms of the project's license, and that the work being submitted is under appropriate copyright.

Please submit a Contributor License Agreement (CLA) before submitting a pull request. You may visit https://cla.microsoft.com to sign digitally.

## Housekeeping
Your pull request should: 

* Include a description of what your change intends to do
* Be a child commit of a reasonably recent commit in the **master** branch 
    * Requests need not be a single commit, but should be a linear sequence of commits (i.e. no merge commits in your PR)
* It is desirable, but not necessary, for the tests to pass at each commit. Please see [README.md](./README.md) for instructions to build the test suite. 
* Have clear commit messages 
    * e.g. "Fix issue", "Add tests for type", etc.
* Include appropriate tests 
    * Tests should include reasonable permutations of the target fix/change
    * Include baseline changes with your change
    * All changed code must have 100% code coverage
* To avoid line ending issues, set `autocrlf = input` and `whitespace = cr-at-eol` in your git configuration
