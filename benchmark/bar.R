#!/usr/bin/env Rscript
library(ggplot2);
library(plyr);

# get __dirname and load ./_cli.R
args = commandArgs(trailingOnly = F);
dirname = dirname(sub("--file=", "", args[grep("--file", args)]));
source(paste0(dirname, '/_cli.R'), chdir=T);

if (!is.null(args.options$help) ||
   (!is.null(args.options$plot) && args.options$plot == TRUE)) {
  stop("usage: cat file.csv | Rscript bar.R
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
  p = ggplot(data=dat, aes(x=nameTwoLines, y=rate, fill=binary));
  p = p + geom_bar(stat="summary", position=position_dodge());
  p = p + ylab("rate of operations (higher is better)");
  p = p + xlab("benchmark");
  p = p + theme(axis.text.x = element_text(angle = 90, hjust = 1, vjust = 0.5));
  ggsave(plot.filename, p);
}
