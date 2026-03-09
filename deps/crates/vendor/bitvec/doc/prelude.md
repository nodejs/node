# Symbol Export

This module collects the general public API into a single place for bulk import,
as `use bitvec::prelude::*;`, without polluting the root namespace of the crate.

This provides all the data structure types and macros, as well as the two traits
needed to operate them as type parameters, by name. It also imports extension
traits without naming them, so that their methods are available but their trait
names are not.
