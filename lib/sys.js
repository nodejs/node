exports.print = function (x) {
  process.stdio.write(x);
};

exports.puts = function (x) {
  process.stdio.write(x.toString() + "\n");
};

exports.debug = function (x) {
  process.stdio.writeError("DEBUG: " + x.toString() + "\n");
};

exports.error = function (x) {
  process.stdio.writeError(x.toString() + "\n");
};

/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 * 
 * @param {Object} value The object to print out
 */
exports.inspect = function (value) {
  if (value === 0) return "0";
  if (value === false) return "false";
  if (value === "") return '""';
  if (typeof(value) == "function") return "[Function]";
  if (value === undefined) return;
  
  try {
    return JSON.stringify(value);
  } catch (e) {
    // TODO make this recusrive and do a partial JSON output of object.
    if (e.message.search("circular")) {
      return "[Circular Object]";
    } else {
      throw e;
    }
  }
};

exports.p = function (x) {
  exports.error(exports.inspect(x));
};

exports.exec = function (command) {
  var child = process.createChildProcess("/bin/sh", ["-c", command]);
  var stdout = "";
  var stderr = "";
  var promise = new process.Promise();

  child.addListener("output", function (chunk) {
    if (chunk) stdout += chunk; 
  });

  child.addListener("error", function (chunk) {
    if (chunk) stderr += chunk; 
  });
  
  child.addListener("exit", function (code) {
    if (code == 0) {
      promise.emitSuccess(stdout, stderr);
    } else {
      promise.emitError(code, stdout, stderr);
    }
  });

  return promise;
};

exports.memoryUsage = function () {
  if (process.platform == "linux2") {
    var data = require("file").read("/proc/self/stat").wait();
    // from linux-2.6.29/Documentation/filesystems/proc.tx
    // 0   pid           process id
    // 1   tcomm         filename of the executable
    // 2   state         state (R is running, S is sleeping, D is sleeping in an
    //                   uninterruptible wait, Z is zombie, T is traced or stopped)
    // 3   ppid          process id of the parent process
    // 4   pgrp          pgrp of the process
    // 5   sid           session id
    // 6   tty_nr        tty the process uses
    // 7   tty_pgrp      pgrp of the tty
    // 8   flags         task flags
    // 9   min_flt       number of minor faults
    // 10  cmin_flt      number of minor faults with child's
    // 11  maj_flt       number of major faults
    // 12  cmaj_flt      number of major faults with child's
    // 13  utime         user mode jiffies
    // 14  stime         kernel mode jiffies
    // 15  cutime        user mode jiffies with child's
    // 16  cstime        kernel mode jiffies with child's
    // 17  priority      priority level
    // 18  nice          nice level
    // 19  num_threads   number of threads
    // 20  it_real_value	(obsolete, always 0)
    // 21  start_time    time the process started after system boot
    // 22  vsize         virtual memory size
    // 23  rss           resident set memory size
    // 24  rsslim        current limit in bytes on the rss
    // 25  start_code    address above which program text can run
    // 26  end_code      address below which program text can run
    // 27  start_stack   address of the start of the stack
    // 28  esp           current value of ESP
    // 29  eip           current value of EIP
    // 30  pending       bitmap of pending signals (obsolete)
    // 31  blocked       bitmap of blocked signals (obsolete)
    // 32  sigign        bitmap of ignored signals (obsolete)
    // 33  sigcatch      bitmap of catched signals (obsolete)
    // 34  wchan         address where process went to sleep
    // 35  0             (place holder)
    // 36  0             (place holder)
    // 37  exit_signal   signal to send to parent thread on exit
    // 38  task_cpu      which CPU the task is scheduled on
    // 39  rt_priority   realtime priority
    // 40  policy        scheduling policy (man sched_setscheduler)
    // 41  blkio_ticks   time spent waiting for block IO
    var fields = data.split(" ");

    // 22  vsize         virtual memory size
    // 23  rss           resident set memory size

    return { vsize: parseInt(fields[22]), rss: 4096*parseInt(fields[23]) };

  } else if (process.platform == "darwin") {
    return process.macGetMemory();
  } else {
    throw new Error("Unsupported on your platform! Complain to Ryan!");
  }
};
