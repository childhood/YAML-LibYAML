#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#include <yaml.h>
#include <ppport_sort.h>

#define TAG_PERL_REF "tag:yaml.org,2002:perl/ref"

typedef struct {
    yaml_parser_t parser;
    yaml_event_t event;
    HV* anchors;
} perl_yaml_loader_t;

typedef struct {
    yaml_emitter_t emitter;
    long anchor;
    HV* anchors;
} perl_yaml_dumper_t;

SV* load_node(perl_yaml_loader_t*);
SV* load_mapping(perl_yaml_loader_t*);
SV* load_sequence(perl_yaml_loader_t*);
SV* load_scalar(perl_yaml_loader_t*);
SV* load_alias(perl_yaml_loader_t*);
SV* load_scalar_ref(perl_yaml_loader_t*);

void dump_prewalk(perl_yaml_dumper_t*, SV*);
void dump_document(perl_yaml_dumper_t*, SV*);
void dump_node(perl_yaml_dumper_t*, SV*);
void dump_hash(perl_yaml_dumper_t*, SV*);
void dump_array(perl_yaml_dumper_t*, SV*);
void dump_scalar(perl_yaml_dumper_t*, SV*);
void dump_ref(perl_yaml_dumper_t*, SV*);

int append_output(SV *, unsigned char *, unsigned int size);

void* _die(char* msg) {
    croak("LibYAML Error - %s", msg);
}

void Load(char* yaml_str) {
    dXSARGS; sp = mark;
    perl_yaml_loader_t loader;
    /*yaml_parser_t parser;
    yaml_event_t event;*/
    SV* node;
    yaml_parser_initialize(&loader.parser);
    yaml_parser_set_input_string(
        &loader.parser,
        (unsigned char *) yaml_str,
        strlen((char *) yaml_str)
    );
    yaml_parser_parse(&loader.parser, &loader.event);
    if (loader.event.type != YAML_STREAM_START_EVENT)
        _die("Expected STREAM_START_EVENT");
    while (1) {
        yaml_parser_parse(&loader.parser, &loader.event);
        if (loader.event.type == YAML_STREAM_END_EVENT)
            break;
        loader.anchors = newHV();
        node = load_node(&loader);
        sv_dec((SV*)loader.anchors);
        if (! node) break;
        XPUSHs(node);
        yaml_parser_parse(&loader.parser, &loader.event);
        if (loader.event.type != YAML_DOCUMENT_END_EVENT)
            _die("Expected DOCUMENT_END_EVENT");
    }
    if (loader.event.type != YAML_STREAM_END_EVENT) {
        char* msg;
        asprintf(&msg,
            "Expected STREAM_END_EVENT; Got: %d != %d",
            loader.event.type,
            YAML_STREAM_END_EVENT
        );
        _die(msg);
    }
    yaml_parser_delete(&loader.parser);
    PUTBACK;
}

SV* load_node(perl_yaml_loader_t * loader) {
    yaml_parser_parse(&loader->parser, &loader->event);
    char * tag = (char *)loader->event.data.mapping_start.tag;

    if (loader->event.type == YAML_DOCUMENT_END_EVENT ||
        loader->event.type == YAML_MAPPING_END_EVENT ||
        loader->event.type == YAML_SEQUENCE_END_EVENT) return NULL;
    if (loader->event.type == YAML_MAPPING_START_EVENT) {
        if (tag && strcmp(tag, TAG_PERL_REF) == 0)
            return load_scalar_ref(loader);
        return load_mapping(loader);
    }
    if (loader->event.type == YAML_SEQUENCE_START_EVENT) {
        return load_sequence(loader);
    }
    if (loader->event.type == YAML_SCALAR_EVENT) {
        return load_scalar(loader);
    }
    if (loader->event.type == YAML_ALIAS_EVENT) {
        return load_alias(loader);
    }

    char* msg;
    asprintf(&msg, "Invalid event '%d' at top level", (int) loader->event.type);
    _die(msg);
}

SV* load_mapping(perl_yaml_loader_t * loader) {
    SV* key_node;
    SV* value_node;
    HV* hash = newHV();
    SV* hash_ref = (SV*) newRV_noinc((SV*) hash);
    char * anchor = (char *)loader->event.data.mapping_start.anchor;
    if (anchor)
        hv_store(loader->anchors, anchor, strlen(anchor), hash_ref, 0);
    while (key_node = load_node(loader)) {
        assert(SVPOK(key_node));
        value_node = load_node(loader);
        hv_store(
            hash, SvPV_nolen(key_node), sv_len(key_node), value_node, 0
        );
    } 
    return hash_ref;
}

