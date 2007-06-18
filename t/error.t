use t::TestYAMLTests tests => 6;

filters {
    error => ['fixup', 'regexp'],
};

run {
    my $test = shift;
    eval {
        Load($test->yaml);
    };
    like $@, $test->error, $test->name;
};

sub fixup {
    s/^\s+/\\s+/gm;
    s/\n/\\n/g;
}

__DATA__
=== Bad hash indentation
+++ yaml
foo: 2
 bar: 4
+++ error
  problem: mapping values are not allowed in this context
  document: 1

=== Unquoted * as hash key
+++ yaml
*: foo
+++ error
  problem: did not find expected alphabetic or numeric character
  document: 1

=== Unquoted * as hash value
+++ yaml
---
foo bar: *
+++ error
  problem: did not find expected alphabetic or numeric character
  document: 1

=== Unquoted * as scalar
+++ yaml
--- xxx
--- * * *
+++ error
  problem: did not find expected alphabetic or numeric character
  document: 2

=== Bad tag for array
+++ yaml
--- !!foo []
+++ error
  problem: Bad tag found for array: 'tag:yaml.org,2002:foo'
  document: 1

=== Bad tag for hash
+++ yaml
--- !foo {}
+++ error
  problem: Bad tag found for hash: '!foo'
  document: 1
