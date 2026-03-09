# Remote Destructor

`BitPtr` only points to indestructible types. This has no effect, and is only
present for symbol compatibility. You should not have been calling it on your
integers or `bool`s anyway!

## Original

[`ptr::drop_in_place`](core::ptr::drop_in_place)
