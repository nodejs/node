# -------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
# -------------------------------------------------------------------------------------------------------
#
if(@ARGV == 0 || (@ARGV == 1 && $ARGV[0] =~ /[-\/]\?/))
{
    print "Usage: perl perf.pl [options]\n\n";
    print "First generate baseline run for all the perf benchmarks using:\n";
    print "perl perf.pl -baseline -binary:<basepath>\\ch.exe\n\n";
    print "Then run the perf benchmarks with your changed binary:\n";
    print "perl perf.pl -binary:<pullrequestfilepath>\\ch.exe\n";
    print "Use perftest.pl for advanced options\n";
    exit(1);
}
else
{
    #fail if unable to run any of the benchmarks
    if (system("perl perftest.pl -octane @ARGV"))
    {
       exit(1);
    }
    
    if (system("perl perftest.pl -kraken @ARGV"))
    {
       exit(1);
    }

    if (system("perl perftest.pl -sunspider @ARGV"))
    {
       exit(1);
    }
    
    if (system("perl perftest.pl -jetstream @ARGV"))
    {
       exit(1);
    }
}
