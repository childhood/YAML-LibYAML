use t::TestYAMLTests tests => 10;

run {
    my $block = shift;
    my @values = eval $block->perl;
    is Dump(@values), $block->yaml, "Dump - " . $block->name
        unless $block->SKIP_DUMP;
    is_deeply [Load($block->yaml)], \@values, "Load - " . $block->name;
};

__DATA__

=== Simple scalar ref
+++ perl
\ 42;
+++ yaml
--- !!perl/ref
=: 42

=== Ref to scalar ref
+++ perl
\\ "foo bar";
+++ yaml
--- !!perl/ref
=: !!perl/ref
  =: foo bar

=== Scalar refs an aliases
+++ perl
my $x = \\ 3.1415;
[$x, $$x];
+++ yaml
---
- !!perl/ref
  =: &1 !!perl/ref
    =: 3.1415
- *1

=== Ref to undef
+++ perl
my $x = {foo => \undef};

+++ yaml
---
foo: !!perl/ref
  =: ~

=== Circular ref to scalar
+++ perl
my $x;
$x = \$x;
+++ yaml
--- &1 !!perl/ref
=: *1