SV* load_sequence(perl_yaml_loader_t * loader) {
    SV* node;
    AV* array = newAV();
    SV* array_ref = (SV*) newRV_noinc((SV*) array);
    char * key = (char *)loader->event.data.sequence_start.anchor;
    if (key)
        hv_store(loader->anchors, key, strlen(key), array_ref, 0);
    while (node = load_node(loader)) {
        av_push(array, node);
    } 
    return array_ref;
}

SV* load_scalar(perl_yaml_loader_t * loader) {
    char * string = (char *) loader->event.data.scalar.value;
    if (loader->event.data.scalar.style == YAML_PLAIN_SCALAR_STYLE) {
        if (strcmp(string, "~") == 0) {
            return &PL_sv_undef;
        }
        else if (strcmp(string, "true") == 0) {
            return &PL_sv_yes;
        }
        else if (strcmp(string, "false") == 0) {
            return &PL_sv_no;
        }
    }
    return newSVpvn(string, strlen(string));
}

SV* load_alias(perl_yaml_loader_t * loader) {
    char * anchor = (char *) loader->event.data.alias.anchor;
    SV** entry = hv_fetch(loader->anchors, anchor, strlen(anchor), 0);
    return entry ? *entry : newSVpvf("*%s", anchor);
}

SV* load_scalar_ref(perl_yaml_loader_t * loader) {
    SV* value_node;
    char * anchor = (char *)loader->event.data.mapping_start.anchor;
    load_node(loader);  // Load the single hash key (=)
    value_node = load_node(loader);
    if (value_node == &PL_sv_undef)
        value_node = newSVpvn(NULL, 0);
    SV* rv = newRV_inc(value_node);
    if (anchor)
        hv_store(loader->anchors, anchor, strlen(anchor), rv, 0);
    load_node(loader) == NULL || _die("Expected end of node");
    return rv;
}

/* -------------------------------------------------------------------------- */

SV* Dump(SV * dummy, ...) {
    perl_yaml_dumper_t dumper;
    yaml_event_t event_stream_start;
    yaml_event_t event_stream_end;
    dXSARGS; sp = mark;
    int i;
    SV* yaml = newSVpvn("", 0);

    yaml_emitter_initialize(&dumper.emitter);
    yaml_emitter_set_width(&dumper.emitter, 2);
    yaml_emitter_set_output(
        &dumper.emitter,
        (yaml_write_handler_t *) &append_output,
        yaml
    );
    yaml_stream_start_event_initialize(
        &event_stream_start,
        YAML_UTF8_ENCODING
    );
    // printf("yaml_emitter_emit event_stream_start\n");
    yaml_emitter_emit(&dumper.emitter, &event_stream_start);
    for (i = 0; i < items; i++) {
        dumper.anchor = 1;
        dumper.anchors = newHV();
        dump_prewalk(&dumper, ST(i));
        dump_document(&dumper, ST(i));
        sv_dec((SV*)dumper.anchors);
    }
    yaml_stream_end_event_initialize(&event_stream_end);
    // printf("yaml_emitter_emit event_stream_end\n");
    yaml_emitter_emit(&dumper.emitter, &event_stream_end);
    yaml_emitter_delete(&dumper.emitter);
    if (yaml) XPUSHs(yaml);
    PUTBACK;
}

// 00:12 <TonyC> hv_store(hv, (char *)&node, sizeof(node), some_sv, 0);
void dump_prewalk(perl_yaml_dumper_t * dumper, SV* node) {
    int i, len;

    if (SvROK(node)) {
        U32 ref_type = SvTYPE(SvRV(node));
        SV* coll = SvROK(node) ? SvRV(node) : NULL;
        // printf("1 > prewalking: %d\n", coll);
        SV** seen = hv_fetch(dumper->anchors, (char *)&coll, sizeof(coll), 0);
        if (seen) {
            if (*seen == &PL_sv_undef) {
                // printf("2 > seen twice %d => %d\n", coll, dumper->anchor);
                hv_store(
                    dumper->anchors, (char *)&coll, sizeof(coll),
                    newSViv(dumper->anchor), 0
                );
                dumper->anchor++;
            }
            return;
        }
        // printf("3 > seen: %d\n", coll);
        hv_store(dumper->anchors, (char *)&coll, sizeof(coll), &PL_sv_undef, 0);

        if (ref_type == SVt_PVAV) {
            // printf("4 > this is array\n");
            AV* array = (AV*) SvRV(node);
            int array_size = av_len(array) + 1;
            for (i = 0; i < array_size; i++) {
                SV** entry = av_fetch(array, i, 0);
                // printf("5 > array entry\n");
                if (entry)
                    dump_prewalk(dumper, *entry);
            }
        }
        else if (ref_type == SVt_PVHV) {
            // printf("4 > this is hash\n");
            HV* hash = (HV*) SvRV(node);
            len = HvKEYS(hash);
            hv_iterinit(hash);
            AV *av = (AV*)sv_2mortal((SV*)newAV());
            for (i = 0; i < len; i++) {
                HE *he = hv_iternext(hash);
                SV *key = hv_iterkeysv(he);
                av_store(av, AvFILLp(av)+1, key);
            }
            for (i = 0; i < len; i++) {
                SV *key = av_shift(av);
                HE *he  = hv_fetch_ent(hash, key, 0, 0);
                SV *val = HeVAL(he);
                // printf("5 > hash entry\n");
                if (val)
                    dump_prewalk(dumper, val);
            }
        }
        else if (ref_type <= SVt_PVNV) {
            // printf("4 > this is scalar ref\n");
            SV* scalar = SvRV(node);
            dump_prewalk(dumper, scalar);
        }
        else {
            // printf("4 > this is other ref\n");
        }
    }
    else {
        // printf("4 > this is scalar '%s'\n", SvPV_nolen(node));
    }
}

