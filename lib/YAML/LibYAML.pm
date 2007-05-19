# ToDo:
#
# * Find out how to loop over all the args passed to a C function
# * Find out how to write C macros
# * Support ~, null, true, false
# * Rewrite documentation
# * Support blessed objects
# * Copy all relevant code from YAML::Syck
#
package YAML::LibYAML;
use 5.008003;
use strict;
use warnings;
our $VERSION = '0.03';
use base 'Exporter';

our @EXPORT = qw(Load Dump);

use YAML::LibYAML::XS qw(Load Dump);

1;

=head1 NAME

YAML::LibYAML - LibYAML bindings for Perl

=head1 SYNOPSIS

    use YAML::LibYAML;

    my $yaml = Dump [ 1..4 ];
    my $array = Load $yaml;

=head1 DESCRIPTION

Kirill Siminov's C<libyaml> is arguably the best YAML implementation.
The C library is written precisely to the YAML 1.1 specification. It was
originally bound to Python and was later bound to Ruby.

This module is a Perl XS binding to libyaml which will (eventually)
offer Perl the best YAML support to date.

This module exports the functions C<Dump> and C<Load>. These functions
are intended to work exactly like C<YAML.pm>'s corresponding functions.

NOTE: This very early proof-of-concept release only works for unblessed
hashes, arrays, and scalars, without reference aliases.

On the other hand, this module supports the full YAML syntax for these data
types. There are no known bugs yet.

This work should progress quickly so check back often.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2007. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See http://www.perl.com/perl/misc/Artistic.html

=cut
