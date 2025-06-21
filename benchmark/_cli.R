
args = commandArgs(TRUE);

args.options = list();

temp.option.key = NULL;

for (arg in args) {
  # Optional arguments declaration
  if (substring(arg, 1, 1) == '-') {
    temp.option.key = substring(arg, 2);
    if (substring(arg, 2, 2) == '-') {
      temp.option.key = substring(arg, 3);
    }

    args.options[[temp.option.key]] = TRUE;
  }
  # Optional arguments value
  else if (!is.null(temp.option.key)) {
    args.options[[temp.option.key]] = arg;

    temp.option.key = NULL;
  }
}
