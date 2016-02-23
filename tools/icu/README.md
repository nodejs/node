Notes about the icu directory.
===

The files in this directory were written for the node.js effort. It's
the intent of their author (Steven R. Loomis / srl295) to merge them
upstream into ICU, pending much discussion within the ICU-PMC.

`icu_small.json` is somewhat node-specific as it specifies a "small ICU"
configuration file for the `icutrim.py` script. `icutrim.py` and
`iculslocs.cpp` may themselves be superseded by components built into
ICU in the future.

The following tickets were opened during this work, and their
resolution may inform the reader as to the current state of icu-trim
upstream:

   * [#10919](http://bugs.icu-project.org/trac/ticket/10919)
     (experimental branch - may copy any source patches here)
   * [#10922](http://bugs.icu-project.org/trac/ticket/10922)
     (data packaging improvements)
   * [#10923](http://bugs.icu-project.org/trac/ticket/10923)
     (rewrite data building in python)

When/if components (not including the `.json` file) are merged into
ICU, this code and `configure` will be updated to detect and use those
variants rather than the ones in this directory.
