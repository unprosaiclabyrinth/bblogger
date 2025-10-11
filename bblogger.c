#include <dr_api.h>
#include <dr_ir_disassemble.h>
#include <dr_modules.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static file_t log_file;
static void event_exit(void);
static void trace_bb(app_pc app_pc);

/* ============================================================
 *               Optional Basic Block Cache (VVERBOSE)
 * ============================================================ */
#ifdef VVERBOSE
#define DISAS_BUFSZ 1024
#define BB_CACHE_TABLE_BITS 10
#define BB_CACHE_TABLE_SIZE (1u << BB_CACHE_TABLE_BITS)

typedef struct bb_data_t {
    app_pc start_pc;
    char *bbstr;
    struct bb_data_t *next;
} bb_data_t;

static bb_data_t **bb_cache_table = NULL;
static void *bb_cache_lock;

static inline uint hash(app_pc start_pc) {
    return (((ptr_int_t)start_pc) >> 4) & (BB_CACHE_TABLE_SIZE - 1);
}

static void init_bb_cache(void) {
    bb_cache_table = dr_global_alloc(BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    memset(bb_cache_table, 0, BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    bb_cache_lock = dr_mutex_create();
}

static void cache_bb_string(app_pc start_pc, char *bbstr) {
    dr_mutex_lock(bb_cache_lock);
    uint idx = hash(start_pc);
    bb_data_t *e = dr_global_alloc(sizeof(*e));
    e->start_pc = start_pc;
    e->bbstr = bbstr;
    e->next = bb_cache_table[idx];
    bb_cache_table[idx] = e;
    dr_mutex_unlock(bb_cache_lock);
}

static char *lookup_bb_string(app_pc start_pc) {
    dr_mutex_lock(bb_cache_lock);
    uint idx = hash(start_pc);
    for (bb_data_t *e = bb_cache_table[idx]; e; e = e->next) {
        if (e->start_pc == start_pc) {
            char *s = e->bbstr;
            dr_mutex_unlock(bb_cache_lock);
            return s;
        }
    }
    dr_mutex_unlock(bb_cache_lock);
    return NULL;
}

static void free_bb_cache(void) {
    for (uint i = 0; i < BB_CACHE_TABLE_SIZE; i++) {
        bb_data_t *e = bb_cache_table[i];
        while (e) {
            bb_data_t *next = e->next;
            dr_global_free(e->bbstr, strlen(e->bbstr) + 1);
            dr_global_free(e, sizeof(*e));
            e = next;
        }
    }
    dr_global_free(bb_cache_table, BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    dr_mutex_destroy(bb_cache_lock);
    bb_cache_table = NULL;
}
#endif /* VVERBOSE */

/* ============================================================
 *                Clean Call: Per-BB Execution
 * ============================================================ */
static void trace_bb(app_pc app_pc) {
    module_data_t *mod = dr_lookup_module(app_pc);
    thread_id_t tid = dr_get_thread_id(dr_get_current_drcontext());
    uint64 time_us = dr_get_microseconds();

    if (mod) {
        ptr_int_t modrel_pc = (ptr_int_t)(app_pc - mod->start);
        const char *modulestr = mod->names.module_name ?
                                mod->names.module_name :
                                mod->names.file_name;
        (void)modulestr; // dummy usage

#ifdef VVERBOSE
        char *bb = lookup_bb_string(app_pc);
        if (bb) {
            dr_fprintf(log_file,
                "\n[time=%" PRIu64 "us thread=%u] === BB @ <%s> + %#lx ===\n%s",
                time_us, tid, modulestr, modrel_pc, bb);
        }
#else
    #ifdef VERBOSE
        dr_fprintf(log_file,
            "\n[time=%" PRIu64 "us thread=%u] <%s> + %#lx\n",
            time_us, tid, modulestr, modrel_pc);
    #else
        dr_fprintf(log_file,
            "\n[time=%" PRIu64 "us thread=%u] %#lx\n",
            time_us, tid, modrel_pc);
    #endif
#endif
        dr_free_module_data(mod);
    } else {
        dr_fprintf(log_file,
            "\n[time=%" PRIu64 "us thread=%u] <no module> @ %p\n",
            time_us, tid, app_pc);
    }

    /* 🔄 Flush log buffer so tail -f sees updates immediately */
    dr_flush_file(log_file);
}

/* ============================================================
 *          Instrumentation: Per Unique Basic Block
 * ============================================================ */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      __attribute__((unused)) bool for_trace,
                      __attribute__((unused)) bool translating) {

    app_pc start_pc = dr_fragment_app_pc(tag);

#ifdef VVERBOSE
    /* Dynamically disassemble a basic block with no truncation */
    size_t bb_sz = 0;
    size_t bb_cap = 4096;
    char *bb_buf = dr_global_alloc(bb_cap);
    bb_buf[0] = '\0';

    for (instr_t *insn = instrlist_first_app(bb);
        insn != NULL;
        insn = instr_get_next_app(insn)) {

        char disas[DISAS_BUFSZ];
        /* Render textual disassembly into a sufficiently large stack buffer */
        instr_disassemble_to_buffer(drcontext, insn, disas, sizeof(disas));

        size_t need = strlen(disas) + 2; // '\n' + '\0'
        while (bb_sz + need >= bb_cap) {
            size_t old_cap = bb_cap;
            bb_cap *= 2;
            char *newb = dr_global_alloc(bb_cap);
            memcpy(newb, bb_buf, bb_sz);
            dr_global_free(bb_buf, old_cap);
            bb_buf = newb;
        }

        /* Print; if somehow truncated, grow and retry once */
        int w = dr_snprintf(bb_buf + bb_sz, bb_cap - bb_sz, "%s\n", disas);
        if (w < 0 || (size_t)w >= bb_cap - bb_sz) {
            /* grow and retry */
            size_t old_cap = bb_cap;
            do { bb_cap *= 2; } while (bb_sz + need >= bb_cap);
            char *newb = dr_global_alloc(bb_cap);
            memcpy(newb, bb_buf, bb_sz);
            dr_global_free(bb_buf, old_cap);
            bb_buf = newb;
            w = dr_snprintf(bb_buf + bb_sz, bb_cap - bb_sz, "%s\n", disas);
            if (w < 0) w = 0; /* ultra defensive */
        }
        bb_sz += (size_t)w;
        bb_buf[bb_sz] = '\0';
    }

    /* --- Manual completion when DR stopped early --- */
    instr_t *last = instrlist_last_app(bb);
    bool incomplete = (last != NULL) && !instr_is_cti(last);

    if (incomplete) {
        app_pc pc = instr_get_app_pc(last) + instr_length(drcontext, last);
        instr_t dec;

        for (;;) {
            instr_init(drcontext, &dec);
            /* If decode() is flaky here, try decode_from_copy(drcontext, pc, &dec, true) */
            if (!decode(drcontext, pc, &dec))
                break;  /* unmapped/invalid => stop extension */

            char disas[DISAS_BUFSZ];
            instr_disassemble_to_buffer(drcontext, &dec, disas, sizeof(disas));
            size_t need = strlen(disas) + 2;

            while (bb_sz + need >= bb_cap) {
                size_t old_cap = bb_cap;
                bb_cap *= 2;
                char *newb = dr_global_alloc(bb_cap);
                memcpy(newb, bb_buf, bb_sz);
                dr_global_free(bb_buf, old_cap);
                bb_buf = newb;
            }

            int w = dr_snprintf(bb_buf + bb_sz, bb_cap - bb_sz, "%s\n", disas);
            if (w < 0 || (size_t)w >= bb_cap - bb_sz) {
                size_t old_cap = bb_cap;
                do { bb_cap *= 2; } while (bb_sz + need >= bb_cap);
                char *newb = dr_global_alloc(bb_cap);
                memcpy(newb, bb_buf, bb_sz);
                dr_global_free(bb_buf, old_cap);
                bb_buf = newb;
                w = dr_snprintf(bb_buf + bb_sz, bb_cap - bb_sz, "%s\n", disas);
                if (w < 0) w = 0;
            }
            bb_sz += (size_t)w;
            bb_buf[bb_sz] = '\0';

            app_pc next_pc = pc + instr_length(drcontext, &dec);
            bool end = instr_is_cti(&dec);
            instr_free(drcontext, &dec);
            pc = next_pc;
            if (end)
                break;
        }
    }

    /* Ensure newline at end (even for empty BBs) */
    if (bb_sz == 0 || bb_buf[bb_sz - 1] != '\n') {
        if (bb_sz + 2 >= bb_cap) {
            size_t old_cap = bb_cap;
            bb_cap += 2;
            char *newb = dr_global_alloc(bb_cap);
            memcpy(newb, bb_buf, bb_sz);
            dr_global_free(bb_buf, old_cap);
            bb_buf = newb;
        }
        bb_buf[bb_sz++] = '\n';
        bb_buf[bb_sz] = '\0';
    }

    cache_bb_string(start_pc, bb_buf);
#endif

    instr_t *first = instrlist_first_app(bb);
    if (!first) first = instrlist_first(bb);
    dr_insert_clean_call(drcontext, bb, first,
                         (void *)trace_bb, false, 1,
                         OPND_CREATE_INTPTR(start_pc));

    return DR_EMIT_DEFAULT;
}

/* ============================================================
 *                        Cleanup
 * ============================================================ */
static void event_exit(void) {
    dr_close_file(log_file);
#ifdef VVERBOSE
    free_bb_cache();
#endif
}

/* ============================================================
 *                        Entry Point
 * ============================================================ */
DR_EXPORT void
dr_client_main(__attribute__((unused)) client_id_t id, int argc, const char *argv[]) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    const char *log_filename = "bbtrace.log";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            log_filename = argv[++i];
        } else {
            dr_printf("DrBblogger: unknown option '%s'\n", argv[i]);
        }
    }

    log_file = dr_open_file(log_filename,
                            DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_app_instruction);

#ifdef VVERBOSE
    init_bb_cache();
#endif
}
