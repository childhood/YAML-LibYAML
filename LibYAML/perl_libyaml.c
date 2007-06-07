#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"
#include "XSUB.h"
#include <yaml.h>
#include <ppport_sort.h>

static SV*      call_coderef(SV*, AV*);
static SV*      fold_results(I32);
static SV*      find_coderef(char*);

#define TAG_PERL_PREFIX "tag:yaml.org,2002:perl/"
#define TAG_PERL_REF TAG_PERL_PREFIX "ref"
#define TAG_PERL_STR TAG_PERL_PREFIX "str"
#define ERRMSG "LibYAML Error - "

typedef struct {
    yaml_parser_t parser;
    yaml_event_t event;
    HV* anchors;
    int load_code;
} perl_yaml_loader_t;

typedef struct {
    yaml_emitter_t emitter;
    long anchor;
    HV* anchors;
    HV* shadows;
    int dump_code;
} perl_yaml_dumper_t;

void set_dumper_options(perl_yaml_dumper_t*);
void set_loader_options(perl_yaml_dumper_t*);

SV* load_node(perl_yaml_loader_t*);
SV* load_mapping(perl_yaml_loader_t*);
SV* load_sequence(perl_yaml_loader_t*);
SV* load_scalar(perl_yaml_loader_t*);
SV* load_alias(perl_yaml_loader_t*);
SV* load_scalar_ref(perl_yaml_loader_t*);

void dump_prewalk(perl_yaml_dumper_t*, SV*);
void dump_document(perl_yaml_dumper_t*, SV*);
void dump_node(perl_yaml_dumper_t*, SV*);
void dump_hash(perl_yaml_dumper_t *, SV*, yaml_char_t*, yaml_char_t*);
void dump_array(perl_yaml_dumper_t*, SV*);
void dump_scalar(perl_yaml_dumper_t*, SV*);
void dump_ref(perl_yaml_dumper_t*, SV*);
void dump_code(perl_yaml_dumper_t*, SV*);
SV* dump_glob(perl_yaml_dumper_t*, SV*);

yaml_char_t* get_yaml_anchor(perl_yaml_dumper_t*, SV*);
yaml_char_t* get_yaml_tag(SV*);

int append_output(SV *, unsigned char *, unsigned int size);

static SV *call_coderef(SV *code, AV *args) {
    dSP;
    SV **svp;
    I32 count = (args && args != Nullav) ? av_len(args) : -1;
    I32 i;

    PUSHMARK(SP);
    for (i = 0; i <= count; i++)
        if ((svp = av_fetch(args, i, FALSE))) 
            XPUSHs(*svp);
    PUTBACK;
    count = call_sv(code, G_ARRAY);
    SPAGAIN;

    return fold_results(count);
}

static SV* fold_results(I32 count) {
    dSP;
    SV *retval = &PL_sv_undef;

    if (count > 1) {
        /* convert multiple return items into a list reference */
        AV *av = newAV();
        SV *last_sv = &PL_sv_undef;
        SV *sv = &PL_sv_undef;
        I32 i;

        av_extend(av, count - 1);
        for(i = 1; i <= count; i++) {
            last_sv = sv;
            sv = POPs;
            if (SvOK(sv) && !av_store(av, count - i, SvREFCNT_inc(sv)))
                SvREFCNT_dec(sv);
        }
        PUTBACK;

        retval = sv_2mortal((SV *) newRV_noinc((SV *) av));

        if (!SvOK(sv) || sv == &PL_sv_undef) {
            /* if first element was undef, die */
            croak(ERRMSG "Call error");
        }
        return retval;

    } else {
        if (count)
            retval = POPs;
        PUTBACK;
        return retval;
    }
}

static SV *find_coderef(char *perl_var) {
    SV *coderef;

    if ((coderef = get_sv(perl_var, FALSE)) 
        && SvROK(coderef) 
        && SvTYPE(SvRV(coderef)) == SVt_PVCV)
        return coderef;

    return NULL;
}

