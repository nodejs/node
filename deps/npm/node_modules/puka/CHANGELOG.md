# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1](https://gitlab.com/rhendric/puka/-/compare/v1.0.0...v1.0.1) - 2020-05-16

### Fixed

- Add more carets to win32 command arguments ([45965ca](https://gitlab.com/rhendric/puka/-/commit/45965ca60fcc518082e0b085d8e81f3f3279ffb4))

    As previously documented and implemented, Puka assumed that all programs
    are batch files for the purpose of multi-escaping commands that appear
    in pipelines. However, regardless of whether a command is in a pipeline,
    one extra layer of escaping is needed if the command invokes a batch
    file, which Puka was not producing. This only applies to the arguments
    to the command, not to the batch file path, nor to paths used in
    redirects. (The property-based spawn test which was supposed to catch
    such oversights missed this one because it was invoking the Node.js
    executable directly, not, as recommended in the documentation, a batch
    file.)

    Going forward, the caveats described in the documentation continue to
    apply: if you are running programs on Windows with Puka, make sure they
    are batch files, or you may find arguments are being escaped with too
    many carets. As the documentation says, if this causes problems for you,
    please open an issue so we can work out the details of what a good
    workaround looks like.

## [1.0.0](https://gitlab.com/rhendric/puka/-/tags/v1.0.0) - 2017-09-29