void dump_document(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_document_start;
    yaml_event_t event_document_end;
    yaml_document_start_event_initialize(
        &event_document_start, NULL, NULL, NULL, 0
    );
    // printf("yaml_emitter_emit event_document_start\n");
    yaml_emitter_emit(&dumper->emitter, &event_document_start);
    dump_node(dumper, node);
    yaml_document_end_event_initialize(&event_document_end, 1);
    // printf("yaml_emitter_emit event_document_end\n");
    yaml_emitter_emit(&dumper->emitter, &event_document_end);
}

void dump_node(perl_yaml_dumper_t * dumper, SV* node) {
    if (SvROK(node)) {
        U32 ref_type = SvTYPE(SvRV(node));
        if (ref_type == SVt_PVHV) {
            dump_hash(dumper, node);
        }
        else if (ref_type == SVt_PVAV) {
            dump_array(dumper, node);
        }
        else if (ref_type <= SVt_PVNV) {
            dump_ref(dumper, node);
        }
        else {
            printf(
                "YAML::LibYAML dump unhandled ref. type == '%d'!\n",
                ref_type
            );
            dump_scalar(dumper, SvRV(node));
        }
    }
    else {
        dump_scalar(dumper, node);
    }
}

void dump_hash(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_mapping_start;
    yaml_event_t event_mapping_end;
    yaml_event_t event_alias;
    int i;
    int len;
    HV* hash = (HV*) SvRV(node);
    len = HvKEYS(hash);
    hv_iterinit(hash);

    yaml_char_t* anchor = NULL;
    SV** seen = hv_fetch(dumper->anchors, (char *)&hash, sizeof(hash), 0);
    if (seen && *seen != &PL_sv_undef) {
        anchor = (yaml_char_t*)SvPV_nolen(*seen);
        if (SvREADONLY(*seen)) {
            // printf("yaml_emitter_emit event_alias (%s)\n", anchor);
            yaml_alias_event_initialize(&event_alias, anchor);
            yaml_emitter_emit(&dumper->emitter, &event_alias);
            return;
        }
        SvREADONLY_on(*seen);
    }

    yaml_mapping_start_event_initialize(
        &event_mapping_start, anchor, NULL, 0, YAML_BLOCK_MAPPING_STYLE
    );
    // printf("yaml_emitter_emit event_mapping_start\n");
    yaml_emitter_emit(&dumper->emitter, &event_mapping_start);

    AV *av = (AV*)sv_2mortal((SV*)newAV());
    for (i = 0; i < len; i++) {
        HE *he = hv_iternext(hash);
        SV *key = hv_iterkeysv(he);
        av_store(av, AvFILLp(av)+1, key); /* av_push(), really */
    }
    STORE_HASH_SORT;
    for (i = 0; i < len; i++) {
        SV *key = av_shift(av);
        HE *he  = hv_fetch_ent(hash, key, 0, 0);
        SV *val = HeVAL(he);
        if (val == NULL) { val = &PL_sv_undef; }
        dump_node(dumper, key);
        dump_node(dumper, val);
    }

    yaml_mapping_end_event_initialize(&event_mapping_end);
    // printf("yaml_emitter_emit event_mapping_end\n");
    yaml_emitter_emit(&dumper->emitter, &event_mapping_end);
}

