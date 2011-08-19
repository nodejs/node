package DES;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)
@EXPORT = qw(
);
# Other items we are prepared to export if requested
@EXPORT_OK = qw(
crypt
);

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.
bootstrap DES;
1;
__END__
