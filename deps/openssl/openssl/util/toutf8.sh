#! /bin/sh
#
# Very simple script to detect and convert files that we want to re-encode to UTF8

git ls-tree -r --name-only HEAD | \
    while read F; do
	charset=`file -bi "$F" | sed -e 's|.*charset=||'`
	if [ "$charset" != "utf-8" -a "$charset" != "binary" -a "$charset" != "us-ascii" ]; then
	    iconv -f ISO-8859-1 -t UTF8 < "$F" > "$F.utf8" && \
		( cmp -s "$F" "$F.utf8" || \
			( echo "$F"
			  mv "$F" "$F.iso-8859-1"
			  mv "$F.utf8" "$F"
			)
		)
	fi
    done
