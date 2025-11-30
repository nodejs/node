# zoneinfo-test-gen

`zoneinfo-test-gen` is a tool for generating `zoneinfo` test data from
TZif data.

To use, run:

```
cargo run -p zoneinfo-test-gen <IANA_IDENTIFIER>
```

This tool will by default attempt to read from the local UNIX zoneinfo
directory, `/usr/share/zoneinfo`.

For specialized data, download a new tzdata from IANA by clicking on the
link below.

[IANA Download](https://data.iana.org/time-zones/tzdata-latest.tar.gz)

Once downloaded and extracted, new tzif files can be compiled with the
below command:

```
zic [OPTIONS] -d <OUTPUT_DIR> <PATH_TO_TZDATA_ZONEINFO_FILES>
```

Options must be set for the specific configuration that the test data
should be in.

NOTE: zoneinfo files are in `vanguard` by default.

From IANA's `tzdata` directory, switching zoneinfo files to rearguard
from vanguard can be completed by running:

```
awk -v DATAFORM=rearguard -f ziguard.awk <ZONEINFO_FILE> > output
```
