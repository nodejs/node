# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Do statistical tests on benchmark results
# This script requires the libraries rjson, R.utils, ggplot2 and data.table
# Install them prior to running

# To use the script, first get some benchmark results, for example via
# tools/run_perf.py ../v8-perf/benchmarks/Octane2.1/Octane2.1-TF.json
#  --outdir=out/x64.release-on --outdir-secondary=out/x64.release-off
# --json-test-results=results-on.json
# --json-test-results-secondary=results-off.json
# then run this script
# Rscript statistics-for-json.R results-on.json results-off.json ~/SVG
# to produce graphs (and get stdio output of statistical tests).


suppressMessages(library("rjson"))       # for fromJson
suppressMessages(library("R.utils"))     # for printf
suppressMessages(library("ggplot2"))     # for plotting
suppressMessages(library("data.table"))  # less broken than data.frame

# Clear all variables from environment
rm(list=ls())

args <- commandArgs(TRUE)
if (length(args) != 3) {
  printf(paste("usage: Rscript %%this_script patched-results.json",
               "unpatched-results.json\n"))
} else {
  patch <- fromJSON(file=args[1])
  nopatch <- fromJSON(file=args[2])
  outputPath <- args[3]
  df <- data.table(L = numeric(), R = numeric(), E = numeric(), 
                   p.value = numeric(), yL = character(), 
                   p.value.sig = logical())
  
  for (i in seq(1, length(patch$traces))) {
    testName <- patch$traces[[i]]$graphs[[2]]
    printf("%s\n", testName)
    
    nopatch_res <- as.integer(nopatch$traces[[i]]$results)
    patch_res <- as.integer(patch$traces[[i]]$results)
    if (length(nopatch_res) > 0) {
      patch_norm <- shapiro.test(patch_res);
      nopatch_norm <- shapiro.test(nopatch_res);

      # Shaprio-Wilk test indicates whether data is not likely to 
      # come from a normal distribution. The p-value is the probability
      # to obtain the sample from a normal distribution. This means, the
      # smaller p, the more likely the sample was not drawn from a normal
      # distribution. See [wikipedia:Shapiro-Wilk-Test].
      printf("  Patched scores look %s distributed (W=%.4f, p=%.4f)\n", 
             ifelse(patch_norm$p.value < 0.05, "not normally", "normally"), 
             patch_norm$statistic, patch_norm$p.value);
      printf("  Unpatched scores look %s distributed (W=%.4f, p=%.4f)\n", 
             ifelse(nopatch_norm$p.value < 0.05, "not normally", "normally"), 
             nopatch_norm$statistic, nopatch_norm$p.value);
      
      hist <- ggplot(data=data.frame(x=as.integer(patch_res)), aes(x)) +
        theme_bw() + 
        geom_histogram(bins=50) +
        ylab("Points") +
        xlab(patch$traces[[i]]$graphs[[2]])
      ggsave(filename=sprintf("%s/%s.svg", outputPath, testName), 
             plot=hist, width=7, height=7)
      
      hist <- ggplot(data=data.frame(x=as.integer(nopatch_res)), aes(x)) +
        theme_bw() + 
        geom_histogram(bins=50) +
        ylab("Points") +
        xlab(patch$traces[[i]]$graphs[[2]])
      ggsave(filename=sprintf("%s/%s-before.svg", outputPath, testName), 
             plot=hist, width=7, height=7)
      
      # The Wilcoxon rank-sum test 
      mww <- wilcox.test(patch_res, nopatch_res, conf.int = TRUE, exact=TRUE)
      printf(paste("  Wilcoxon U-test W=%.4f, p=%.4f,",
                   "confidence interval [%.1f, %.1f],",
                   "est. effect size %.1f \n"),
                   mww$statistic, mww$p.value,
                   mww$conf.int[1], mww$conf.int[2], mww$estimate);
      df <-rbind(df, list(mww$conf.int[1], mww$conf.int[2], 
                          unname(mww$estimate), unname(mww$p.value),
                          testName, ifelse(mww$p.value < 0.05, TRUE, FALSE)))
      # t-test
      t <- t.test(patch_res, nopatch_res, paired=FALSE)
      printf(paste("  Welch t-test t=%.4f, df = %.2f, p=%.4f,",
                   "confidence interval [%.1f, %.1f], mean diff %.1f \n"),
             t$statistic, t$parameter, t$p.value, 
             t$conf.int[1], t$conf.int[2], t$estimate[1]-t$estimate[2]);
    }
  }
  df2 <- cbind(x=1:nrow(df), df[order(E),])
  speedup <- ggplot(df2, aes(x = x, y = E, colour=p.value.sig)) +
    geom_errorbar(aes(ymax = L, ymin = R), colour="black") +
    geom_point(size = 4) +
    scale_x_discrete(limits=df2$yL,
                       name=paste("Benchmark, n=", length(patch_res))) +
    theme_bw() +
    geom_hline(yintercept = 0) +
    ylab("Est. Effect Size in Points") +
    theme(axis.text.x = element_text(angle = 90, hjust = 1, vjust=0.5)) +
    theme(legend.position = "bottom") +
    scale_colour_manual(name="Statistical Significance (MWW, p < 0.05)",
                          values=c("red", "green"),
                          labels=c("not significant", "significant")) +
    theme(legend.justification=c(0,1), legend.position=c(0,1))
  print(speedup)
  ggsave(filename=sprintf("%s/speedup-estimates.svg", outputPath), 
         plot=speedup, width=7, height=7)
}
