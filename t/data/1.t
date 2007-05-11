# Data is in a Perl .t file so vim Test::Base hilighters work...
#
__DATA__
=== Very Simple List
+++ yaml
---
- one
- two
+++ perl
[
"one",
"two"
]

=== Very Simple Hash
+++ yaml
---
one: two
+++ perl
{
"one" => "two"
}

=== Parse a more complicated structure
+++ yaml
---
- one
- two
- foo bar: [blah: {1: 2}, xxx]
  la la: mama
- three
+++ perl
[
"one",
"two",
{
    "foo bar" => [
        {
            "blah" => {
                1 => 2,
            },
        },
        "xxx",
    ],
    "la la" => "mama",
},
"three",
]

=== JSON is YAML
+++ yaml
{"name": "ingy", "rank": "yes",
"serial number":
42}
+++ perl
{
    "name" => "ingy",
    "rank" => "yes",
    "serial number" => 42,
}
