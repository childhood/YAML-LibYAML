#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#include <yaml.h>

MODULE = YAML::LibYAML::XS		PACKAGE = YAML::LibYAML::XS		

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
        SV * dummy;
        PL_markstack_ptr++;
        Dump(dummy);
        return;


