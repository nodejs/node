# Diagnostic Tooling Support Tiers

Diagnostic tooling is important to the consumers of Node.js. It is used both
in development and in production in order to investigate problems.  The failure
of one of these tools may be as big a problem for an end user as a bug within
the runtime itself.

The Node.js project has assessed the tools and the APIs which support those
tools. Each of the tools and APIs has been put into one of
the following tiers.  

* Tier 1 - Must always be working(CI tests passing) for all
  Current and LTS Node.js releases. A release will not be shipped if the test
  suite for the tool/API is not green. To be considered for inclusion
  in this tier it must have a good test suite and that test suite and a job
  must exist in the Node.js CI so that it can be run as part of the release
  process.  No commit to master should break this tool/API if the next
  major release is within 1 month. In addition:
    * The maintainers of the tool remain responsive when there are problems;
    * The tool must be actively used by the ecosystem;
    * The tool must be heavily depended on; and
    * The tool must only be using APIs exposed by Nodejs as opposed to
      its dependencies.
      
* Tier 2 - Must be working(CI tests passing) for all
  LTS releases. An LTS release will not be shipped if the test
  suite for the tool/API is not green. To be considered for inclusion
  in this tier it must have a good test suite and that test suite and a job
  must exist in the Node.js CI so that it can be run as part of the release
  process. 
  
 * Tier 3 - If possible its test suite
   will be run at least nightly in the Node.js CI and issues opened for
   failures.  Does not block shipping a release.
   
 * Tier 4 - Does not block shipping a release.
    
 * Unclassified - tool/API is new or does not have the required testing in the 
   Node.js CI in order to qualify for a higher tier.
   
The choice of which tier a particular tool will be assigned to, will be a
collaborative decision between Diagnostics WG and Release WG. Some of the
criteria considered might be:

* If the tool fits into a key category as listed below.
* Whether the tool is actively used by the ecosystem.
* The availability of alternatives.
* Impact to the overall ecosystem if the tool is not working.
* The availability of reliable test suite that can be integrated into our CI.
* The availability of maintainer or community collaborator who will help
  resolve issues when there are CI failures.
* If the tool is maintained by the Node.js Foundation GitHub organization.
 
The current categories of tools/APIs that fall under these Tiers are:

* FFDC (F) - First failure data capture, easy to consume initial diagnostic
  information.
* Tracing (T) - use of logging to provide information about execution flow.
* Memory (M) - tools that provide additional information about memory 
  used in the Heap or by native code.
* Profiling (P) - tools that provide additional information about where
  CPU cycles are being spent.
* AsyncFlow (A) - tools that provide additional insight into asynchronous
  execution flow.

 
The tools are currently assigned to Tiers as follows:
 
## Tier 1

 | Tool Type | Tool/API Name             | Regular Testing in Node.js CI | Integrated with Node.js | Target Tier |
 |-----------|---------------------------|-------------------------------|-------------------------|-------------|
 |           |                           |                               |                         |             |
 
## Tier 2

 | Tool Type | Tool/API Name             | Regular Testing in Node.js CI | Integrated with Node.js | Target Tier |
 |-----------|---------------------------|-------------------------------|-------------------------|-------------|
 |           |                           |                               |                         |             |
 
 
## Tier 3
 
 | Tool Type | Tool/API Name             | Regular Testing in Node.js CI | Integrated with Node.js | Target Tier |
 |-----------|---------------------------|-------------------------------|-------------------------|-------------|
 |           |                           |                               |                         |             |

## Tier 4

 | Tool Type | Tool/API Name             | Regular Testing in Node.js CI | Integrated with Node.js | Target Tier |
 |-----------|---------------------------|-------------------------------|-------------------------|-------------|
 |           |                           |                               |                         |             |
 
## Not yet classified
 
 | Tool Type | Tool/API Name             | Regular Testing in Node.js CI | Integrated with Node.js | Target Tier |
 |-----------|---------------------------|-------------------------------|-------------------------|-------------|
 | FFDC      | node-report               | No                            | No                      |     1       |
 | Memory    | llnode                    | ?                             | No                      |     2       |          
 | Memory    | mdb_V8                    | No                            | No                      |     4       | 
 | Memory    | node-heapdump             | No                            | No                      |     2       |
 | Memory    | V8 heap profiler          | No                            | Yes                     |     1       |
 | Memory    | V8 sampling heap profiler | No                            | Yes                     |     1       |
 | AsyncFlow | Async Hooks (API)         | ?                             | Yes                     |     1       |
 | Debugger  | V8 Debug protocol (API)   | No                            | Yes                     |     1       |
 | Debugger  | Command line Debug Client | ?                             | Yes                     |     1       |
 | Debugger  | Chrome Dev tools          | ?                             | No                      |     3       |
 | Debugger  | Chakracore - time-travel  | No                            | Data source only        | too early   |
 | Tracing   | trace_events (API)        | No                            | Yes                     |     1       |
 | Tracing   | DTrace                    | No                            | Partial                 |     3       |
 | Tracing   | LTTng                     | No                            | Removed?                |     N/A     |
 | Tracing   | ETW                       | No                            | Partial                 |     3       |
 | Tracing   | Systemtap                 | No                            | Partial                 |     ?       |
 | Profiling | V8 CPU profiler  (--prof) | No                            | Yes                     |     1       |
 | Profiling | Linux perf                | No                            | Partial                 |     ?       |
 | Profiling | DTrace                    | No                            | Partial                 |     3       |
 | Profiling | Windows Xperf             | No                            | ?                       |     ?       |
 | Profiling | Ox                        | No                            | No                      | too early   |
 | Profiling | node-clinic               | No                            | No                      | to early    |
 | F/P/T     | appmetrics                | No                            | No                      |     ?       |
 | M/T       | eBPF tracing tool         | No                            | No                      |     ?       | 
 
