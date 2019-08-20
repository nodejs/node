#!/usr/bin/env Rscript
library(ggplot2);
library(plyr);

# get __dirname and load ./_cli.R
args = commandArgs(trailingOnly = F);
dirname = dirname(sub("--file=", "", args[grep("--file", args)]));
source(paste0(dirname, '/_cli.R'), chdir=T);

if (is.null(args.options$xaxis) || is.null(args.options$category) ||
   (!is.null(args.options$plot) && args.options$plot == TRUE)) {
  stop("usage: cat file.csv | Rscript scatter.R [variable=value ...]
  --xaxis    variable   variable name to use as xaxis (required)
  --category variable   variable name to use as colored category (required)
  --plot     filename   save plot to filename
  --log                 use a log-2 scale for xaxis in the plot");
}

plot.filename = args.options$plot;

# parse options
x.axis.name = args.options$xaxis;
category.name = args.options$category;
use.log2 = !is.null(args.options$log);

# parse data
dat = read.csv(file('stdin'), strip.white=TRUE);
dat = data.frame(dat);

# List of aggregated variables
aggregate = names(dat);
aggregate = aggregate[
  ! aggregate %in% c('rate', 'time', 'filename', x.axis.name, category.name)
];
# Variables that don't change aren't aggregated
for (aggregate.key in aggregate) {
  if (length(unique(dat[[aggregate.key]])) == 1) {
    aggregate = aggregate[aggregate != aggregate.key];
  }
}

# Print out aggregated variables
for (aggregate.variable in aggregate) {
  cat(sprintf('aggregating variable: %s\n', aggregate.variable));
}
if (length(aggregate) > 0) {
  cat('\n');
}

# Calculate statistics
stats = ddply(dat, c(x.axis.name, category.name), function(subdat) {
  rate = subdat$rate;

  # calculate confidence interval of the mean
  ci = NA;
  if (length(rate) > 1) {
    se = sqrt(var(rate)/length(rate));
    ci = se * qt(0.975, length(rate) - 1)
  }

  # calculate mean and 95 % confidence interval
  r = list(
    rate = mean(rate),
    confidence.interval = ci
  );

  return(data.frame(r));
});

print(stats, row.names=F);

if (!is.null(plot.filename)) {
  p = ggplot(stats, aes_string(x=x.axis.name, y='rate', colour=category.name));
  if (use.log2) {
    p = p + scale_x_continuous(trans='log2');
  }
  p = p + geom_errorbar(
    aes(ymin=rate-confidence.interval, ymax=rate+confidence.interval),
    width=.1, na.rm=TRUE
  );
  p = p + geom_point();
  p = p + geom_line();
  p = p + ylab("rate of operations (higher is better)");
  p = p + ggtitle(dat[1, 1]);
  ggsave(plot.filename, p);
}