void Load(char* yaml_str) {
    dXSARGS; sp = mark;
    perl_yaml_loader_t loader;
    SV* node;
    yaml_parser_initialize(&loader.parser);
    yaml_parser_set_input_string(
        &loader.parser,
        (unsigned char *) yaml_str,
        strlen((char *) yaml_str)
    );
    yaml_parser_parse(&loader.parser, &loader.event);
    if (loader.event.type != YAML_STREAM_START_EVENT)
        croak(ERRMSG "Expected STREAM_START_EVENT");
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
            croak(ERRMSG "Expected DOCUMENT_END_EVENT");
    }
    if (loader.event.type != YAML_STREAM_END_EVENT) {
        croak(ERRMSG "Expected STREAM_END_EVENT; Got: %d != %d",
            loader.event.type,
            YAML_STREAM_END_EVENT
         );
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
        if (tag && strEQ(tag, TAG_PERL_REF))
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

    croak(ERRMSG "Invalid event '%d' at top level", (int) loader->event.type);
}

SV* load_mapping(perl_yaml_loader_t * loader) {
    SV* key_node;
    SV* value_node;
    HV* hash = newHV();
    SV* hash_ref = (SV*) newRV_noinc((SV*) hash);
    char * anchor = (char *)loader->event.data.mapping_start.anchor;
    char * tag = (char *)loader->event.data.mapping_start.tag;
    if (anchor)
        hv_store(loader->anchors, anchor, strlen(anchor), hash_ref, 0);
    while (key_node = load_node(loader)) {
        assert(SVPOK(key_node));
        value_node = load_node(loader);
        hv_store(
            hash, SvPV_nolen(key_node), sv_len(key_node), value_node, 0
        );
    } 
    if (tag) {
        char* prefix = TAG_PERL_PREFIX "hash:";
        if (strlen(tag) <= strlen(prefix) ||
            ! strnEQ(tag, prefix, strlen(prefix))
        ) croak(ERRMSG "Bad tag found for hash");
        char* class = tag + strlen(prefix);
        sv_bless(hash_ref, gv_stashpv(class, TRUE)); 
    }
    return hash_ref;
}

SV* load_sequence(perl_yaml_loader_t * loader) {
    SV* node;
    AV* array = newAV();
    SV* array_ref = (SV*) newRV_noinc((SV*) array);
    char * key = (char *)loader->event.data.sequence_start.anchor;
    char * tag = (char *)loader->event.data.mapping_start.tag;
    if (key)
        hv_store(loader->anchors, key, strlen(key), array_ref, 0);
    while (node = load_node(loader)) {
        av_push(array, node);
    } 
    if (tag) {
        char* prefix = TAG_PERL_PREFIX "array:";
        if (strlen(tag) <= strlen(prefix) ||
            ! strnEQ(tag, prefix, strlen(prefix))
        ) croak(ERRMSG "Bad tag found for array");
        char* class = tag + strlen(prefix);
        sv_bless(array_ref, gv_stashpv(class, TRUE)); 
    }
    return array_ref;
}

SV* load_scalar(perl_yaml_loader_t * loader) {
    char * string = (char *) loader->event.data.scalar.value;
    if (loader->event.data.scalar.style == YAML_PLAIN_SCALAR_STYLE) {
        if (strEQ(string, "~")) {
            return &PL_sv_undef;
        }
        else if (strEQ(string, "")) {
            return &PL_sv_undef;
        }
        else if (strEQ(string, "true")) {
            return &PL_sv_yes;
        }
        else if (strEQ(string, "false")) {
            return &PL_sv_no;
        }
    }
    return newSVpvn(string, strlen(string));
}

SV* load_alias(perl_yaml_loader_t * loader) {
    char * anchor = (char *) loader->event.data.alias.anchor;
    SV** entry = hv_fetch(loader->anchors, anchor, strlen(anchor), 0);
    if (entry) return *entry;
    croak(ERRMSG "No anchor for alias '%s'", anchor);
}

