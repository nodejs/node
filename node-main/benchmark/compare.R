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
dat$name = paste0(dat$filename, ' ', dat$configuration);

# Create a box plot
if (!is.null(plot.filename)) {
  p = ggplot(data=dat);
  p = p + geom_boxplot(aes(x=nameTwoLines, y=rate, fill=binary));
  p = p + ylab("rate of operations (higher is better)");
  p = p + xlab("benchmark");
  p = p + theme(axis.text.x = element_text(angle = 90, hjust = 1, vjust = 0.5));
  ggsave(plot.filename, p);
}

# Computes the shared standard error, as used in Welch's t-test.
welch.sd = function (old.rate, new.rate) {
  old.se.squared = var(old.rate) / length(old.rate)
  new.se.squared = var(new.rate) / length(new.rate)
  return(sqrt(old.se.squared + new.se.squared))
}

# Calculate the improvement confidence interval. The improvement is calculated
# by dividing by old.mu and not new.mu, because old.mu is what the mean
# improvement is calculated relative to.
confidence.interval = function (shared.se, old.mu, w, risk) {
  interval = qt(1 - (risk / 2), w$parameter) * shared.se;
  return(sprintf("±%.2f%%", (interval / old.mu) * 100))
}

# Calculate the statistics table.
statistics = ddply(dat, "name", function(subdat) {
  old.rate = subset(subdat, binary == "old")$rate;
  new.rate = subset(subdat, binary == "new")$rate;

  # Calculate improvement for the "new" binary compared with the "old" binary
  old.mu = mean(old.rate);
  new.mu = mean(new.rate);
  improvement = sprintf("%.2f %%", ((new.mu - old.mu) / old.mu * 100));

  r = list(
    confidence = "NA",
    improvement = improvement,
    "accuracy (*)" = "NA",
    "(**)" = "NA",
    "(***)" = "NA"
  );

  # Check if there is enough data to calculate the p-value.
  if (length(old.rate) > 1 && length(new.rate) > 1) {
    # Perform a statistical test to see if there actually is a difference in
    # performance.
    w = t.test(rate ~ binary, data=subdat);
    shared.se = welch.sd(old.rate, new.rate)

    # Add user-friendly stars to the table. There should be at least one star
    # before you can say that there is an improvement.
    confidence = '';
    if (w$p.value < 0.001) {
      confidence = '***';
    } else if (w$p.value < 0.01) {
      confidence = '**';
    } else if (w$p.value < 0.05) {
      confidence = '*';
    }

    r = list(
      confidence = confidence,
      improvement = improvement,
      "accuracy (*)" = confidence.interval(shared.se, old.mu, w, 0.05),
      "(**)" = confidence.interval(shared.se, old.mu, w, 0.01),
      "(***)" = confidence.interval(shared.se, old.mu, w, 0.001)
    );
  }

  return(data.frame(r, check.names=FALSE));
});


# Set the benchmark names as the row.names to left align them in the print.
row.names(statistics) = statistics$name;
statistics$name = NULL;

options(width = 200);
print(statistics);
cat("\n")
cat(sprintf(
"Be aware that when doing many comparisons the risk of a false-positive
result increases. In this case, there are %d comparisons, you can thus
expect the following amount of false-positive results:
  %.2f false positives, when considering a   5%% risk acceptance (*, **, ***),
  %.2f false positives, when considering a   1%% risk acceptance (**, ***),
  %.2f false positives, when considering a 0.1%% risk acceptance (***)
",
nrow(statistics),
nrow(statistics) * 0.05,
nrow(statistics) * 0.01,
nrow(statistics) * 0.001))
