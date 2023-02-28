# libzip API changes

This file describes changes in the libzip API and how to adapt your
code for them.

You can define `ZIP_DISABLE_DEPRECATED` before including `<zip.h>` to hide
prototypes for deprecated functions, to find out about functions that
might be removed at some point.

## Changed in libzip-1.0

### new type `zip_error_t`

Error information is stored in the newly public type `zip_error_t`. Use
this to access information about an error, instead of the deprecated
functions that operated on two ints.

deprecated functions:
- `zip_error_get_sys_type()`
- `zip_error_get()`
- `zip_error_to_str()`
- `zip_file_error_get()`

See their man pages for instructions on how to replace them.

The most common affected use is `zip_open`. The new recommended usage
is:

```c
int err;
if ((za = zip_open(archive, flags, &err)) == NULL) {
	zip_error_t error;
	zip_error_init_with_code(&error, err);
	fprintf(stderr, "can't open zip archive '%s': %s\n", archive, zip_error_strerror(&error));
	zip_error_fini(&error);
}
```

### more typedefs

The following typedefs have been added for better readability:

```c
typedef struct zip zip_t;
typedef struct zip_file zip_file_t;
typedef struct zip_source zip_source_t;
typedef struct zip_stat zip_stat_t;
```

This means you can use "`zip_t`" instead of "`struct zip`", etc.


### torrentzip support removed

torrentzip depends on a particular zlib version which is by now quite
old.

## Changed in libzip-0.11

### new type `zip_flags_t`

The functions which have flags now use the `zip_flags_t` type for this.
All old flags fit; you need only to adapt code if you were saving flags in a
local variable. Use `zip_flags_t` for such a variable.
This affects:
- `zip_fopen()`
- `zip_fopen_encrypted()`
- `zip_fopen_index()`
- `zip_fopen_index_encrypted()`
- `zip_get_archive_comment()`
- `zip_get_archive_flag()`
- `zip_get_num_entries()`
- `zip_get_name()`
- `zip_name_locate()`
- `zip_set_archive_flag()`
- `zip_source_zip()`
- `zip_stat()`
- `zip_stat_index()`

#### `ZIP_FL_*`, `ZIP_AFL_*`, `ZIP_STAT_*` are now unsigned constants

To match the new `zip_flags_t` type.

#### `zip_add()`, `zip_add_dir()`

These functions were replaced with `zip_file_add()` and `zip_dir_add()`, respectively,
to add a flags argument.

#### `zip_rename()`, `zip_replace()`

These functions were replaced with `zip_file_rename()` and `zip_file_replace()`,
respectively, to add a flags argument.

#### `zip_get_file_comment()`

This function was replaced with `zip_file_get_comment()`; one argument was promoted from
`int` to `zip_uint32_t`, the other is now a `zip_flags_t`.

#### `zip_set_file_comment()`

This function was replaced with `zip_file_set_comment()`; an argument was promoted from
`int` to `zip_uint16_t`, and a `zip_flags_t` argument was added.

### integer type size changes

Some argument and return values were not the right size or sign.

#### `zip_name_locate()`

The return value was `int`, which can be too small. The function now returns `zip_int64_t`.


#### `zip_get_num_entries()`

The return type is now signed, to allow signaling errors.

#### `zip_set_archive_comment()`

The last argument changed from `int` to `zip_uint16_t`.

### extra field handling rewritten

The `zip_get_file_extra()` and `zip_set_file_extra()` functions were removed.
They only worked on the whole extra field set.

Instead, you can now set, get, count, and delete each extra field separately,
using the functions:
- `zip_file_extra_field_delete()`
- `zip_file_extra_field_delete_by_id()`
- `zip_file_extra_field_get()`
- `zip_file_extra_field_get_by_id()`
- `zip_file_extra_fields_count()`
- `zip_file_extra_fields_count_by_id()`
- `zip_file_extra_field_set()`

Please read the corresponding man pages for details.

### new functions

#### `zip_discard()`

The new `zip_discard()` function closes an archive without committing the
scheduled changes.

#### `zip_set_file_compression()`

The new `zip_set_file_compression()` function allows setting compression
levels for files.

### argument changes

#### file names

File names arguments are now allowed to be `NULL` to have an empty file name.
This mostly affects `zip_file_add()`, `zip_dir_add()`, and `zip_file_rename()`.

For `zip_get_name()`, `zip_file_get_comment()`, and `zip_get_archive_comment()`, if
the file name or comment is empty, a string of length 0 is returned.
`NULL` is returned for errors only.

Previously, `NULL` was returned for empty/unset file names and comments and
errors, leaving no way to differentiate between the two.
