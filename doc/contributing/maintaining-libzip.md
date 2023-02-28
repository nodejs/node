# Maintaining libzip

## Updating libzip

Update libzip:

```bash
mkdir -p temp temp/lib
cp deps/libzip/lib/{zip_err_str.c,zip_node_ext.c,config.h,zipconf.h} temp/lib/
cp deps/libzip/libzip.gyp temp/

git clone https://github.com/nih-at/libzip
rm -rf deps/libzip libzip/.git
mv libzip deps/

rsync -a temp/ deps/libzip
rm -rf temp
```

Check that Node.js still builds and tests. A couple of files may need to be
tweaked, depending on the changes upstream:

* the file list in `deps/libzip/libzip.gyp` may need to see entries added or
  removed

* the `deps/libzip/lib/{zip_err_str.c,config.h,zipconf.h}` files may need to be
  regenerated; in that case use CMake to rebuild them, then move the newly
  generated files in place of the checked-in ones.

## Committing libzip

Add libzip: `git add --all deps/libzip`

Commit the changes with a message like

```text
deps: update libzip to upstream d7f3ca9

Updated as described in doc/contributing/maintaining-libzip.md.
```
