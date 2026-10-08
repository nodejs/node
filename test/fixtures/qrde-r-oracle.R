# Regenerate qrde-r-oracle.json with:
# Rscript test/fixtures/qrde-r-oracle.R > test/fixtures/qrde-r-oracle.json

beta_front <- function(x, a, b) {
  result <- numeric(length(x))
  interior <- x > 0 & x < 1
  result[interior] <- x[interior] * (1 - x[interior]) *
    dbeta(x[interior], a, b)
  result
}

bucket_ranges <- function(values, resolutions, counts, mode) {
  lower <- upper <- values
  spread <- counts > 1 &
    (mode == "all" | (mode == "hdr" & resolutions > 1))
  half <- resolutions / 2
  lower[spread] <- values[spread] - half[spread]
  upper[spread] <- values[spread] + half[spread]
  if (length(values) > 1) {
    if (spread[1]) lower[1] <- values[1]
    if (spread[length(values)]) upper[length(values)] <- values[length(values)]
  }
  list(lower = lower, upper = upper, spread = spread)
}

hd_quantile <- function(values, resolutions, counts, p, mode) {
  n <- sum(counts)
  a <- (n + 1) * p
  b <- (n + 1) * (1 - p)
  u1 <- cumsum(counts) / n
  u0 <- c(0, head(u1, -1))
  threshold <- (a + 1) / (a + b + 2)
  lower <- u1 <= threshold
  upper <- u0 >= threshold
  crossing <- !(lower | upper)
  mass <- numeric(length(values))
  mass[lower] <- pbeta(u1[lower], a, b) - pbeta(u0[lower], a, b)
  mass[upper] <- pbeta(1 - u0[upper], b, a) -
    pbeta(1 - u1[upper], b, a)
  mass[crossing] <- 1 - pbeta(u0[crossing], a, b) -
    pbeta(1 - u1[crossing], b, a)

  ranges <- bucket_ranges(values, resolutions, counts, mode)
  if (!any(ranges$spread)) return(sum(values * mass))

  front0 <- beta_front(u0, a, b)
  front1 <- beta_front(u1, a, b)
  local <- (p - u0) * mass - (front1 - front0) / (a + b)
  rank_width <- counts / n
  local <- pmax(0, pmin(rank_width * mass, local))
  sum(ifelse(ranges$spread,
             ranges$lower * mass +
               (ranges$upper - ranges$lower) * local / rank_width,
             values * mass))
}

make_case <- function(name, values, resolutions, counts, bins, mode, indices) {
  ranges <- bucket_ranges(values, resolutions, counts, mode)
  expected <- vapply(indices, function(index) {
    if (index == 0) return(ranges$lower[1])
    if (index == bins) return(tail(ranges$upper, 1))
    hd_quantile(values, resolutions, counts, index / bins, mode)
  }, numeric(1))
  list(name = name, indices = indices, expected = expected)
}

make_probability_case <- function(name, values, resolutions, counts,
                                  probabilities, mode) {
  ranges <- bucket_ranges(values, resolutions, counts, mode)
  expected <- vapply(probabilities, function(probability) {
    if (probability == 0) return(ranges$lower[1])
    if (probability == 1) return(tail(ranges$upper, 1))
    hd_quantile(values, resolutions, counts, probability, mode)
  }, numeric(1))
  list(name = name, probabilities = probabilities, expected = expected)
}

cases <- list(
  make_case("small-none", c(1, 2, 4, 16, 100), rep(1, 5),
            c(1, 2, 5, 3, 1), 10, "none", 0:10),
  make_case("small-all", c(1, 2, 4, 16, 100), rep(1, 5),
            c(1, 2, 5, 3, 1), 10, "all", 0:10),
  make_case("exact-support", 1:1024, rep(1, 1024), rep(1, 1024),
            100, "none", c(1, 2, 10, 25, 50, 75, 90, 98, 99)),
  make_probability_case("tail-focused", 1:1024, rep(1, 1024), rep(1, 1024),
                        c(0, 0.5, 0.9, 0.99, 0.999, 0.9999, 1), "none"),
  make_case("approximation-threshold", c(1, 131071), c(1, 1),
            c(26239, 973761), 1000, "none",
            c(1, 10, 24, 25, 26, 27, 50, 500, 950, 973, 974, 975, 976,
              990, 999)),
  make_case("multimodal-none", c(1, 10, 100, 1000), rep(1, 4),
            c(900000, 90000, 9000, 1000), 100, "none",
            c(1, 10, 25, 50, 75, 89, 90, 91, 98, 99)),
  make_case("multimodal-all", c(1, 10, 100, 1000), rep(1, 4),
            c(900000, 90000, 9000, 1000), 100, "all",
            c(1, 10, 25, 50, 75, 89, 90, 91, 98, 99)),
  make_case("wide-none",
            c(1049088, 1100048498688, 4505798650626048, 9005000231485440),
            c(1024, 1073741824, 4398046511104, 4398046511104),
            c(700000, 200000, 99999, 1), 1000, "none",
            c(1, 100, 500, 699, 700, 701, 899, 900, 901, 990, 999)),
  make_case("wide-hdr",
            c(1049088, 1100048498688, 4505798650626048, 9005000231485440),
            c(1024, 1073741824, 4398046511104, 4398046511104),
            c(700000, 200000, 99999, 1), 1000, "hdr",
            c(1, 100, 500, 699, 700, 701, 899, 900, 901, 990, 999)),
  make_case("huge-count", c(1, 1000), c(1, 1),
            c(2^50, 2 * 2^50), 3, "none", c(1, 2))
)

number_array <- function(values) {
  paste0("[", paste(sprintf("%.17g", values), collapse = ", "), "]")
}

cat("{\n")
cat(sprintf("  \"rVersion\": \"%s\",\n", getRversion()))
cat("  \"implementation\": \"stats::pbeta/stats::dbeta\",\n")
cat("  \"generator\": \"test/fixtures/qrde-r-oracle.R\",\n")
cat("  \"cases\": {\n")
for (i in seq_along(cases)) {
  case <- cases[[i]]
  cat(sprintf("    \"%s\": {\n", case$name))
  if (is.null(case$indices)) {
    cat(sprintf("      \"probabilities\": %s,\n",
                number_array(case$probabilities)))
  } else {
    cat(sprintf("      \"indices\": %s,\n", number_array(case$indices)))
  }
  cat(sprintf("      \"quantiles\": %s\n", number_array(case$expected)))
  cat(sprintf("    }%s\n", if (i == length(cases)) "" else ","))
}
cat("  }\n")
cat("}\n")
