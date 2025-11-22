//! Test for ensuring that we don't unintentionally add deps

use std::collections::BTreeSet;
use std::process::{self, Command};
use std::str;

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
struct DepSpec {
    crate_name: String,
    crate_version: String,
}

/// Get the deep (fully resolved) dependency list produced by `cargo tree -p {package} -e {edge_kind}`
fn get_dep_list(package: &str, edge_kind: &str, extra_args: &str) -> Vec<DepSpec> {
    let mut cmd = Command::new("cargo");
    cmd.arg("tree")
        .arg("-p")
        .arg(package)
        .arg("-e")
        .arg(edge_kind)
        .arg("--no-default-features");
    for arg in extra_args.split(' ') {
        if !arg.is_empty() {
            cmd.arg(arg);
        }
    }
    let output = cmd.output().expect("Failed to run `cargo tree`");

    if !output.status.success() {
        eprintln!("Failed to run `cargo tree -p {package} -e {edge_kind} --no-default-features {extra_args}`:");
        if let Ok(s) = str::from_utf8(&output.stderr) {
            eprintln!("{s}");
        }
        process::exit(1);
    }
    let mut spec: Vec<_> = output
        .stdout
        .split(|b| *b == b'\n')
        .filter_map(|slice| {
            if slice.is_empty() {
                return None;
            }
            if slice[0] == b'[' {
                // cargo tree output has sections like `[dev-dependencies]`
                return None;
            }

            let mut iter = slice.split(|b| *b == b' ');
            let mut found_crate_name = None;
            for section in &mut iter {
                if section.is_empty() {
                    continue;
                }
                // The format is {line drawing characters} {crate name} {crate version}
                if char::from(section[0]).is_ascii_alphabetic() {
                    found_crate_name =
                        Some(str::from_utf8(section).expect("Must be utf-8").to_owned());
                    break;
                }
            }
            if let Some(crate_name) = found_crate_name {
                let crate_version = iter
                    .next()
                    .expect("There must be a version after the crate name!");
                let crate_version = str::from_utf8(crate_version)
                    .expect("Must be utf-8")
                    .to_owned();
                Some(DepSpec {
                    crate_name,
                    crate_version,
                })
            } else {
                None
            }
        })
        .collect();
    spec.sort();
    spec.dedup();

    spec
}

/// Given a `cargo tree` invocation and the dependency sets to check, checks for any unlisted or duplicated deps
///
/// `dep_list_name_for_error` is the name of the const above to show in the error suggestion
fn test_dep_list(
    package: &str,
    edge_kind: &str,
    extra_args: &str,
    sets: &[&BTreeSet<&str>],
    dep_list_name_for_error: &str,
) {
    println!("Testing `cargo tree -p {package} -e {edge_kind} --no-default-features {extra_args}`");
    let mut errors = Vec::new();
    let dep_list = get_dep_list(package, edge_kind, extra_args);
    for i in dep_list.windows(2) {
        if i[0].crate_name == i[1].crate_name {
            errors.push(format!(
                "Found two versions for `{0}` ({1} & {2})",
                i[0].crate_name, i[0].crate_version, i[1].crate_version
            ));
        }
    }

    'dep_loop: for i in dep_list {
        if i.crate_name == package {
            continue;
        }
        let name = &i.crate_name;
        for s in sets {
            if s.contains(&**name) {
                continue 'dep_loop;
            }
        }
        errors.push(format!(
            "Found non-allowlisted crate `{name}`, consider adding to \
                             {dep_list_name_for_error} in depcheck/src/main.rs if intentional"
        ));
    }

    if !errors.is_empty() {
        eprintln!("Found invalid dependencies:");
        for e in errors {
            eprintln!("\t{e}");
        }
        process::exit(1);
    }
}

fn main() {
    let basic_runtime: BTreeSet<_> = BASIC_RUNTIME_DEPS.iter().copied().collect();
    let compiled_data: BTreeSet<_> = COMPILED_DEPS.iter().copied().collect();

    test_dep_list(
        "temporal_capi",
        "normal,no-proc-macro",
        "",
        &[&basic_runtime],
        "`BASIC_RUNTIME_DEPS`",
    );

    test_dep_list(
        "temporal_capi",
        "normal,no-proc-macro",
        "--features compiled_data",
        &[&basic_runtime, &compiled_data],
        "`COMPILED_DEPS`",
    );
}

/// Dependencies that are always allowed as runtime dependencies
///
pub const BASIC_RUNTIME_DEPS: &[&str] = &[
    // temporal_rs crates
    "temporal_rs",
    "timezone_provider",
    // ICU4X components and utils
    "calendrical_calculations",
    "core_maths",
    "diplomat-runtime",
    "icu_calendar",
    "icu_calendar_data",
    "icu_collections",
    "icu_locale",
    "icu_locale_core",
    "icu_locale_data",
    "icu_provider",
    "ixdtf",
    "litemap",
    "potential_utf",
    "tinystr",
    "writeable",
    "yoke",
    "zerofrom",
    "zerotrie",
    "zerovec",
    // Other deps
    "libm",
    "num-traits",
    "stable_deref_trait",
];

// Most of these should be removed
pub const COMPILED_DEPS: &[&str] = &[
    "bytes",
    "combine",
    "jiff-tzdb",
    "memchr",
    "tzif",
    "timezone_provider",
];
