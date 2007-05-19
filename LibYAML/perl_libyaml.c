#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#include <yaml.h>
#include <ppport_sort.h>

typedef struct {
    yaml_parser_t parser;
    yaml_event_t event;
} perl_yaml_loader_t;

SV* get_node(perl_yaml_loader_t*);
SV* handle_mapping(perl_yaml_loader_t*);
SV* handle_sequence(perl_yaml_loader_t*);
SV* handle_scalar(perl_yaml_loader_t*);

void dump_document(yaml_emitter_t*, SV*);
void dump_node(yaml_emitter_t*, SV*);
void dump_hash(yaml_emitter_t*, SV*);
void dump_array(yaml_emitter_t*, SV*);
void dump_scalar(yaml_emitter_t*, SV*);
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
        node = get_node(&loader);
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

SV* get_node(perl_yaml_loader_t * loader) {
    SV* node;
    char* msg;
    yaml_parser_parse(&loader->parser, &loader->event);

    if (loader->event.type == YAML_DOCUMENT_END_EVENT ||
        loader->event.type == YAML_MAPPING_END_EVENT ||
        loader->event.type == YAML_SEQUENCE_END_EVENT) return NULL;
    node = (loader->event.type == YAML_MAPPING_START_EVENT) ?
        handle_mapping(loader) :
    (loader->event.type == YAML_SEQUENCE_START_EVENT) ?
        handle_sequence(loader) :
    (loader->event.type == YAML_SCALAR_EVENT) ?
        handle_scalar(loader) :
    NULL;
    if (node) return node;

    asprintf(&msg, "Invalid event '%d' at top level", (int) loader->event.type);
    _die(msg);
}

SV* handle_sequence(perl_yaml_loader_t * loader) {
    SV* node;
    AV* sequence = newAV();
    while (node = get_node(loader)) {
        av_push(sequence, node);
    } 
    return (SV*) newRV_noinc((SV*) sequence);
}

SV* handle_mapping(perl_yaml_loader_t * loader) {
    HV* mapping = newHV();
    SV* key_node;
    SV* value_node;
    while (key_node = get_node(loader)) {
        assert(SVPOK(key_node));
        value_node = get_node(loader);
        hv_store(
            mapping, SvPV_nolen(key_node), sv_len(key_node), value_node, 0
        );
    } 
    return (SV*) newRV_noinc((SV*) mapping);
}

SV* handle_scalar(perl_yaml_loader_t * loader) {
    char* msg;
    asprintf(&msg, ">>> handle_scalar: %s\n", loader->event.data.scalar.value);
    return newSVpvf((char *) loader->event.data.scalar.value);
}

/* -------------------------------------------------------------------------- */

SV* Dump(SV * dummy, ...) {
    yaml_emitter_t emitter;
    yaml_event_t event_stream_start;
    yaml_event_t event_stream_end;
    dXSARGS; sp = mark;
    int i;
    SV* yaml = newSVpvf("");

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_width(&emitter, 2);
    yaml_emitter_set_output(
        &emitter,
        (yaml_write_handler_t *) &append_output,
        yaml
    );
    yaml_stream_start_event_initialize(
        &event_stream_start,
        YAML_UTF8_ENCODING
    );
    yaml_emitter_emit(&emitter, &event_stream_start);
    for (i = 0; i < items; i++) {
        dump_document(&emitter, ST(i));
    }
    yaml_stream_end_event_initialize(&event_stream_end);
    yaml_emitter_emit(&emitter, &event_stream_end);
    yaml_emitter_delete(&emitter);
    if (yaml) XPUSHs(yaml);
    PUTBACK;
}

void dump_document(yaml_emitter_t * emitter, SV* node) {
    yaml_event_t event_document_start;
    yaml_event_t event_document_end;
    yaml_document_start_event_initialize(
        &event_document_start, NULL, NULL, NULL, 0
    );
    yaml_emitter_emit(emitter, &event_document_start);
    dump_node(emitter, node);
    yaml_document_end_event_initialize(&event_document_end, 1);
    yaml_emitter_emit(emitter, &event_document_end);
}

