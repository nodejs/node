#!/usr/bin/env Rscript

# To use this script you'll need to install R: http://www.r-project.org/
# and a library for R called ggplot2 
# Which can be done by starting R and typing install.packages("ggplot2")
# like this:
#
#     shell% R
#     R version 2.11.0 beta (2010-04-12 r51689)
#     >  install.packages("ggplot2")
#     (follow prompt) 
#
# Then you can try this script by providing a full path to .data file
# outputed from 'make bench'
#
#     > cd ~/src/node
#     > make bench
#     ...
#     > ./benchmark/plot.R .benchmark_reports/ab-hello-world-buffer-1024/ff456b38862de3fd0118c6ac6b3f46edb1fbb87f/20101013162056.data
#     
# This will generate a PNG file which you can view
#
#
# Hopefully these steps will be automated in the future.



library(ggplot2)

args <- commandArgs(TRUE)

ab.load <- function (filename, name) {
  raw <- data.frame(read.csv(filename, sep="\t", header=T), server=name)
  raw <- data.frame(raw, time=raw$seconds-min(raw$seconds))
  raw <- data.frame(raw, time_s=raw$time/1000000)
  raw
}

#ab.tsPoint <- function (d) {
#  qplot(time_s, ttime, data=d, facets=server~.,
#        geom="point", alpha=I(1/15), ylab="response time (ms)",
#        xlab="time (s)", main="c=30, res=26kb", 
#        ylim=c(0,100))
#}
#
#ab.tsLine <- function (d) {
#  qplot(time_s, ttime, data=d, facets=server~.,
#        geom="line", ylab="response time (ms)",
#        xlab="time (s)", main="c=30, res=26kb", 
#        ylim=c(0,100))
#}


filename <- args[0:1]
data <- ab.load(filename, "node")


# histogram

#hist_png_filename <- gsub(".data", "_hist.png", filename)
hist_png_filename <- "hist.png"

png(filename = hist_png_filename, width = 480, height = 380, units = "px")

qplot(ttime, data=data, geom="histogram",
      main="xxx",
      binwidth=1, xlab="response time (ms)",
      xlim=c(0,100))

print(hist_png_filename)



# time series

#ts_png_filename <- gsub(".data", "_ts.png", filename)
ts_png_filename = "ts.png"

png(filename = ts_png_filename, width = 480, height = 380, units = "px")

qplot(time, ttime, data=data, facets=server~.,
      geom="point", alpha=I(1/15), ylab="response time (ms)",
      xlab="time (s)", main="xxx",
      ylim=c(0,100))

print(ts_png_filename)
