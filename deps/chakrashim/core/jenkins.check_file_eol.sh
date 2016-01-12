#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

# We will run dos2unix on the argument and make sure that it doesn't change.
# If it changes, that means someone introduced a CRLF by not setting core.autocrlf to true.

ERRFILE=jenkins.check_eol.sh.err
ERRFILETEMP=$ERRFILE.0

# display a helpful message for someone reading the log
echo "Check EOL > Checking $1"

if [ ! -e $1 ]; then # the file wasn't present; not necessarily an error
    echo "WARNING: file not found: $1"
    exit 0 # don't report an error but don't run the rest of this file
fi

# We can't rely on dos2unix being installed, so simply grep for the CR octet 0x0d via xxd.
# We don't want to simply detect a literal 0d in the file or output so configure xxd to emit
# octets in such a way that we can grep for the CR octet and not accidentally detect
# text of the file or 0d spanning 2 octets in xxd output, e.g., 20d1 (' Ñ').
xxd -i -c 16 $1 | grep '0x0d' > $ERRFILETEMP
if [ $? -eq 0 ]; then # grep found matches ($?==0), so we found CR (0x0d) in the file
    echo "ERROR: CR (0x0d) was introduced in $1" >> $ERRFILE

    # Display a user-readable hex dump for context of the problem.
    # Don't pollute the log with every single matching line, first 10 lines should be enough.
    echo "Displaying first 10 lines of hex dump where CR (0x0d) was found:" >> $ERRFILE
    xxd -g 1 $1 | grep -n '0d ' > $ERRFILETEMP
    head -n 10 $ERRFILETEMP >> $ERRFILE

    # To help the user, display how many lines of hex output actually contained CR.
    LINECOUNT=`python -c "file=open('$ERRFILETEMP', 'r'); print len(file.readlines())"`
    echo "Total hex dump lines containing CR (0x0d): $LINECOUNT" >> $ERRFILE
    echo "--------------" >> $ERRFILE # same length as '--- ERRORS ---'
fi

rm -f $ERRFILETEMP
