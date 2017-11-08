Once the workload, production server has reached a steady state/ post warm-up phase

- [] Collect the perf profile for the workload:
```
     perf record -ag -e cycles:pp  -o <perf_profile_cycles_pp> --  sleep 30 
```
`:pp` offers a more precise dist of cycles accounting

- [] Use perf script command to decode the collected profile
```
      perf script -i perf_profile_inst -f comm,ip -c node | gzip -c > perf_profile_decoded.pds.gz
```
- [] Use nm to dump the binary's symbol information
```
        nm -S node > node.nm
```
- [] Run the hfsort program to determine the function ordering 
     for the node binary ideal for this workload.
     https://github.com/facebook/hhvm/tree/master/hphp/tools/hfsort
```
          Usage: hfsort [-p] [-e <EDGCNT_FILE>] <SYMBOL_FILE> <PERF_DATA_FILE>

           -p,   use pettis-hansen algorithm for code layout  (optional)

           -e    <EDGCNT_FILE>, use edge profile result to build the call graph

        hfsort -p node.nm perf_profile_decoded.pds.gz
```
This application will create 2 files hotfuncs.txt and result-hfsort.txt

hotfuncs.txt is what is used here. hfsort is one way to generate it.
hfsort generates the minimal working order set required to optimally run the program.