SV* load_scalar_ref(perl_yaml_loader_t * loader) {
    SV* value_node;
    char * anchor = (char *)loader->event.data.mapping_start.anchor;
    SV* rv = newRV_noinc(&PL_sv_undef);
    if (anchor) {
        hv_store(loader->anchors, anchor, strlen(anchor), rv, 0);
    }
    load_node(loader);  // Load the single hash key (=)
    value_node = load_node(loader);
    SvRV(rv) = value_node;
    if (load_node(loader)) 
        croak(ERRMSG "Expected end of node");
    return rv;
}

/* -------------------------------------------------------------------------- */

void set_dumper_options(perl_yaml_dumper_t* dumper) {
    GV* gv;
    dumper->dump_code = (
        (gv = gv_fetchpv("YAML::LibYAML::UseCode", TRUE, SVt_PV)) &&
        SvTRUE(GvSV(gv))
    ||
        (gv = gv_fetchpv("YAML::LibYAML::DumpCode", TRUE, SVt_PV)) &&
        SvTRUE(GvSV(gv))
    );
}

SV* Dump(SV * dummy, ...) {
    perl_yaml_dumper_t dumper;
    yaml_event_t event_stream_start;
    yaml_event_t event_stream_end;
    dXSARGS; sp = mark;
    int i;
    SV* yaml = newSVpvn("", 0);
    set_dumper_options(&dumper);

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
    yaml_emitter_emit(&dumper.emitter, &event_stream_start);
    for (i = 0; i < items; i++) {
        dumper.anchor = 1;
        dumper.anchors = newHV();
        dumper.shadows = newHV();

        dump_prewalk(&dumper, ST(i));
        dump_document(&dumper, ST(i));

        sv_dec((SV*)dumper.anchors);
        sv_dec((SV*)dumper.shadows);
    }
    yaml_stream_end_event_initialize(&event_stream_end);
    yaml_emitter_emit(&dumper.emitter, &event_stream_end);
    yaml_emitter_delete(&dumper.emitter);
    if (yaml) XPUSHs(yaml);
    PUTBACK;
}

void dump_prewalk(perl_yaml_dumper_t * dumper, SV* node) {
    int i, len;

    if (! (SvROK(node) || SvTYPE(node) == SVt_PVGV)) return;

    SV* object = SvROK(node) ? SvRV(node) : node;
    SV** seen = hv_fetch(dumper->anchors, (char *)&object, sizeof(object), 0);
    if (seen) {
        if (*seen == &PL_sv_undef) {
            hv_store(
                dumper->anchors, (char *)&object, sizeof(object),
                newSViv(dumper->anchor), 0
            );
            dumper->anchor++;
        }
        return;
    }
    hv_store(dumper->anchors, (char *)&object, sizeof(object), &PL_sv_undef, 0);

    if (SvTYPE(node) == SVt_PVGV) {
        node = dump_glob(dumper, node);
    }

    U32 ref_type = SvTYPE(SvRV(node));
    if (ref_type == SVt_PVAV) {
        AV* array = (AV*) SvRV(node);
        int array_size = av_len(array) + 1;
        for (i = 0; i < array_size; i++) {
            SV** entry = av_fetch(array, i, 0);
            if (entry)
                dump_prewalk(dumper, *entry);
        }
    }
    else if (ref_type == SVt_PVHV) {
        HV* hash = (HV*) SvRV(node);
        len = HvKEYS(hash);
        hv_iterinit(hash);
        for (i = 0; i < len; i++) {
            HE *he = hv_iternext(hash);
            SV *key = hv_iterkeysv(he);
            SV *val = HeVAL(he);
            if (val)
                dump_prewalk(dumper, val);
        }
    }
    else if (ref_type <= SVt_PVNV) {
        SV* scalar = SvRV(node);
        dump_prewalk(dumper, scalar);
    }
}

void dump_document(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_document_start;
    yaml_event_t event_document_end;
    yaml_document_start_event_initialize(
        &event_document_start, NULL, NULL, NULL, 0
    );
    yaml_emitter_emit(&dumper->emitter, &event_document_start);
    dump_node(dumper, node);
    yaml_document_end_event_initialize(&event_document_end, 1);
    yaml_emitter_emit(&dumper->emitter, &event_document_end);
}

