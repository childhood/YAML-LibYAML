#include <perl_libyaml.h>
/* XXX Make -Wall not complain about 'local_patches' not being used. */
#if !defined(PERL_PATCHLEVEL_H_IMPLICIT)
void xxx_local_patches_xs() {
    printf("%s", local_patches[0]);
}
#endif

MODULE = YAML::XS::LibYAML		PACKAGE = YAML::XS::LibYAML		

PROTOTYPES: DISABLE

void
Load (yaml_str)
	char*	yaml_str
        PPCODE:
        PL_markstack_ptr++;
        Load(yaml_str);
        return;

void
Dump (...)
        PPCODE:
        SV * dummy = NULL;
        PL_markstack_ptr++;
        Dump(dummy);
        return;
