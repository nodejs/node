#ifndef UNITS_H
#define UNITS_H

char *format_binary(long double);
char *format_metric(long double);
char *format_time_us(long double);
char *format_time_s(long double);

int scan_metric(char *, uint64_t *);
int scan_time(char *, uint64_t *);

#endif /* UNITS_H */
