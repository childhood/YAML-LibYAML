use t::TestYAMLTests tests => 1;
no warnings 'once';

is Dump(*YAML), "--- '*main::YAML'\n",
    "Globs dump as scalars for now";