void dump_node(yaml_emitter_t * emitter, SV* node) {
    (SvROK(node)) 
      ? (SvTYPE(SvRV(node)) == SVt_PVAV) ? dump_array(emitter, node) :
        (SvTYPE(SvRV(node)) == SVt_PVHV) ? dump_hash(emitter, node) :
        dump_scalar(emitter, SvRV(node))
      : dump_scalar(emitter, node);
}

void dump_hash(yaml_emitter_t * emitter, SV* node) {
    yaml_event_t event_mapping_start;
    yaml_event_t event_mapping_end;
    int i;
    int len;
    HV* hash = (HV*) SvRV(node);
#ifdef HAS_RESTRICTED_HASHES
    len = HvTOTALKEYS(hash);
#else
    len = HvKEYS(hash);
#endif
    hv_iterinit(hash);

    yaml_mapping_start_event_initialize(
        &event_mapping_start, NULL, NULL, 0, YAML_BLOCK_MAPPING_STYLE
    );
    yaml_emitter_emit(emitter, &event_mapping_start);

    AV *av = (AV*)sv_2mortal((SV*)newAV());
    for (i = 0; i < len; i++) {
#ifdef HAS_RESTRICTED_HASHES
        HE *he = hv_iternext_flags(hash, HV_ITERNEXT_WANTPLACEHOLDERS);
#else
        HE *he = hv_iternext(hash);
#endif
        SV *key = hv_iterkeysv(he);
        av_store(av, AvFILLp(av)+1, key); /* av_push(), really */
    }
    STORE_HASH_SORT;
    for (i = 0; i < len; i++) {
#ifdef HAS_RESTRICTED_HASHES
        int placeholders = (int)HvPLACEHOLDERS_get(hv);
#endif
        SV *key = av_shift(av);
        HE *he  = hv_fetch_ent(hash, key, 0, 0);
        SV *val = HeVAL(he);
        if (val == NULL) { val = &PL_sv_undef; }
        dump_node(emitter, key);
        dump_node(emitter, val);
    }

    yaml_mapping_end_event_initialize(&event_mapping_end);
    yaml_emitter_emit(emitter, &event_mapping_end);
}

void dump_array(yaml_emitter_t * emitter, SV* node) {
    yaml_event_t event_sequence_start;
    yaml_event_t event_sequence_end;
    int i;
    AV* array = (AV*) SvRV(node);
    int array_size = av_len(array) + 1;
    yaml_sequence_start_event_initialize(
        &event_sequence_start, NULL, NULL, 0, YAML_BLOCK_SEQUENCE_STYLE
    );
    yaml_emitter_emit(emitter, &event_sequence_start);
    for (i = 0; i < array_size; i++) {
        SV** entry = av_fetch(array, i, 0);
        if (entry == NULL)
            dump_node(emitter, &PL_sv_undef);
        else
            dump_node(emitter, *entry);
    }
    yaml_sequence_end_event_initialize(&event_sequence_end);
    yaml_emitter_emit(emitter, &event_sequence_end);
}

void dump_scalar(yaml_emitter_t * emitter, SV* node) {
    yaml_event_t event_scalar;
    char * string = SvPV_nolen(node);
    int length = strlen(string);
    yaml_scalar_event_initialize(
        &event_scalar,
        NULL, NULL,
        (unsigned char *) string, length,
        1, 0, YAML_PLAIN_SCALAR_STYLE
    );
    if (! yaml_emitter_emit(emitter, &event_scalar))
        printf("emit scalar error: %s\n", emitter->problem);
}

int append_output(SV * yaml, unsigned char * buffer, unsigned int size) {
    sv_catpvn(yaml, (const char *) buffer, (STRLEN) size);
}
