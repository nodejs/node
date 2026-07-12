target_sources(frozen-headers INTERFACE
  "${prefix}/frozen/algorithm.h"
  "${prefix}/frozen/map.h"
  "${prefix}/frozen/random.h"
  "${prefix}/frozen/set.h"
  "${prefix}/frozen/string.h"
  "${prefix}/frozen/unordered_map.h"
  "${prefix}/frozen/unordered_set.h"
  "${prefix}/frozen/bits/algorithms.h"
  "${prefix}/frozen/bits/basic_types.h"
  "${prefix}/frozen/bits/elsa.h"
  "${prefix}/frozen/bits/pmh.h")
