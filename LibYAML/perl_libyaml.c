#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#include <yaml.h>

SV* get_node(yaml_parser_t*);
SV* handle_mapping(yaml_parser_t*, yaml_event_t*);
SV* handle_sequence(yaml_parser_t*, yaml_event_t*);
SV* handle_scalar(yaml_parser_t*, yaml_event_t*);

void* _die(char* msg) {
    croak("LibYAML Error - %s", msg);
}

void Load(char* yaml_str) {
    /* printf(">>> Load\n"); */
    dXSARGS; sp = mark;
    yaml_parser_t parser;
    yaml_event_t event;
    SV* node;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(
        &parser,
        (unsigned char *) yaml_str,
        strlen((char *) yaml_str)
    );
    yaml_parser_parse(&parser, &event);
    if (event.type != YAML_STREAM_START_EVENT)
        _die("Expected STREAM_START_EVENT");
    while (event.type != YAML_STREAM_END_EVENT) {
        yaml_parser_parse(&parser, &event);
        if (event.type != YAML_DOCUMENT_START_EVENT)
            _die("Expected DOCUMENT_START_EVENT");
        node = get_node(&parser);
        if (node) XPUSHs(node);
        yaml_parser_parse(&parser, &event);
        if (event.type != YAML_DOCUMENT_END_EVENT)
            _die("Expected DOCUMENT_END_EVENT");
        break;
    }
    /* printf("okokok"); */
    yaml_parser_parse(&parser, &event);
    if (event.type != YAML_STREAM_END_EVENT)
        _die("Expected STREAM_END_EVENT");
    yaml_parser_delete(&parser);
    PUTBACK;
}

SV* get_node(yaml_parser_t * parser) {
    SV* node;
    char* msg;
    /* printf(">>> get_node\n"); */
    yaml_event_t event;
    yaml_parser_parse(parser, &event);

    if (event.type == YAML_DOCUMENT_END_EVENT ||
        event.type == YAML_MAPPING_END_EVENT ||
        event.type == YAML_SEQUENCE_END_EVENT) return NULL;
    node = (event.type == YAML_MAPPING_START_EVENT) ?
        handle_mapping(parser, &event) :
    (event.type == YAML_SEQUENCE_START_EVENT) ?
        handle_sequence(parser, &event) :
    (event.type == YAML_SCALAR_EVENT) ?
        handle_scalar(parser, &event) :
    NULL;
    if (node) return node;

    asprintf(&msg, "Invalid event '%d' at top level", (int) event.type);
    _die(msg);
}

SV* handle_sequence(yaml_parser_t * parser, yaml_event_t * event) {
    SV* node;
    /* printf(">>> handle_sequence\n"); */
    AV* sequence = newAV();
    while (node = get_node(parser)) {
        av_push(sequence, node);
    } 
    /* printf("end of seq\n"); */
    return (SV*) newRV_noinc((SV*) sequence);
}

SV* handle_mapping(yaml_parser_t * parser, yaml_event_t * event) {
    /* printf(">>> handle_mapping\n"); */
    HV* mapping = newHV();
    SV* key_node;
    SV* value_node;
    while (key_node = get_node(parser)) {
        assert(SVPOK(key_node));
        value_node = get_node(parser);
        hv_store(
            mapping, SvPV_nolen(key_node), sv_len(key_node), value_node, 0
        );
    } 
    return (SV*) newRV_noinc((SV*) mapping);
}

SV* handle_scalar(yaml_parser_t * parser, yaml_event_t * event) {
    char* msg;
    asprintf(&msg, ">>> handle_scalar: %s\n", event->data.scalar.value);
    /* printf(msg); */
    return newSVpvf((char *) event->data.scalar.value);
}