void dump_node(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_char_t* anchor = NULL;
    yaml_char_t* tag = NULL;
    if (SvTYPE(node) == SVt_PVGV) {
        tag = (yaml_char_t*)TAG_PERL_PREFIX "glob";
        if (!anchor) {
            anchor = get_yaml_anchor(dumper, node);
        }
        if (anchor && strEQ((char*)anchor, "")) return;
        SV** svr = hv_fetch(dumper->shadows, (char *)&node, sizeof(node), 0);
        if (svr) {
            node = *svr;
        }
    }

    if (SvROK(node)) {
        U32 ref_type = SvTYPE(SvRV(node));
        if (ref_type == SVt_PVHV) {
            dump_hash(dumper, node, anchor, tag);
        }
        else if (ref_type == SVt_PVAV) {
            dump_array(dumper, node);
        }
        else if (ref_type <= SVt_PVNV) {
            dump_ref(dumper, node);
        }
        else if (ref_type == SVt_PVCV) {
            dump_code(dumper, node);
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

yaml_char_t* get_yaml_anchor(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_alias;
    yaml_char_t* anchor = NULL;
    SV** seen = hv_fetch(dumper->anchors, (char *)&node, sizeof(node), 0);
    if (seen && *seen != &PL_sv_undef) {
        anchor = (yaml_char_t*)SvPV_nolen(*seen);
        if (SvREADONLY(*seen)) {
            yaml_alias_event_initialize(&event_alias, anchor);
            yaml_emitter_emit(&dumper->emitter, &event_alias);
            return (yaml_char_t *) "";
        }
        SvREADONLY_on(*seen);
        return anchor;
    }
    return NULL;
}

yaml_char_t* get_yaml_tag(SV* node) {
    if (! (
        sv_isobject(node) || 
        SvRV(node) && (
            SvTYPE(SvRV(node)) == SVt_PVCV
        )
    )) return NULL;
    char* ref = sv_reftype(SvRV(node), TRUE);
    char* type = "";

    switch (SvTYPE(SvRV(node))) {
        case SVt_PVAV: { type = "array"; break; }
        case SVt_PVHV: { type = "hash"; break; }
        case SVt_PVCV: { type = "code"; break; }
        case SVt_PVGV: { type = "glob"; break; }

/* flatten scalar ref objects so that they dump as !perl/scalar:Foo::Bar foo */
        case SVt_PVMG: {
            if ( !SvROK(SvRV(node)) ) {
                type = "scalar";
                node = SvRV(node);
                break;
            } else {
                type = "ref";
                break;
            }
        }
    }
    yaml_char_t* tag;
    if ((strlen(type) == 0))
        tag = (yaml_char_t*) form("%s%s", TAG_PERL_PREFIX, ref);
    else if (SvTYPE(SvRV(node)) == SVt_PVCV && strEQ(ref, "CODE"))
        tag = (yaml_char_t*) form("%s%s", TAG_PERL_PREFIX, type);
    else
        tag = (yaml_char_t*) form("%s%s:%s", TAG_PERL_PREFIX, type, ref);
    return tag;
} 

void dump_hash(
    perl_yaml_dumper_t * dumper, SV* node,
    yaml_char_t* anchor, yaml_char_t* tag
) {
    yaml_event_t event_mapping_start;
    yaml_event_t event_mapping_end;
    int i;
    int len;
    HV* hash = (HV*) SvRV(node);
    len = HvKEYS(hash);
    hv_iterinit(hash);

    if (!anchor)
        anchor = get_yaml_anchor(dumper, (SV*)hash);
    if (anchor && strEQ((char*)anchor, "")) return;

    if (!tag)
        tag = get_yaml_tag(node);
    
    yaml_mapping_start_event_initialize(
        &event_mapping_start, anchor, tag, 0, YAML_BLOCK_MAPPING_STYLE
    );
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
    yaml_emitter_emit(&dumper->emitter, &event_mapping_end);
}

void dump_array(perl_yaml_dumper_t * dumper, SV * node) {
    yaml_event_t event_sequence_start;
    yaml_event_t event_sequence_end;
    yaml_event_t event_alias;
    int i;
    AV* array = (AV*) SvRV(node);
    int array_size = av_len(array) + 1;

    yaml_char_t* anchor = get_yaml_anchor(dumper, (SV*)array);
    if (anchor && strEQ((char*)anchor, "")) return;

    yaml_char_t* tag = get_yaml_tag(node);
    
    yaml_sequence_start_event_initialize(
        &event_sequence_start, anchor, tag, 0, YAML_BLOCK_SEQUENCE_STYLE
    );

    yaml_emitter_emit(&dumper->emitter, &event_sequence_start);
    for (i = 0; i < array_size; i++) {
        SV** entry = av_fetch(array, i, 0);
        if (entry == NULL)
            dump_node(dumper, &PL_sv_undef);
        else
            dump_node(dumper, *entry);
    }
    yaml_sequence_end_event_initialize(&event_sequence_end);
    yaml_emitter_emit(&dumper->emitter, &event_sequence_end);
}

void dump_scalar(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_scalar;
    char * string;
    yaml_char_t* tag = (yaml_char_t*) TAG_PERL_STR;
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
            strEQ(string, "~") ||
            strEQ(string, "true") ||
            strEQ(string, "false") ||
            strEQ(string, "null") ||
            (type >= SVt_PVGV)
        ) {
            style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
        }
    }
    int length = strlen(string);
    yaml_scalar_event_initialize(
        &event_scalar,
        NULL,
        tag,
        (unsigned char *) string,
        length,
        1,
        1,
        style
    );
    yaml_emitter_emit(&dumper->emitter, &event_scalar) ||
        printf(
            "emit scalar '%s', error: %s\n", string, dumper->emitter.problem
        );
}

void dump_code(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_scalar;
    yaml_scalar_style_t style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
    char * string = "{ \"DUMMY\" }";
    if (dumper->dump_code) {
        // load_module(PERL_LOADMOD_NOIMPORT, newSVpv("B::Deparse", 0), NULL);
        SV* code = find_coderef("YAML::LibYAML::coderef2text");
        AV* args = newAV();
        av_push(args, SvREFCNT_inc(node));
        args = (AV *) sv_2mortal((SV *) args);
        SV* result = call_coderef(code, args);
        if (result && result != &PL_sv_undef) {
            string = SvPV_nolen(result);
            style = YAML_LITERAL_SCALAR_STYLE;
        }
    }
    int length = strlen(string);
    svtype type = SvTYPE(node);
    yaml_char_t* tag = get_yaml_tag(node);
    
    yaml_scalar_event_initialize(
        &event_scalar,
        NULL,
        tag,
        (unsigned char *) string,
        strlen(string),
        0,
        0,
        style
    );

    yaml_emitter_emit(&dumper->emitter, &event_scalar);
}

SV* dump_glob(perl_yaml_dumper_t * dumper, SV* node) {
    SV* code = find_coderef("YAML::LibYAML::glob2hash");
    AV* args = newAV();
    av_push(args, SvREFCNT_inc(node));
    args = (AV *) sv_2mortal((SV *) args);
    SV* result = call_coderef(code, args);
    hv_store(
        dumper->shadows, (char *)&node, sizeof(node),
        result, 0
    );
    return result;
}

// XXX Refo this to just dump a special map
void dump_ref(perl_yaml_dumper_t * dumper, SV* node) {
    yaml_event_t event_mapping_start;
    yaml_event_t event_mapping_end;
    yaml_event_t event_scalar;
    yaml_event_t event_alias;
    int i;
    int len;
    SV* referent = SvRV(node);

    yaml_char_t* anchor = get_yaml_anchor(dumper, referent);
    if (anchor && strEQ((char*)anchor, "")) return;

    yaml_mapping_start_event_initialize(
        &event_mapping_start, anchor,
        (unsigned char *) TAG_PERL_PREFIX "ref",
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
