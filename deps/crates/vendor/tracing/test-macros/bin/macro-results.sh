#!/bin/bash

mkdir -p results
echo "*" > results/.gitignore

echo "macro,directives,field_braces,surrounding,fmt_args,field_key,field_value,result" > results/result.csv

for test_file in $(ls tests/); do
    test_name=${test_file%".rs"}
    echo "$test_file -> $test_name"

    cargo check --test "${test_name}" 2>&1 \
        | perl -n -e 'if (/DEBUG:(.+)$/) { print "$1\n" }' \
        > results/failures-${test_name}.csv

    cat tests/${test_file} \
        | perl -n -e "if (/DEBUG:(.+)$/) { \$line = \$1; system(\"grep \'\$line\' results/failures-${test_name}.csv 2>&1 >/dev/null\"); my \$result = \$? == 0 ? 'fail' : 'pass'; print \"\$line,\$result\\n\" }" \
		> results/result-${test_name}.csv

	cat results/result-${test_name}.csv >> results/result.csv
done