void dump_array(perl_yaml_dumper_t * dumper, SV * node) {
    yaml_event_t event_sequence_start;
    yaml_event_t event_sequence_end;
    yaml_event_t event_alias;
    int i;
    AV* array = (AV*) SvRV(node);
    int array_size = av_len(array) + 1;

    yaml_char_t* anchor = NULL;
    SV** seen = hv_fetch(dumper->anchors, (char *)&array, sizeof(array), 0);
    if (seen && *seen != &PL_sv_undef) {
        anchor = (yaml_char_t*)SvPV_nolen(*seen);
        if (SvREADONLY(*seen)) {
            // printf("yaml_emitter_emit event_alias (%s)\n", anchor);
            yaml_alias_event_initialize(&event_alias, anchor);
            yaml_emitter_emit(&dumper->emitter, &event_alias);
            return;
        }
        SvREADONLY_on(*seen);
    }

    yaml_sequence_start_event_initialize(
        &event_sequence_start, anchor, NULL, 0, YAML_BLOCK_SEQUENCE_STYLE
    );
    // printf("yaml_emitter_emit event_sequence_start\n");
    yaml_emitter_emit(&dumper->emitter, &event_sequence_start);
    for (i = 0; i < array_size; i++) {
        SV** entry = av_fetch(array, i, 0);
        if (entry == NULL)
            dump_node(dumper, &PL_sv_undef);
        else
            dump_node(dumper, *entry);
    }
    yaml_sequence_end_event_initialize(&event_sequence_end);
    // printf("yaml_emitter_emit event_sequence_end\n");
    yaml_emitter_emit(&dumper->emitter, &event_sequence_end);
}

void dump_scalar(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_scalar;
    char * string;
    int plain_implicit = 1;
    int quoted_implicit = 0;
    yaml_scalar_style_t style = YAML_PLAIN_SCALAR_STYLE;
    svtype type = SvTYPE(node);


    if (type == SVt_NULL) {
        string = "~";
        style = YAML_PLAIN_SCALAR_STYLE;
    }
    else if (node == &PL_sv_yes) {
        string = "true";
        style = YAML_PLAIN_SCALAR_STYLE;
    }
    else if (node == &PL_sv_no) {
        string = "false";
        style = YAML_PLAIN_SCALAR_STYLE;
    }
    else {
        string = SvPV_nolen(node);
        if (
            (strlen(string) == 0) ||
            (strcmp(string, "~") == 0) ||
            (strcmp(string, "true") == 0) ||
            (strcmp(string, "false") == 0) ||
            (strcmp(string, "null") == 0)
        ) {
            plain_implicit = 0;
            quoted_implicit = 1;
            style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
        }
    }
    int length = strlen(string);
    yaml_scalar_event_initialize(
        &event_scalar,
        NULL,
        NULL,
        (unsigned char *) string,
        length,
        plain_implicit,
        quoted_implicit,
        style
    );
    // printf("yaml_emitter_emit event_scalar\n");
    yaml_emitter_emit(&dumper->emitter, &event_scalar) ||
        printf(
            "emit scalar '%s', error: %s\n", string, dumper->emitter.problem
        );
}

void dump_ref(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_mapping_start;
    yaml_event_t event_mapping_end;
    yaml_event_t event_scalar;
    yaml_event_t event_alias;
    int i;
    int len;
    SV* referent = SvRV(node);

    yaml_char_t* anchor = NULL;
    SV** seen =
        hv_fetch(dumper->anchors, (char *)&referent, sizeof(referent), 0);
    if (seen && *seen != &PL_sv_undef) {
        anchor = (yaml_char_t*)SvPV_nolen(*seen);
        if (SvREADONLY(*seen)) {
            yaml_alias_event_initialize(&event_alias, anchor);
            yaml_emitter_emit(&dumper->emitter, &event_alias);
            return;
        }
        SvREADONLY_on(*seen);
    }

    yaml_mapping_start_event_initialize(
        &event_mapping_start, anchor,
        (unsigned char *) "tag:yaml.org,2002:perl/ref",
        0, YAML_BLOCK_MAPPING_STYLE
    );
    yaml_emitter_emit(&dumper->emitter, &event_mapping_start);

    yaml_scalar_event_initialize(
        &event_scalar,
        NULL, NULL,
        (unsigned char *) "=", 1,
        1, 1,
        YAML_PLAIN_SCALAR_STYLE
    );
    yaml_emitter_emit(&dumper->emitter, &event_scalar);
    dump_node(dumper, referent);

    yaml_mapping_end_event_initialize(&event_mapping_end);
    yaml_emitter_emit(&dumper->emitter, &event_mapping_end);
}

int append_output(SV * yaml, unsigned char * buffer, unsigned int size) {
    sv_catpvn(yaml, (const char *) buffer, (STRLEN) size);
}
