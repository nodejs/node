use std::env;
use std::fs;
use std::fs::File;
use std::path::Path;

use swc_sourcemap::ram_bundle::{split_ram_bundle, RamBundle, RamBundleType};
use swc_sourcemap::SourceMapIndex;

const USAGE: &str = "
Usage:
    ./split_ram_bundle RAM_BUNDLE SOURCEMAP OUT_DIRECTORY

This example app splits the given RAM bundle and the sourcemap into a set of
source files and their sourcemaps.

Both indexed and file RAM bundles are supported.
";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = env::args().collect();
    if args.len() < 4 {
        println!("{USAGE}");
        std::process::exit(1);
    }

    let bundle_path = Path::new(&args[1]);
    let ram_bundle = RamBundle::parse_indexed_from_path(bundle_path)
        .or_else(|_| RamBundle::parse_unbundle_from_path(bundle_path))?;

    match ram_bundle.bundle_type() {
        RamBundleType::Indexed => println!("Indexed RAM Bundle detected"),
        RamBundleType::Unbundle => println!("File RAM Bundle detected"),
    }

    let sourcemap_file = File::open(&args[2])?;
    let ism = SourceMapIndex::from_reader(sourcemap_file).unwrap();

    let output_directory = Path::new(&args[3]);
    if !output_directory.exists() {
        panic!("Directory {} does not exist!", output_directory.display());
    }

    println!(
        "Ouput directory: {}",
        output_directory.canonicalize()?.display()
    );
    let ram_bundle_iter = split_ram_bundle(&ram_bundle, &ism).unwrap();
    for result in ram_bundle_iter {
        let (name, sv, sm) = result.unwrap();
        println!("Writing down source: {name}");
        fs::write(output_directory.join(name.clone()), sv.source())?;

        let sourcemap_name = format!("{name}.map");
        println!("Writing down sourcemap: {sourcemap_name}");
        let out_sm = File::create(output_directory.join(sourcemap_name))?;
        sm.to_writer(out_sm)?;
    }
    println!("Done.");

    Ok(())
}
