#!/usr/bin/env Rscript

# To use this to graph some benchmarks, install R (http://www.r-project.org/)
# and ggplot (http://ggplot2.org/).
#
# Once installed, you can generate some CSV output with a command like this:
#
#     $ OUTPUT_FORMAT=csv node benchmark/http/client-request-body.js > data.csv
#     $ ./benchmark/plot_csv.R data.csv data.png bytes type
#
# Where the 3rd argument to this script is the graph's X coordinate, the 4th is
# how the output is grouped, and the Y coordinate defaults to result.

library(methods)
library(ggplot2)

# get info from arguments
args <- commandArgs(TRUE)

csvFilename <- args[1]
graphFilename <- args[2]

xCoordinate <- args[3]
groupBy <- args[4]

# read data
data <- read.csv(file = csvFilename, head = TRUE)

# plot and save
plot <- ggplot(data = data, aes_string(x = xCoordinate, y = 'result', col = groupBy)) +
        geom_point(size = 5) +
        ggtitle(data$filename)

png(filename = graphFilename, width = 560, height = 480, units = 'px')
print(plot)
graphics.off()

cat(paste('Saved to', graphFilename, '\n'))
