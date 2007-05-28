#!/usr/bin/perl

use strict;
use warnings;

use YAML;
use YAML::Syck;
use YAML::LibYAML;
use YAML::Tiny;

use Benchmark;

use Data::Dumper;

my $reps = 4;

my %struct = (
   argh_hash   => { map { $_ => "Argh" } 0 .. $reps },
   argh_list  => [ map { "Argh" } 0 .. $reps ],
   argh_scalar => ("Argh" x 512) # Uncomment to make pathological scalar case
);

my $current_struct = \%struct;

my %methods =
  (
   yamlpm => sub {
       YAML::Dump( $current_struct );
   },

   syck => sub {
       YAML::Syck::Dump( $current_struct );
   },

   libyaml => sub {
       YAML::LibYAML::Dump( $current_struct );
   },
   tiny => sub {
       YAML::Tiny::Dump( $current_struct );
   },
  );

while ( my ( $name, $method ) = each %methods ) {
    print $name . " => (" . $method->() . ")\n";
}

Benchmark::cmpthese( 25000, \%methods );
