# Bit Index Error

This type marks that a value is out of range to be used as an index within an
`R` element. It is likely never produced, as `bitvec` does not construct invalid
indices, but is provided for completeness and to ensure that in the event of
this error occurring, the diagnostic information is useful.
