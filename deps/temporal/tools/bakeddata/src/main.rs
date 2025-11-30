use databake::{quote, Bake};
use rustc_hash::FxHasher;
use std::{
    collections::{BTreeMap, BTreeSet, HashMap},
    fs::{self, File},
    hash::{Hash, Hasher},
    io::{self, BufWriter, Write},
    path::Path,
};
use timezone_provider::{
    experimental_tzif::{ZeroTzifULE, ZoneInfoProvider},
    IanaIdentifierNormalizer,
};
use zerovec::ule::VarULE;

trait BakedDataProvider {
    fn write_data(&self, data_path: &Path) -> io::Result<()>;

    fn write_debug(&self, debug_path: &Path) -> io::Result<()>;
}

impl BakedDataProvider for ZoneInfoProvider<'_> {
    fn write_data(&self, data_path: &Path) -> io::Result<()> {
        fs::create_dir_all(data_path)?;
        let generated_file = data_path.join("compiled_zoneinfo_provider.rs.data");
        let baked = self.bake(&Default::default());

        let baked_macro = quote! {
            #[macro_export]
            macro_rules! compiled_zoneinfo_provider {
                ($providername:ident) => {
                    pub const $providername: &'static timezone_provider::experimental_tzif::ZoneInfoProvider = &#baked;
                }
            }
        };
        let file = syn::parse_file(&baked_macro.to_string()).unwrap();
        let formatted = prettyplease::unparse(&file);
        let mut file = BufWriter::new(File::create(generated_file)?);
        write!(file, "//@generated\n// (by `bakeddata` binary in temporal_rs, using `databake`)\n\n{formatted}")
    }

    fn write_debug(&self, debug_path: &Path) -> io::Result<()> {
        let zoneinfo_debug_path = debug_path.join("zoneinfo");
        // Remove zoneinfo directory and recreate, so we can rely on diff of what is
        // changed / missing.
        if zoneinfo_debug_path.exists() {
            fs::remove_dir_all(zoneinfo_debug_path.clone())?;
        }
        // Recreate directory.
        fs::create_dir_all(zoneinfo_debug_path.clone())?;

        let map_file = zoneinfo_debug_path.join("map.json");

        // Create id sets for the tzifs
        let mut tzif_ids: HashMap<usize, BTreeSet<String>> = HashMap::new();
        for (identifier, index) in self.ids.to_btreemap().iter() {
            if let Some(id_set) = tzif_ids.get_mut(index) {
                id_set.insert(identifier.clone());
            } else {
                tzif_ids.insert(*index, BTreeSet::from([identifier.clone()]));
            }
        }

        let tzif_dir_path = zoneinfo_debug_path.join("tzifs");
        fs::create_dir_all(tzif_dir_path.clone())?;

        let mut id_map: BTreeMap<String, String> = BTreeMap::new();
        for (id, tzif) in self.tzifs.iter().enumerate() {
            let mut tzif_data = serde_json::Map::new();
            let id_set = tzif_ids.get(&id).unwrap();
            tzif_data.insert("ids".into(), serde_json::to_value(id_set)?);
            tzif_data.insert("tzif".into(), serde_json::to_value(tzif)?);
            let filename = format!("tzif-{}-{}.json", hash_ids(id_set), hash_tzif(tzif));
            let filepath = tzif_dir_path.join(filename.clone());
            for id in id_set {
                id_map.insert(id.clone(), filename.clone());
            }
            fs::write(filepath, serde_json::to_string_pretty(&tzif_data)?)?;
        }

        fs::write(
            map_file,
            format!("{}\n", serde_json::to_string_pretty(&id_map)?),
        )?;

        // TODO: Add version
        Ok(())
    }
}

fn hash_ids(set: &BTreeSet<String>) -> String {
    let mut hasher = FxHasher::default();
    set.hash(&mut hasher);
    format!("{:x}", hasher.finish())
}

fn hash_tzif(tzif: &ZeroTzifULE) -> String {
    let mut hasher = FxHasher::default();
    tzif.as_bytes().hash(&mut hasher);
    format!("{:x}", hasher.finish())
}

impl BakedDataProvider for IanaIdentifierNormalizer<'_> {
    fn write_data(&self, data_path: &Path) -> io::Result<()> {
        fs::create_dir_all(data_path)?;
        let generated_file = data_path.join("iana_normalizer.rs.data");
        let baked = self.bake(&Default::default());

        let baked_macro = quote! {
            #[macro_export]
            macro_rules! iana_normalizer_singleton {
                ($providername:ident) => {
                    pub const $providername: &'static timezone_provider::IanaIdentifierNormalizer = &#baked;
                }
            }
        };
        let file = syn::parse_file(&baked_macro.to_string()).unwrap();
        let formatted = prettyplease::unparse(&file);
        let mut file = BufWriter::new(File::create(generated_file)?);
        write!(file, "//@generated\n// (by `bakeddata` binary in temporal_rs, using `databake`)\n\n{formatted}")
    }

    fn write_debug(&self, debug_path: &Path) -> io::Result<()> {
        fs::create_dir_all(debug_path)?;
        let debug_filename = debug_path.join("iana_normalizer.json");
        let json = serde_json::to_string_pretty(self).unwrap();
        fs::write(debug_filename, json)
    }
}

fn write_data_file_with_debug(
    data_path: &Path,
    provider: &impl BakedDataProvider,
) -> io::Result<()> {
    let debug_path = data_path.join("debug");
    provider.write_debug(&debug_path)?;
    provider.write_data(data_path)
}

fn main() -> io::Result<()> {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    let tzdata_input = std::env::var("TZDATA_DIR").unwrap_or("tzdata".into());
    let tzdata_path = Path::new(&tzdata_input);
    let tzdata_dir = manifest_dir
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join(tzdata_path);
    println!("Using tzdata directory: {tzdata_dir:?}");

    let provider = Path::new(manifest_dir)
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("provider/src");
    println!("Using provider directory: {provider:?}");

    // Write identifiers
    write_data_file_with_debug(
        &provider.join("data"),
        &IanaIdentifierNormalizer::build(&tzdata_dir).unwrap(),
    )?;

    // Write tzif data
    write_data_file_with_debug(
        &provider.join("data"),
        &ZoneInfoProvider::build(&tzdata_dir).unwrap(),
    )
}
