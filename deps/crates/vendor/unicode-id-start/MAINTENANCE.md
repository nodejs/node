
To regenerate tables, run the following in the repo root (replace the unicode version):

```bash
cargo binstall ucd-generate
curl -LO https://www.unicode.org/Public/zipped/16.0.0/UCD.zip
unzip UCD.zip -d UCD
ucd-generate property-bool UCD --include ID_Start,ID_Continue > tests/tables/tables.rs
ucd-generate property-bool UCD --include ID_Start,ID_Continue --fst-dir tests/fst
ucd-generate property-bool UCD --include ID_Start,ID_Continue --trie-set > tests/trie/trie.rs
cargo run --manifest-path generate/Cargo.toml
```
