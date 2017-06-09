#!/usr/bin/env Rscript
library(ggplot2);
library(plyr);

# get __dirname and load ./_cli.R
args = commandArgs(trailingOnly = F);
dirname = dirname(sub("--file=", "", args[grep("--file", args)]));
source(paste0(dirname, '/_cli.R'), chdir=T);

if (!is.null(args.options$help) ||
   (!is.null(args.options$plot) && args.options$plot == TRUE)) {
  stop("usage: cat file.csv | Rscript compare.R
  --help           show this message
  --plot filename  save plot to filename");
}

plot.filename = args.options$plot;

dat = read.csv(
  file('stdin'),
  colClasses=c('character', 'character', 'character', 'numeric', 'numeric')
);
dat = data.frame(dat);

dat$nameTwoLines = paste0(dat$filename, '\n', dat$configuration);
dat$name = paste0(dat$filename, dat$configuration);

# Create a box plot
if (!is.null(plot.filename)) {
  p = ggplot(data=dat);
  p = p + geom_boxplot(aes(x=nameTwoLines, y=rate, fill=binary));
  p = p + ylab("rate of operations (higher is better)");
  p = p + xlab("benchmark");
  p = p + theme(axis.text.x = element_text(angle = 90, hjust = 1, vjust = 0.5));
  ggsave(plot.filename, p);
}

# Print a table with results
statistics = ddply(dat, "name", function(subdat) {
  old.rate = subset(subdat, binary == "old")$rate;
  new.rate = subset(subdat, binary == "new")$rate;

  # Calculate improvement for the "new" binary compared with the "old" binary
  old.mu = mean(old.rate);
  new.mu = mean(new.rate);
  improvement = sprintf("%.2f %%", ((new.mu - old.mu) / old.mu * 100));

  p.value = NA;
  confidence = 'NA';
  # Check if there is enough data to calculate the calculate the p-value
  if (length(old.rate) > 1 && length(new.rate) > 1) {
    # Perform a statistics test to see of there actually is a difference in
    # performance.
    w = t.test(rate ~ binary, data=subdat);
    p.value = w$p.value;

    # Add user friendly stars to the table. There should be at least one star
    # before you can say that there is an improvement.
    confidence = '';
    if (p.value < 0.001) {
      confidence = '***';
    } else if (p.value < 0.01) {
      confidence = '**';
    } else if (p.value < 0.05) {
      confidence = '*';
    }
  }

  r = list(
    improvement = improvement,
    confidence = confidence,
    p.value = p.value
  );
  return(data.frame(r));
});


# Set the benchmark names as the row.names to left align them in the print
row.names(statistics) = statistics$name;
statistics$name = NULL;

options(width = 200);
print(statistics);
