/* ------------------------------------------------------------------------- */
/*   "asm" : The Inform assembler                                            */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

uchar *zcode_holding_area;         /* Area holding code yet to be transferred
                                      to either zcode_area or temp file no 1 */
uchar *zcode_markers;              /* Bytes holding marker values for this
                                      code                                   */
static int zcode_ha_size;          /* Number of bytes in holding area        */

memory_block zcode_area;           /* Block to hold assembled code (if
                                      temporary files are not being used)    */

int32 zmachine_pc;                 /* PC position of assembly (byte offset
                                      from start of Z-code area)             */

int32 no_instructions;             /* Number of instructions assembled       */
int execution_never_reaches_here,  /* TRUE if the current PC value in the
                                      code area cannot be reached: e.g. if
                                      the previous instruction was a "quit"
                                      opcode and no label is set to here     */
    next_label,                    /* Used to count the labels created all
                                      over Inform in current routine, from 0 */
    next_sequence_point;           /* Likewise, for sequence points          */
int no_sequence_points;            /* Kept for statistics purposes only      */

static int label_moved_error_already_given;
                                   /* When one label has moved, all subsequent
                                      ones probably have too, and this flag
                                      suppresses the runaway chain of error
                                      messages which would otherwise result  */

int  sequence_point_follows;       /* Will the next instruction assembled    */
                                   /* be at a sequence point in the routine? */

dbgl debug_line_ref;               /* Source code ref of current statement   */


int32 variable_tokens[256];        /* The entries 16 to 255 give the symbol
                                      table index for the names of the
                                      global variables                       */
int variable_usage[256];           /* TRUE if referred to, FALSE otherwise   */

assembly_instruction AI;           /* A structure used to hold the full
                                      specification of a single Z-code
                                      instruction: effectively this is the
                                      input to the routine
                                      assemble_instruction()                 */

static char opcode_syntax_string[128];  /*  Text buffer holding the correct
                                      syntax for an opcode: used to produce
                                      helpful assembler error messages       */

static int routine_locals;         /* The number of local variables used by
			   	      the routine currently being compiled   */

static int32 routine_start_pc;

/* ------------------------------------------------------------------------- */
/*   Label data                                                              */
/* ------------------------------------------------------------------------- */

static int first_label, last_label;
static int32 *label_offsets;       /* Double-linked list of label offsets    */
static int   *label_next,          /* (i.e. zmachine_pc values) in PC order  */
             *label_prev;
static int32 *label_symbols;       /* Symbol numbers if defined in source    */

static int   *sequence_point_labels;  /* Label numbers for each              */
static dbgl  *sequence_point_refs;    /* Source code references for each     */
                                      /* (used for making debugging file)    */

static void set_label_offset(int label, int32 offset)
{
    if (label >= MAX_LABELS) memoryerror("MAX_LABELS", MAX_LABELS);

    label_offsets[label] = offset;
    if (last_label == -1)
    {   label_prev[label] = -1;
        first_label = label;
    }
    else
    {   label_prev[label] = last_label;
        label_next[last_label] = label;
    }
    last_label = label;
    label_next[label] = -1;
    label_symbols[label] = -1;
}

/* ------------------------------------------------------------------------- */
/*   Used in printing assembly traces                                        */
/* ------------------------------------------------------------------------- */

extern char *variable_name(int32 i)
{
    if (i==0) return("sp");
    if (i<16) return local_variable_texts[i-1];

    if (i==255) return("TEMP1");
    if (i==254) return("TEMP2");
    if (i==253) return("TEMP3");
    if (i==252) return("TEMP4");
    if (i==251) return("self");
    if (i==250) return("sender");
    if (i==249) return("sw__var");

    return ((char *) symbs[variable_tokens[i]]);
}

extern void print_operand(assembly_operand o)
{   switch(o.type)
    {   case EXPRESSION_OT: printf("expr_"); break;
        case LONG_CONSTANT_OT: printf("long_"); break;
        case SHORT_CONSTANT_OT: printf("short_"); break;
        case VARIABLE_OT:
             if (o.value==0) { printf("sp"); return; }
             printf("%s", variable_name(o.value)); return;
        case OMITTED_OT: printf("<no value>"); return;
    }
    printf("%d", o.value);
}

/* ------------------------------------------------------------------------- */
/*   Writing bytes to the code area                                          */
/* ------------------------------------------------------------------------- */

static void byteout(int32 i, int mv)
{   if (zcode_ha_size >= MAX_ZCODE_SIZE)
        memoryerror("MAX_ZCODE_SIZE",MAX_ZCODE_SIZE);
    zcode_markers[zcode_ha_size] = (uchar) mv;
    zcode_holding_area[zcode_ha_size++] = (uchar) i;
    zmachine_pc++;
}

/* ------------------------------------------------------------------------- */
/*   A database of the 115 canonical Infocom opcodes in Versions 3 to 6      */
/* ------------------------------------------------------------------------- */

typedef struct opcode
{   uchar *name;        /* Lower case standard name */
    int version1;     /* Valid from this version number... */
    int version2;     /* ...until this one (or forever if this is 0) */
    int extension;    /* In later versions, see this line in extension table:
                         if -1, the opcode is illegal in later versions */
    int code;         /* Opcode number within its operand-number block */
    int flags;        /* Flags (see below) */
    int op_rules;     /* Any unusual operand rule applying (see below) */
    int flags2_set;   /* If not zero, set this bit in Flags 2 in the header
                         of any game using the opcode */
    int no;           /* Number of operands (see below) */
} opcode;

    /* Flags which can be set */

#define St      1     /* Store */
#define Br      2     /* Branch */
#define Rf      4     /* "Return flag": execution never continues after this
                         opcode (e.g., is a return or unconditional jump) */

    /* Codes for any unusual operand assembly rules */

#define VARIAB   1    /* First operand expected to be a variable name and
                         assembled to a short constant: the variable number */
#define TEXT     2    /* One text operand, to be Z-encoded into the program */
#define LABEL    3    /* One operand, a label, given as long constant offset */
#define CALL     4    /* First operand is name of a routine, to be assembled
                         as long constant (the routine's packed address):
                         as if the name were prefixed by #r$ */

    /* Codes for the number of operands */

#define TWO      1    /* 2 (with certain types of operand, compiled as VAR) */
#define VAR      2    /* 0 to 4 */
#define VAR_LONG 3    /* 0 to 8 */
#define ONE      4    /* 1 */
#define ZERO     5    /* 0 */
#define EXT      6    /* Extended opcode set VAR: 0 to 4 */
#define EXT_LONG 7    /* Extended: 0 to 8 (not used by the canonical opcodes) */

static opcode opcodes_table[] =
{
    /* Opcodes introduced in Version 3 */

/* 0 */ { (uchar *) "je",              3, 0, -1, 0x01,     Br,      0, 0, TWO },
/* 1 */ { (uchar *) "jl",              3, 0, -1, 0x02,     Br,      0, 0, TWO },
/* 2 */ { (uchar *) "jg",              3, 0, -1, 0x03,     Br,      0, 0, TWO },
/* 3 */ { (uchar *) "dec_chk",         3, 0, -1, 0x04,     Br, VARIAB, 0, TWO },
/* 4 */ { (uchar *) "inc_chk",         3, 0, -1, 0x05,     Br, VARIAB, 0, TWO },
/* 5 */ { (uchar *) "jin",             3, 0, -1, 0x06,     Br,      0, 0, TWO },
/* 6 */ { (uchar *) "test",            3, 0, -1, 0x07,     Br,      0, 0, TWO },
/* 7 */ { (uchar *) "or",              3, 0, -1, 0x08,     St,      0, 0, TWO },
/* 8 */ { (uchar *) "and",             3, 0, -1, 0x09,     St,      0, 0, TWO },
/* 9 */ { (uchar *) "test_attr",       3, 0, -1, 0x0A,     Br,      0, 0, TWO },
/* 10 */ {(uchar *) "set_attr",        3, 0, -1, 0x0B,      0,      0, 0, TWO },
/* 11 */ {(uchar *) "clear_attr",      3, 0, -1, 0x0C,      0,      0, 0, TWO },
/* 12 */ {(uchar *) "store",           3, 0, -1, 0x0D,      0, VARIAB, 0, TWO },
/* 13 */ {(uchar *) "insert_obj",      3, 0, -1, 0x0E,      0,      0, 0, TWO },
/* 14 */ {(uchar *) "loadw",           3, 0, -1, 0x0F,     St,      0, 0, TWO },
/* 15 */ {(uchar *) "loadb",           3, 0, -1, 0x10,     St,      0, 0, TWO },
/* 16 */ {(uchar *) "get_prop",        3, 0, -1, 0x11,     St,      0, 0, TWO },
/* 17 */ {(uchar *) "get_prop_addr",   3, 0, -1, 0x12,     St,      0, 0, TWO },
/* 18 */ {(uchar *) "get_next_prop",   3, 0, -1, 0x13,     St,      0, 0, TWO },
/* 19 */ {(uchar *) "add",             3, 0, -1, 0x14,     St,      0, 0, TWO },
/* 20 */ {(uchar *) "sub",             3, 0, -1, 0x15,     St,      0, 0, TWO },
/* 21 */ {(uchar *) "mul",             3, 0, -1, 0x16,     St,      0, 0, TWO },
/* 22 */ {(uchar *) "div",             3, 0, -1, 0x17,     St,      0, 0, TWO },
/* 23 */ {(uchar *) "mod",             3, 0, -1, 0x18,     St,      0, 0, TWO },
/* 24 */ {(uchar *) "call",            3, 0, -1, 0x20,     St,   CALL, 0, VAR },
/* 25 */ {(uchar *) "storew",          3, 0, -1, 0x21,      0,      0, 0, VAR },
/* 26 */ {(uchar *) "storeb",          3, 0, -1, 0x22,      0,      0, 0, VAR },
/* 27 */ {(uchar *) "put_prop",        3, 0, -1, 0x23,      0,      0, 0, VAR },
            /* This is the version of "read" called "sread" internally: */
/* 28 */ {(uchar *) "read",            3, 0, -1, 0x24,      0,      0, 0, VAR },
/* 29 */ {(uchar *) "print_char",      3, 0, -1, 0x25,      0,      0, 0, VAR },
/* 30 */ {(uchar *) "print_num",       3, 0, -1, 0x26,      0,      0, 0, VAR },
/* 31 */ {(uchar *) "random",          3, 0, -1, 0x27,     St,      0, 0, VAR },
/* 32 */ {(uchar *) "push",            3, 0, -1, 0x28,      0,      0, 0, VAR },
/* 33 */ {(uchar *) "pull",            3, 5,  6, 0x29,      0, VARIAB, 0, VAR },
/* 34 */ {(uchar *) "split_window",    3, 0, -1, 0x2A,      0,      0, 0, VAR },
/* 35 */ {(uchar *) "set_window",      3, 0, -1, 0x2B,      0,      0, 0, VAR },
/* 36 */ {(uchar *) "output_stream",   3, 0, -1, 0x33,      0,      0, 0, VAR },
/* 37 */ {(uchar *) "input_stream",    3, 0, -1, 0x34,      0,      0, 0, VAR },
/* 38 */ {(uchar *) "sound_effect",    3, 0, -1, 0x35,      0,      0, 7, VAR },
/* 39 */ {(uchar *) "jz",              3, 0, -1, 0x00,     Br,      0, 0, ONE },
/* 40 */ {(uchar *) "get_sibling",     3, 0, -1, 0x01,  St+Br,      0, 0, ONE },
/* 41 */ {(uchar *) "get_child",       3, 0, -1, 0x02,  St+Br,      0, 0, ONE },
/* 42 */ {(uchar *) "get_parent",      3, 0, -1, 0x03,     St,      0, 0, ONE },
/* 43 */ {(uchar *) "get_prop_len",    3, 0, -1, 0x04,     St,      0, 0, ONE },
/* 44 */ {(uchar *) "inc",             3, 0, -1, 0x05,      0, VARIAB, 0, ONE },
/* 45 */ {(uchar *) "dec",             3, 0, -1, 0x06,      0, VARIAB, 0, ONE },
/* 46 */ {(uchar *) "print_addr",      3, 0, -1, 0x07,      0,      0, 0, ONE },
/* 47 */ {(uchar *) "remove_obj",      3, 0, -1, 0x09,      0,      0, 0, ONE },
/* 48 */ {(uchar *) "print_obj",       3, 0, -1, 0x0A,      0,      0, 0, ONE },
/* 49 */ {(uchar *) "ret",             3, 0, -1, 0x0B,     Rf,      0, 0, ONE },
/* 50 */ {(uchar *) "jump",            3, 0, -1, 0x0C,     Rf,  LABEL, 0, ONE },
/* 51 */ {(uchar *) "print_paddr",     3, 0, -1, 0x0D,      0,      0, 0, ONE },
/* 52 */ {(uchar *) "load",            3, 0, -1, 0x0E,     St, VARIAB, 0, ONE },
/* 53 */ {(uchar *) "not",             3, 3,  0, 0x0F,     St,      0, 0, ONE },
/* 54 */ {(uchar *) "rtrue",           3, 0, -1, 0x00,     Rf,      0, 0,ZERO },
/* 55 */ {(uchar *) "rfalse",          3, 0, -1, 0x01,     Rf,      0, 0,ZERO },
/* 56 */ {(uchar *) "print",           3, 0, -1, 0x02,      0,   TEXT, 0,ZERO },
/* 57 */ {(uchar *) "print_ret",       3, 0, -1, 0x03,     Rf,   TEXT, 0,ZERO },
/* 58 */ {(uchar *) "nop",             3, 0, -1, 0x04,      0,      0, 0,ZERO },
/* 59 */ {(uchar *) "save",            3, 3,  1, 0x05,     Br,      0, 0,ZERO },
/* 60 */ {(uchar *) "restore",         3, 3,  2, 0x06,     Br,      0, 0,ZERO },
/* 61 */ {(uchar *) "restart",         3, 0, -1, 0x07,      0,      0, 0,ZERO },
/* 62 */ {(uchar *) "ret_popped",      3, 0, -1, 0x08,     Rf,      0, 0,ZERO },
/* 63 */ {(uchar *) "pop",             3, 0, -1, 0x09,      0,      0, 0,ZERO },
/* 64 */ {(uchar *) "quit",            3, 0, -1, 0x0A,     Rf,      0, 0,ZERO },
/* 65 */ {(uchar *) "new_line",        3, 0, -1, 0x0B,      0,      0, 0,ZERO },
/* 66 */ {(uchar *) "show_status",     3, 3, -1, 0x0C,      0,      0, 0,ZERO },
/* 67 */ {(uchar *) "verify",          3, 0, -1, 0x0D,     Br,      0, 0,ZERO },

    /* Opcodes introduced in Version 4 */

/* 68 */ {(uchar *) "call_2s",         4, 0, -1, 0x19,     St,   CALL, 0, TWO },
/* 69 */ {(uchar *) "call_vs",         4, 0, -1, 0x20,     St,   CALL, 0, VAR },
            /* This is the version of "read" called "aread" internally: */
/* 70 */ {(uchar *) "read",            4, 0, -1, 0x24,     St,      0, 0, VAR },
/* 71 */ {(uchar *) "call_vs2",        4, 0, -1, 0x2C,     St,   CALL, 0,
                                                                     VAR_LONG },
/* 72 */ {(uchar *) "erase_window",    4, 0, -1, 0x2D,      0,      0, 0, VAR },
/* 73 */ {(uchar *) "erase_line",      4, 0, -1, 0x2E,      0,      0, 0, VAR },
/* 74 */ {(uchar *) "set_cursor",      4, 0, -1, 0x2F,      0,      0, 0, VAR },
/* 75 */ {(uchar *) "get_cursor",      4, 0, -1, 0x30,      0,      0, 0, VAR },
/* 76 */ {(uchar *) "set_text_style",  4, 0, -1, 0x31,      0,      0, 0, VAR },
/* 77 */ {(uchar *) "buffer_mode",     4, 0, -1, 0x32,      0,      0, 0, VAR },
/* 78 */ {(uchar *) "read_char",       4, 0, -1, 0x36,     St,      0, 0, VAR },
/* 79 */ {(uchar *) "scan_table",      4, 0, -1, 0x37,  St+Br,      0, 0, VAR },
/* 80 */ {(uchar *) "call_1s",         4, 0, -1, 0x08,     St,   CALL, 0, ONE },

    /* Opcodes introduced in Version 5 */

/* 81 */ {(uchar *) "call_2n",         5, 0, -1, 0x1a,      0,   CALL, 0, TWO },
/* 82 */ {(uchar *) "set_colour",      5, 0, -1, 0x1b,      0,      0, 6, TWO },
/* 83 */ {(uchar *) "throw",           5, 0, -1, 0x1c,      0,      0, 0, TWO },
/* 84 */ {(uchar *) "call_vn",         5, 0, -1, 0x39,      0,   CALL, 0, VAR },
/* 85 */ {(uchar *) "call_vn2",        5, 0, -1, 0x3a,      0,   CALL, 0,
                                                                     VAR_LONG },
/* 86 */ {(uchar *) "tokenise",        5, 0, -1, 0x3b,      0,      0, 0, VAR },
/* 87 */ {(uchar *) "encode_text",     5, 0, -1, 0x3c,      0,      0, 0, VAR },
/* 88 */ {(uchar *) "copy_table",      5, 0, -1, 0x3d,      0,      0, 0, VAR },
/* 89 */ {(uchar *) "print_table",     5, 0, -1, 0x3e,      0,      0, 0, VAR },
/* 90 */ {(uchar *) "check_arg_count", 5, 0, -1, 0x3f,     Br,      0, 0, VAR },
/* 91 */ {(uchar *) "call_1n",         5, 0, -1, 0x0F,      0,   CALL, 0, ONE },
/* 92 */ {(uchar *) "catch",           5, 0, -1, 0x09,     St,      0, 0, ZERO },
/* 93 */ {(uchar *) "piracy",          5, 0, -1, 0x0F,     Br,      0, 0, ZERO },
/* 94 */ {(uchar *) "log_shift",       5, 0, -1, 0x02,     St,      0, 0, EXT },
/* 95 */ {(uchar *) "art_shift",       5, 0, -1, 0x03,     St,      0, 0, EXT },
/* 96 */ {(uchar *) "set_font",        5, 0, -1, 0x04,     St,      0, 0, EXT },
/* 97 */ {(uchar *) "save_undo",       5, 0, -1, 0x09,     St,      0, 4, EXT },
/* 98 */ {(uchar *) "restore_undo",    5, 0, -1, 0x0A,     St,      0, 4, EXT },

    /* Opcodes introduced in Version 6 */

/* 99 */  { (uchar *) "draw_picture",  6, 6, -1, 0x05,      0,      0, 3, EXT },
/* 100 */ { (uchar *) "picture_data",  6, 6, -1, 0x06,     Br,      0, 3, EXT },
/* 101 */ { (uchar *) "erase_picture", 6, 6, -1, 0x07,      0,      0, 3, EXT },
/* 102 */ { (uchar *) "set_margins",   6, 6, -1, 0x08,      0,      0, 0, EXT },
/* 103 */ { (uchar *) "move_window",   6, 6, -1, 0x10,      0,      0, 0, EXT },
/* 104 */ { (uchar *) "window_size",   6, 6, -1, 0x11,      0,      0, 0, EXT },
/* 105 */ { (uchar *) "window_style",  6, 6, -1, 0x12,      0,      0, 0, EXT },
/* 106 */ { (uchar *) "get_wind_prop", 6, 6, -1, 0x13,     St,      0, 0, EXT },
/* 107 */ { (uchar *) "scroll_window", 6, 6, -1, 0x14,      0,      0, 0, EXT },
/* 108 */ { (uchar *) "pop_stack",     6, 6, -1, 0x15,      0,      0, 0, EXT },
/* 109 */ { (uchar *) "read_mouse",    6, 6, -1, 0x16,      0,      0, 5, EXT },
/* 110 */ { (uchar *) "mouse_window",  6, 6, -1, 0x17,      0,      0, 5, EXT },
/* 111 */ { (uchar *) "push_stack",    6, 6, -1, 0x18,     Br,      0, 0, EXT },
/* 112 */ { (uchar *) "put_wind_prop", 6, 6, -1, 0x19,      0,      0, 0, EXT },
/* 113 */ { (uchar *) "print_form",    6, 6, -1, 0x1a,      0,      0, 0, EXT },
/* 114 */ { (uchar *) "make_menu",     6, 6, -1, 0x1b,     Br,      0, 8, EXT },
/* 115 */ { (uchar *) "picture_table", 6, 6, -1, 0x1c,      0,      0, 3, EXT }
};

    /* Subsequent forms for opcodes whose meaning changes with version */

static opcode extension_table[] =
{
/* 0 */ { (uchar *) "not",             4, 4,  3, 0x0F,     St,      0, 0, ONE },
/* 1 */ { (uchar *) "save",            4, 4,  4, 0x05,     St,      0, 0,ZERO },
/* 2 */ { (uchar *) "restore",         4, 4,  5, 0x06,     St,      0, 0,ZERO },
/* 3 */ { (uchar *) "not",             5, 0, -1, 0x38,      0,      0, 0, VAR },
/* 4 */ { (uchar *) "save",            5, 0, -1, 0x00,     St,      0, 0, EXT },
/* 5 */ { (uchar *) "restore",         5, 0, -1, 0x01,     St,      0, 0, EXT },
/* 6 */ { (uchar *) "pull",            6, 6, -1, 0x29,     St,      0, 0, VAR }
};

static opcode invalid_opcode =
        { (uchar *) "invalid",         0, 0, -1, 0xff,      0,      0, 0, ZERO};

static opcode custom_opcode;

static opcode internal_number_to_opcode(int32 i)
{   opcode x;
    if (i == -1) return custom_opcode;
    x = opcodes_table[i];
    if (instruction_set_number < x.version1) return invalid_opcode;
    if (x.version2 == 0) return x;
    if (instruction_set_number <= x.version2) return x;
    i=x.extension;
    x = extension_table[i];
    if (instruction_set_number < x.version1) return invalid_opcode;
    if (x.version2 == 0) return x;
    if (instruction_set_number <= x.version2) return x;
    return extension_table[x.extension];
}

static void make_opcode_syntax(opcode opco)
{   char *p = "", *q = opcode_syntax_string;
    sprintf(q, "%s", opco.name);
    switch(opco.no)
    {   case ONE: p=" <operand>"; break;
        case TWO: p=" <operand1> <operand2>"; break;
        case EXT:
        case VAR: p=" <0 to 4 operands>"; break;
        case VAR_LONG: p=" <0 to 8 operands>"; break;
    }
    switch(opco.op_rules)
    {   case TEXT: sprintf(q+strlen(q), " <text>"); return;
        case LABEL: sprintf(q+strlen(q), " <label>"); return;
        case VARIAB:
            sprintf(q+strlen(q), " <variable>");
        case CALL:
            if (opco.op_rules==CALL) sprintf(q+strlen(q), " <routine>");
            switch(opco.no)
            {   case ONE: p=""; break;
                case TWO: p=" <operand>"; break;
                case EXT:
                case VAR: p=" <1 to 4 operands>"; break;
                case VAR_LONG: p=" <1 to 8 operands>"; break;
            }
            break;
    }
    sprintf(q+strlen(q), "%s", p);
    if ((opco.flags & St) != 0) sprintf(q+strlen(q), " -> <result-variable>");
    if ((opco.flags & Br) != 0) sprintf(q+strlen(q), " ?[~]<label>");
}

/* ========================================================================= */
/*   The assembler itself does four things:                                  */
/*                                                                           */
/*       assembles instructions                                              */
/*       sets label N to the current code position                           */
/*       assembles routine headers                                           */
/*       assembles routine ends                                              */
/* ------------------------------------------------------------------------- */

static void write_operand(assembly_operand op)
{   int32 j;
    if (module_switch && (op.marker != 0))
    {   if ((op.marker != VARIABLE_MV) && (op.type == SHORT_CONSTANT_OT))
            op.type = LONG_CONSTANT_OT;
    }
    j=op.value;
    switch(op.type)
    {   case LONG_CONSTANT_OT:
            byteout(j/256, op.marker); byteout(j%256, 0); return;
        case SHORT_CONSTANT_OT:
            if (op.marker == 0)
            byteout(j, 0);
            else byteout(j, 0x80 + op.marker); return;
        case VARIABLE_OT:
            byteout(j, (module_switch)?(0x80 + op.marker):0); return;
    }
}

extern void assemble_instruction(assembly_instruction *AI)
{
    uchar *start_pc, *operands_pc;
    int32 offset, j, topbits, types_byte1, types_byte2;
    int operand_rules, min, max, no_operands_given, at_seq_point = FALSE;
    assembly_operand o1, o2;
    opcode opco;

    offset = zmachine_pc;

    no_instructions++;

    if (veneer_mode) sequence_point_follows = FALSE;
    if (sequence_point_follows)
    {   sequence_point_follows = FALSE; at_seq_point = TRUE;
        if (debugfile_switch)
        {   sequence_point_labels[next_sequence_point] = next_label;
            sequence_point_refs[next_sequence_point] = debug_line_ref;
            set_label_offset(next_label++, zmachine_pc);
        }
        next_sequence_point++;
    }

    opco = internal_number_to_opcode(AI->internal_number);
    if (opco.version1==0)
    {   error_named("Opcode unavailable in this Z-machine version",
            opcode_names.keywords[AI->internal_number]);
        return;
    }

    if (execution_never_reaches_here)
        warning("This statement can never be reached");

    operand_rules = opco.op_rules;
    execution_never_reaches_here = ((opco.flags & Rf) != 0);

    if (opco.flags2_set != 0) flags2_requirements[opco.flags2_set] = 1;

    no_operands_given = AI->operand_count;

    if ((opco.no == TWO) && ((no_operands_given==3)||(no_operands_given==4)))
        opco.no = VAR;

    /* 1. Write the opcode byte(s) */

    start_pc = zcode_holding_area + zcode_ha_size;

    switch(opco.no)
    {   case VAR_LONG: topbits=0xc0; min=0; max=8; break;
        case VAR:      topbits=0xc0; min=0; max=4; break;
        case ZERO:     topbits=0xb0; min=0; max=0; break;
        case ONE:      topbits=0x80; min=1; max=1; break;
        case TWO:      topbits=0x00; min=2; max=2; break;
        case EXT:      topbits=0x00; min=0; max=4;
                       byteout(0xbe, 0); opco.no=VAR; break;
        case EXT_LONG: topbits=0x00; min=0; max=8;
                       byteout(0xbe, 0); opco.no=VAR_LONG; break;
    }
    byteout(opco.code + topbits, 0);

    operands_pc = zcode_holding_area + zcode_ha_size;

    /* 2. Dispose of the special rules LABEL and TEXT */

    if (operand_rules==LABEL)
    {   j = (AI->operand[0]).value;
        byteout(j/256, LABEL_MV); byteout(j%256, 0);
        goto Instruction_Done;
    }

    if (operand_rules==TEXT)
    {   int32 i;
        j = subtract_pointers(
                (translate_text(zcode_holding_area + zcode_ha_size, AI->text)),
                (zcode_holding_area + zcode_ha_size));
        for (i=0; i<j; i++) zcode_markers[zcode_ha_size++] = 0;
        zmachine_pc += j;
        goto Instruction_Done;
    }

    /* 3. Sort out the operands */

    if ((no_operands_given < min) || (no_operands_given > max))
        goto OpcodeSyntaxError;

    switch(opco.no)
    {   case VAR:
        case VAR_LONG:
            byteout(0, 0);
            if (opco.no == VAR_LONG) byteout(0, 0);
            types_byte1=0xff; types_byte2=0xff;
            for (j=0; j<no_operands_given; j++)
            {   int multi, mask;
                switch(j)
                {   case 0: case 4: multi=0x40; mask=0xc0; break;
                    case 1: case 5: multi=0x10; mask=0x30; break;
                    case 2: case 6: multi=0x04; mask=0x0c; break;
                    case 3: case 7: multi=0x01; mask=0x03; break;
                }
                o1 = AI->operand[j];
                write_operand(o1);
                if (j<4)
                    types_byte1 = (types_byte1 & (~mask)) + o1.type*multi;
                else
                    types_byte2 = (types_byte2 & (~mask)) + o1.type*multi;
            }
            *operands_pc=types_byte1;
            if (opco.no == VAR_LONG) *(operands_pc+1)=types_byte2;
            break;

        case ONE:
            o1 = AI->operand[0];
            *start_pc=(*start_pc) + o1.type*0x10;
            write_operand(o1);
            break;

        case TWO:
            o1 = AI->operand[0];
            o2 = AI->operand[1];

            /* Transfer to VAR form if either operand is a long constant */

            if ((o1.type==LONG_CONSTANT_OT)||(o2.type==LONG_CONSTANT_OT))
            {   *start_pc=(*start_pc) + 0xc0;
                byteout(o1.type*0x40 + o2.type*0x10 + 0x0f, 0);
            }
            else
            {   if (o1.type==VARIABLE_OT) *start_pc=(*start_pc) + 0x40;
                if (o2.type==VARIABLE_OT) *start_pc=(*start_pc) + 0x20;
            }
            write_operand(o1);
            write_operand(o2);
            break;
    }

    /* 4. Assemble a Store destination, if needed */

    if ((AI->store_variable_number) != -1)
    {   o1.type = VARIABLE_OT;
        o1.value = AI->store_variable_number;
        variable_usage[o1.value] = TRUE;
        o1.marker = 0;

        /*  Note that variable numbers 249 to 255 (i.e. globals 233 to 239)
            are used as scratch workspace, so need no mapping between
            modules and story files: nor do local variables 0 to 15  */

        if ((o1.value >= 16) && (o1.value < 249)) o1.marker = VARIABLE_MV;
        write_operand(o1);
    }

    /* 5. Assemble a branch, if needed */

    if (AI->branch_label_number != -1)
    {   int32 addr, long_form;
        int branch_on_true = (AI->branch_flag)?1:0;

        switch (AI->branch_label_number)
        {   case -2: addr = 2; branch_on_true = 0; long_form = 0; break;
                                                 /* branch nowhere, carry on */
            case -3: addr = 0; long_form = 0; break;  /* rfalse on condition */
            case -4: addr = 1; long_form = 0; break;  /* rtrue on condition */
            default:
                long_form = 1; addr = AI->branch_label_number;
                break;
        }
        if (long_form==1)
        {   byteout(branch_on_true*0x80 + addr/256, BRANCH_MV);
            byteout(addr%256, 0);
        }
        else
            byteout(branch_on_true*0x80+ 0x40 + (addr&0x3f), 0);
    }

    Instruction_Done:

    if (asm_trace_level > 0)
    {   int i;
        printf("%5d  +%05lx %3s %-12s ", ErrorReport.line_number,
            ((long int) offset),
            (at_seq_point)?"<*>":"   ", opco.name);

        if ((AI->internal_number == print_zc)
            || (AI->internal_number == print_ret_zc))
        {   printf("\"");
            for (i=0;(AI->text)[i]!=0 && i<35; i++) printf("%c",(AI->text)[i]);
            if (i == 35) printf("...");
            printf("\"");
        }

        for (i=0; i<AI->operand_count; i++)
        {   if ((i==0) && (opco.op_rules == VARIAB))
            {   if ((AI->operand[0]).type == VARIABLE_OT)
                {   printf("["); print_operand(AI->operand[i]); }
                else
                    printf("%s", variable_name((AI->operand[0]).value));
            }
            else
            if ((i==0) && (opco.op_rules == LABEL))
            {   printf("L%d", AI->operand[0].value);
            }
            else print_operand(AI->operand[i]);
            printf(" ");
        }
        if (AI->store_variable_number != -1)
        {   assembly_operand AO;
            printf("-> ");
            AO.type = VARIABLE_OT; AO.value = AI->store_variable_number;
            print_operand(AO); printf(" ");
        }

        switch(AI->branch_label_number)
        {   case -4: printf("rtrue if %s", (AI->branch_flag)?"TRUE":"FALSE");
                break;
            case -3: printf("rfalse if %s", (AI->branch_flag)?"TRUE":"FALSE");
                break;
            case -2: printf("(no branch)"); break;
            case -1: break;
            default:
                printf("to L%d if %s", AI->branch_label_number,
                   (AI->branch_flag)?"TRUE":"FALSE"); break;
        }

        if (asm_trace_level>=2)
        {   for (j=0;start_pc<zcode_holding_area + zcode_ha_size;
                 j++, start_pc++)
            {   if (j%16==0) printf("\n                               ");
                printf("%02x ", *start_pc);
            }
        }
        printf("\n");
    }

    if (module_switch) flush_link_data();

    return;

    OpcodeSyntaxError:

    make_opcode_syntax(opco);
    error_named("Assembly mistake: syntax is", opcode_syntax_string);
}

extern void assemble_label_no(int n)
{
    if (asm_trace_level > 0)
        printf("%5d  +%05lx    .L%d\n", ErrorReport.line_number,
            ((long int) zmachine_pc), n);
    set_label_offset(n, zmachine_pc);
    execution_never_reaches_here = FALSE;
}

extern void define_symbol_label(int symbol)
{   label_symbols[svals[symbol]] = symbol;
}

extern int32 assemble_routine_header(int no_locals,
    int routine_asterisked, char *name, dbgl *line_ref)
{   int i, rv;

    execution_never_reaches_here = FALSE;

    routine_locals = no_locals;
    for (i=0; i<16; i++) variable_usage[i] = FALSE;

    if (veneer_mode) routine_starts_line = -1;
    else routine_starts_line = ErrorReport.line_number
             + 0x10000*ErrorReport.file_number;

    if (asm_trace_level > 0)
    {   printf("\n%5d  +%05lx  [ %s ", ErrorReport.line_number,
            ((long int) zmachine_pc), name);
        for (i=1; i<=no_locals; i++) printf("%s ", variable_name(i));
        printf("\n\n");
    }

    if (debugfile_switch)
    {   write_debug_byte(ROUTINE_DBR);
        write_debug_byte(no_routines/256);
        write_debug_byte(no_routines%256);
        write_dbgl(*line_ref);
        write_debug_address(zmachine_pc);
        write_debug_string(name);

        for (i=1; i<=no_locals; i++) write_debug_string(variable_name(i));

        write_debug_byte(0);

        routine_start_pc = zmachine_pc;
    }

    /*  Update the routine counter                                           */

    no_routines++;

    /*  Actually assemble the routine header into the code area; note        */
    /*  Inform doesn't support the setting of local variables to default     */
    /*  values other than 0 in V3 and V4.  (In V5+ the Z-Machine doesn't     */
    /*  provide the possibility in any case.)                                */

    byteout(no_locals, 0);

    /*  Not the packed address, but the scaled offset from code area start:  */

    rv = zmachine_pc/scale_factor;

    if (instruction_set_number<5)
        for (i=0; i<no_locals; i++) { byteout(0,0); byteout(0,0); }

    next_label = 0; next_sequence_point = 0; last_label = -1;

    /*  Compile code to print out text like "a=3, b=4, c=5" when the         */
    /*  function is called, if it's required.                                */

    if ((trace_fns_setting==2) || routine_asterisked
        || ((trace_fns_setting==1) && (is_systemfile()==FALSE)))
    {   char fnt[80]; assembly_operand PV; int ln;

        sprintf(fnt, "[ %s(", name);
        AI.text = fnt; assemble_0(print_zc);
        ln = next_label++;
        for (i=1; (i<=7)&&(i<=no_locals); i++)
        {   if (version_number >= 5)
            {   PV.type = SHORT_CONSTANT_OT;
                PV.value = i; PV.marker = 0;
                assemble_1_branch(check_arg_count_zc, PV, ln, FALSE);
            }
            sprintf(fnt, "%s%s = ", (i==1)?"":", ", variable_name(i));
            AI.text = fnt; assemble_0(print_zc);
            PV.type = VARIABLE_OT; PV.value = i; PV.marker = 0;
            assemble_1(print_num_zc, PV);
        }
        assemble_label_no(ln);
        sprintf(fnt, ") ]^"); AI.text = fnt;
        assemble_0(print_zc);
    }

    return rv;
}

static void transfer_routine(void);

void assemble_routine_end(int embedded_flag, dbgl *line_ref)
{   int32 i;

    /* No marker is made in the Z-machine's code area to indicate the        */
    /* end of a routine.  Instead, we simply assemble a return opcode if     */
    /* need be (it won't be if the last instruction was, say, a "quit").     */
    /* The return value is true (1) for normal routines, false (0) for       */
    /* embedded routines (e.g. the library uses this for "before"            */
    /* properties).                                                          */

    if (!execution_never_reaches_here)
    {   sequence_point_follows = FALSE;
        if (embedded_flag) assemble_0(rfalse_zc);
                      else assemble_0(rtrue_zc);
    }

    /* Dump the contents of the current routine into longer-term Z-code
       storage                                                               */

    transfer_routine();

    /* Tell the debugging file about the routine just ended.                 */

    if (debugfile_switch)
    {
        write_debug_byte(LINEREF_DBR);
        write_debug_byte((no_routines-1)/256);
        write_debug_byte((no_routines-1)%256);
        write_debug_byte(next_sequence_point/256);
        write_debug_byte(next_sequence_point%256);

        for (i=0; i<next_sequence_point; i++)
        {   int32 j = label_offsets[sequence_point_labels[i]] - routine_start_pc;

            write_dbgl(sequence_point_refs[i]);
            write_debug_byte(j / 256);
            write_debug_byte(j % 256);
        }

        write_debug_byte(ROUTINE_END_DBR);
        write_debug_byte((no_routines-1)/256);
        write_debug_byte((no_routines-1)%256);
        write_dbgl(*line_ref);
        write_debug_address(zmachine_pc);
    }

    /* Issue warnings about any local variables not used in the routine. */

    for (i=1; i<=routine_locals; i++)
        if (!(variable_usage[i]))
            dbnu_warning("Local variable", variable_name(i),
                routine_starts_line);

    for (i=0; i<next_label; i++)
    {   int j = label_symbols[i];
        if (j != -1)
        {   if (sflags[j] & CHANGE_SFLAG)
                error_named_at("Routine contains no such label as",
                    (char *) symbs[j], slines[j]);
            else
                if ((sflags[j] & USED_SFLAG) == 0)
                    dbnu_warning("Label", (char *) symbs[j], slines[j]);
            stypes[j] = CONSTANT_T;
            sflags[j] = UNKNOWN_SFLAG;
        }
    }
    no_sequence_points += next_sequence_point;
    next_label = 0; next_sequence_point = 0;
}

/* ------------------------------------------------------------------------- */
/*   Called when the holding area contains an entire routine of code:        */
/*   backpatches the labels, issues module markers, then dumps the routine   */
/*   into longer-term storage.                                               */
/*   Note that in the code received, all branches have long form, and their  */
/*   contents are not an offset but the label numbers they branch to.        */
/*   Similarly, LABEL operands (those of "jump" instructions) are label      */
/*   numbers.  So this routine must change the label numbers to offsets,     */
/*   slimming the code down as it does so to take advantage of short-form    */
/*   branch operands where possible.                                         */
/* ------------------------------------------------------------------------- */

static int32 adjusted_pc;

static void transfer_to_temp_file(uchar *c)
{   fputc(*c,Temp2_fp);
    adjusted_pc++;
}

static void transfer_to_zcode_area(uchar *c)
{   write_byte_to_memory_block(&zcode_area, adjusted_pc++, *c);
}

static void transfer_routine(void)
{   int32 i, j, pc, new_pc, label, long_form, offset_of_next, addr,
          branch_on_true;
    void (* transfer_byte)(uchar *);

    adjusted_pc = zmachine_pc - zcode_ha_size;

    if (asm_trace_level >= 3)
    {   printf("Backpatching routine at %05lx: initial size %d, %d labels\n",
             (long int) adjusted_pc, zcode_ha_size, next_label);
    }

    transfer_byte =
        (temporary_files_switch)?transfer_to_temp_file:transfer_to_zcode_area;

    /*  (1) Scan through for branches and make short/long decisions in each
            case.  Mark omitted bytes (2nd bytes in branches converted to
            short form) with DELETED_MV.                                     */

    for (i=0, pc=adjusted_pc; i<zcode_ha_size; i++, pc++)
    {   if (zcode_markers[i] == BRANCH_MV)
        {   if (asm_trace_level >= 4)
                printf("Branch detected at offset %04x\n", pc);
            j = (256*zcode_holding_area[i] + zcode_holding_area[i+1]) & 0x3ff;
            if (asm_trace_level >= 4)
                printf("To label %d, which is %d from here\n",
                    j, label_offsets[j]-pc);
            if ((label_offsets[j] >= pc+2) && (label_offsets[j] < pc+32))
            {   if (asm_trace_level >= 4) printf("Short form\n");
                zcode_markers[i+1] = DELETED_MV;
            }
        }
    }

    /*  (2) Calculate the new positions of the labels.  Note that since the
            long/short decision was taken on the basis of the old labels,
            and since the new labels are slightly closer together because
            of branch bytes deleted, there may be a few further branch
            optimisations which are possible but which have been missed
            (if two labels move inside the "short" range as a result of
            a previous optimisation).  However, this is acceptably uncommon. */

    if (next_label > 0)
    {   if (asm_trace_level >= 4)
        {   printf("Opening label: %d\n", first_label);
            for (i=0;i<next_label;i++)
                printf("Label %d offset %04x next -> %d previous -> %d\n",
                    i, label_offsets[i], label_next[i], label_prev[i]);
        }

        for (i=0, pc=adjusted_pc, new_pc=adjusted_pc, label = first_label;
            i<zcode_ha_size; i++, pc++)
        {   while ((label != -1) && (label_offsets[label] == pc))
            {   if (asm_trace_level >= 4)
                    printf("Position of L%d corrected from %04x to %04x\n",
                        label, label_offsets[label], new_pc);
                label_offsets[label] = new_pc;
                label = label_next[label];
            }
           if (zcode_markers[i] != DELETED_MV) new_pc++;
        }
    }

    /*  (3) As we are transferring, replace the label numbers in branch
            operands with offsets to those labels.  Also issue markers, now
            that we know where they occur in the final Z-code area.          */

    for (i=0, new_pc=adjusted_pc; i<zcode_ha_size; i++)
    {   switch(zcode_markers[i])
        { case BRANCH_MV:
            long_form = 1; if (zcode_markers[i+1] == DELETED_MV) long_form = 0;

            j = (256*zcode_holding_area[i] + zcode_holding_area[i+1]) & 0x3ff;
            branch_on_true = ((zcode_holding_area[i]) & 0x80);
            offset_of_next = new_pc + long_form + 1;

            addr = label_offsets[j] - offset_of_next + 2;
            if (addr<0) addr+=(int32) 0x10000L;

            addr=addr&0x3fff;
            if (long_form==1)
            {   zcode_holding_area[i] = branch_on_true + addr/256;
                zcode_holding_area[i+1] = addr%256;
            }
            else
            {   if (addr >= 32)
                {   error("*** Label out of range for branch ***");
                    printf("Addr is %04x\n", addr);
                }
                zcode_holding_area[i] = branch_on_true + 0x40 + (addr&0x3f);
            }
            transfer_byte(zcode_holding_area + i); new_pc++;
            break;

          case LABEL_MV:
            j = 256*zcode_holding_area[i] + zcode_holding_area[i+1];
            addr = label_offsets[j] - new_pc;
            if (addr<0) addr += (int32) 0x10000L;
            zcode_holding_area[i] = addr/256;
            zcode_holding_area[i+1] = addr%256;
            transfer_byte(zcode_holding_area + i); new_pc++;
            break;

          case DELETED_MV:
            break;

          default:
            switch(zcode_markers[i] & 0x7f)
            {   case NULL_MV: break;
                case VARIABLE_MV:
                case OBJECT_MV:
                case ACTION_MV:
                case IDENT_MV:
                    if (!module_switch) break;
                default:
                    if ((zcode_markers[i] & 0x7f) > LARGEST_BPATCH_MV)
                    {   error("*** Illegal code backpatch value ***");
                        printf("Illegal value of %02x at PC = %04x\n",
                            zcode_markers[i] & 0x7f, new_pc);
                        break;
                    }

                    write_byte_to_memory_block(&zcode_backpatch_table,
                        zcode_backpatch_size++,
                        zcode_markers[i] + 32*(new_pc/65536));
                    write_byte_to_memory_block(&zcode_backpatch_table,
                        zcode_backpatch_size++, (new_pc/256)%256);
                    write_byte_to_memory_block(&zcode_backpatch_table,
                        zcode_backpatch_size++, new_pc%256);
                    break;
            }
            transfer_byte(zcode_holding_area + i); new_pc++;
            break;
        }
    }

    if (asm_trace_level >= 3)
    {   printf("After branch optimisation, routine length is %d bytes\n",
             new_pc - adjusted_pc);
    }

    /*  Insert null bytes if necessary to ensure the next routine address is */
    /*  expressible as a packed address                                      */

    {   uchar zero[1];
        zero[0] = 0;
        while ((adjusted_pc%scale_factor)!=0) transfer_byte(zero);
    }

    zmachine_pc = adjusted_pc;
    zcode_ha_size = 0;
}

/* ========================================================================= */
/*   Front ends for the instruction assembler: convenient shorthand forms    */
/*   used in various code generation routines all over Inform.               */
/* ------------------------------------------------------------------------- */

void assemble_0(int internal_number)
{   AI.internal_number = internal_number;
    AI.operand_count = 0;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_0_to(int internal_number, assembly_operand o)
{   AI.internal_number = internal_number;
    AI.operand_count = 0;
    AI.store_variable_number = o.value;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_0_branch(int internal_number, int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 0;
    AI.store_variable_number = -1;
    AI.branch_label_number = label;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

void assemble_1(int internal_number, assembly_operand o1)
{   AI.internal_number = internal_number;
    AI.operand_count = 1;
    AI.operand[0] = o1;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_1_to(int internal_number,
    assembly_operand o1, assembly_operand st)
{   AI.internal_number = internal_number;
    AI.operand_count = 1;
    AI.operand[0] = o1;
    AI.store_variable_number = st.value;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_1_branch(int internal_number,
    assembly_operand o1, int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 1;
    AI.operand[0] = o1;
    AI.branch_label_number = label;
    AI.store_variable_number = -1;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

void assemble_2(int internal_number,
    assembly_operand o1, assembly_operand o2)
{   AI.internal_number = internal_number;
    AI.operand_count = 2;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_3(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3)
{   AI.internal_number = internal_number;
    AI.operand_count = 3;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_3_to(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3,
    assembly_operand st)
{   AI.internal_number = internal_number;
    AI.operand_count = 3;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.store_variable_number = st.value;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_3_branch(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3,
    int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 3;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.store_variable_number = -1;
    AI.branch_label_number = label;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

void assemble_4(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3,
    assembly_operand o4)
{   AI.internal_number = internal_number;
    AI.operand_count = 4;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.operand[3] = o4;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_4_branch(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3,
    assembly_operand o4, int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 4;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.operand[3] = o4;
    AI.store_variable_number = -1;
    AI.branch_label_number = label;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

void assemble_4_to(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand o3,
    assembly_operand o4, assembly_operand st)
{   AI.internal_number = internal_number;
    AI.operand_count = 4;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.operand[2] = o3;
    AI.operand[3] = o4;
    AI.store_variable_number = st.value;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_2_to(int internal_number,
    assembly_operand o1, assembly_operand o2, assembly_operand st)
{   AI.internal_number = internal_number;
    AI.operand_count = 2;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.store_variable_number = st.value;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_2_branch(int internal_number,
    assembly_operand o1, assembly_operand o2, int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 2;
    AI.operand[0] = o1;
    AI.operand[1] = o2;
    AI.branch_label_number = label;
    AI.store_variable_number = -1;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

void assemble_objcode(int internal_number,
    assembly_operand o1, assembly_operand st, int label, int flag)
{   AI.internal_number = internal_number;
    AI.operand_count = 1;
    AI.operand[0] = o1;
    AI.branch_label_number = label;
    AI.store_variable_number = st.value;
    AI.branch_flag = flag;
    assemble_instruction(&AI);
}

extern void assemble_inc(assembly_operand o1)
{   int m = 0;
    if ((o1.value >= 16) && (o1.value<LOWEST_SYSTEM_VAR_NUMBER))
        m = VARIABLE_MV;
    AI.internal_number = inc_zc;
    AI.operand_count = 1;
    AI.operand[0].value = o1.value;
    AI.operand[0].type = SHORT_CONSTANT_OT;
    AI.operand[0].marker = m;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

extern void assemble_dec(assembly_operand o1)
{   int m = 0;
    if ((o1.value >= 16) && (o1.value<LOWEST_SYSTEM_VAR_NUMBER))
        m = VARIABLE_MV;
    AI.internal_number = dec_zc;
    AI.operand_count = 1;
    AI.operand[0].value = o1.value;
    AI.operand[0].type = SHORT_CONSTANT_OT;
    AI.operand[0].marker = m;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

extern void assemble_store(assembly_operand o1, assembly_operand o2)
{   int m = 0;
    if ((o1.value >= 16) && (o1.value<LOWEST_SYSTEM_VAR_NUMBER))
        m = VARIABLE_MV;

    if ((o2.type == VARIABLE_OT) && (o2.value == 0))
    {
        /*  Assemble "pull VAR" rather than "store VAR sp",
            saving 1 byte  */

        AI.internal_number = pull_zc;
        if (instruction_set_number == 6)
        {   AI.operand_count = 0;
            AI.store_variable_number = o1.value;
        }
        else
        {   AI.operand_count = 1;
            AI.operand[0].value = o1.value;
            AI.operand[0].type = SHORT_CONSTANT_OT;
            AI.operand[0].marker = m;
            AI.store_variable_number = -1;
        }
        AI.branch_label_number = -1;
        assemble_instruction(&AI);
        return;
    }

    AI.internal_number = store_zc;
    AI.operand_count = 2;
    AI.operand[0].value = o1.value;
    AI.operand[0].type = SHORT_CONSTANT_OT;
    AI.operand[0].marker = m;
    AI.operand[1] = o2;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    assemble_instruction(&AI);
}

void assemble_jump(int n)
{   assembly_operand AO;
    if (n==-4) assemble_0(rtrue_zc);
    else if (n==-3) assemble_0(rfalse_zc);
    else
    {   AO.type = LONG_CONSTANT_OT; AO.value = n; AO.marker = 0;
        assemble_1(jump_zc, AO);
    }
}

/* ========================================================================= */
/*   Parsing and then calling the assembler for @ (assembly language)        */
/*   statements                                                              */
/* ------------------------------------------------------------------------- */

static assembly_operand parse_operand(void)
{   assembly_operand AO;

    AO = parse_expression(ASSEMBLY_CONTEXT);
    if (AO.type == EXPRESSION_OT)
    {   ebf_error("variable or constant", "expression");
        AO.type = SHORT_CONSTANT_OT;
    }
    return(AO);
}

extern void parse_assembly(void)
{   int n, min, max, indirect_addressed, error_flag = FALSE;
    opcode O;

    AI.operand_count = 0;
    AI.store_variable_number = -1;
    AI.branch_label_number = -1;
    AI.text = NULL;

    opcode_names.enabled = TRUE;
    get_next_token();
    opcode_names.enabled = FALSE;

    if (token_type == DQ_TT)
    {   int i;
        AI.internal_number = -1;

        custom_opcode.name = (uchar *) token_text;
        custom_opcode.version1 = instruction_set_number;
        custom_opcode.version2 = instruction_set_number;
        custom_opcode.extension = -1;
        custom_opcode.flags = 0;
        custom_opcode.op_rules = 0;
        custom_opcode.flags2_set = 0;
        custom_opcode.no = ZERO;

        for (i=0; token_text[i]!=0; i++)
        {   if (token_text[i] == ':')
            {   token_text[i++] = 0;
                break;
            }
        }
        if (token_text[i] == 0)
            error("Opcode specification should have form \"VAR:102\"");

        n = -1;
        if (strcmp(token_text, "0OP")==0)      n=ZERO;
        if (strcmp(token_text, "1OP")==0)      n=ONE;
        if (strcmp(token_text, "2OP")==0)      n=TWO;
        if (strcmp(token_text, "VAR")==0)      n=VAR;
        if (strcmp(token_text, "EXT")==0)      n=EXT;
        if (strcmp(token_text, "VAR_LONG")==0) n=VAR_LONG;
        if (strcmp(token_text, "EXT_LONG")==0) n=EXT_LONG;

        if (i>0) token_text[i-1] = ':';

        if (n==-1)
        {   ebf_error("Expected 0OP, 1OP, 2OP, VAR, EXT, VAR_LONG or EXT_LONG",
                token_text);
            n = EXT;
        }
        custom_opcode.no = n;

        custom_opcode.code = atoi(token_text+i);
        while (isdigit(token_text[i])) i++;

        {   int max, min;
            min = 0;
            switch(n)
            {   case ZERO: case ONE: max = 16; break;
                case VAR: case VAR_LONG: min = 32; max = 64; break;
                case EXT: case EXT_LONG: max = 256; break;
                case TWO: max = 32; break;
            }
            if ((custom_opcode.code < min) || (custom_opcode.code >= max))
            {   char range[32];
                sprintf(range, "%d to %d", min, max-1);
            error_named("For this operand type, opcode number must be in range",
                    range);
                custom_opcode.code = min;
            }
        }

        while (token_text[i++] != 0)
        {   switch(token_text[i-1])
            {   case 'B': custom_opcode.flags |= Br; break;
                case 'S': custom_opcode.flags |= St; break;
                case 'T': custom_opcode.op_rules = TEXT; break;
                case 'I': custom_opcode.op_rules = VARIAB; break;
                case 'F': custom_opcode.flags2_set = atoi(token_text+i);
                          while (isdigit(token_text[i])) i++; break;
                default:
                    error("Unknown flag: options are B (branch), S (store), \
T (text), I (indirect addressing), F** (set this Flags 2 bit)");
                    break;
            }
        }
        O = custom_opcode;
    }
    else
    {   if (token_type != OPCODE_NAME_TT)
        {   ebf_error("an opcode name", token_text);
            panic_mode_error_recovery();
            return;
        }
        AI.internal_number = token_value;
        O = internal_number_to_opcode(AI.internal_number);
    }

    indirect_addressed = (O.op_rules == VARIAB);

    if (O.op_rules == TEXT)
    {   get_next_token();
        if (token_type != DQ_TT)
            ebf_error("literal text in double-quotes", token_text);
        AI.text = token_text;
        if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)) return;
        get_next_token();
        if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
        {   assemble_instruction(&AI);
            return;
        }
        ebf_error("semicolon ';' after print string", token_text);
        put_token_back();
        return;
    }

    return_sp_as_variable = TRUE;
    do
    {   get_next_token();

        if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)) break;

        if ((token_type == SEP_TT) && (token_value == ARROW_SEP))
        {   if (AI.store_variable_number != -1)
                error("Only one '->' store destination can be given");
            get_next_token();
            if ((token_type != SYMBOL_TT)
                && (token_type != LOCAL_VARIABLE_TT))
                ebf_error("variable name or 'sp'", token_text);
            n = 255;
            if (token_type == LOCAL_VARIABLE_TT) n = token_value;
            else
            {   if (strcmp(token_text, "sp") == 0) n = 0;
                else
                {   if (stypes[token_value] != GLOBAL_VARIABLE_T)
                        error_named(
                            "Store '->' destination not 'sp' or a variable:",
                            token_text);
                    else n = svals[token_value];
                }
            }
            AI.store_variable_number = n;
            continue;
        }

        if ((token_type == SEP_TT) &&
            ((token_value == BRANCH_SEP) || (token_value == NBRANCH_SEP)))
        {   if (AI.branch_label_number != -1)
                error("Only one '?' branch destination can be given");

            AI.branch_flag = (token_value == BRANCH_SEP);

            opcode_names.enabled = TRUE;
            get_next_token();
            opcode_names.enabled = FALSE;

            n = -2;
            if ((token_type == OPCODE_NAME_TT)
                && (token_value == rfalse_zc)) n = -3;
            else
            if ((token_type == OPCODE_NAME_TT)
                && (token_value == rtrue_zc)) n = -4;
            else
            {   if (token_type == SYMBOL_TT)
                {   put_token_back();
                    n = parse_label();
                }
                else
                    ebf_error("label name after '?' or '?~'", token_text);
            }
            AI.branch_label_number = n;
            continue;
        }

        if (AI.operand_count == 8)
        {   error("No assembly instruction may have more than 8 operands");
            panic_mode_error_recovery(); break;
        }

        if ((token_type == SEP_TT) && (token_value == OPEN_SQUARE_SEP))
        {   if (!indirect_addressed)
                error("This opcode does not use indirect addressing");
            if (AI.operand_count > 0)
            error("Indirect addressing can only be used on the first operand");
            AI.operand[AI.operand_count++] = parse_operand();
            get_next_token();
            if (!((token_type == SEP_TT) && (token_value == CLOSE_SQUARE_SEP)))
            {   ebf_error("']'", token_text);
                put_token_back();
            }
        }
        else
        {   put_token_back();
            AI.operand[AI.operand_count++] = parse_operand();
            if ((indirect_addressed)
                && (AI.operand[AI.operand_count-1].type == VARIABLE_OT))
            {   AI.operand[AI.operand_count-1].type = SHORT_CONSTANT_OT;
                AI.operand[AI.operand_count-1].marker = VARIABLE_MV;
            }
        }

    } while (TRUE);

    return_sp_as_variable = FALSE;


    if (O.version1 == 0)
    {   error_named("Opcode unavailable in this Z-machine version:",
            opcode_names.keywords[AI.internal_number]);
        return;
    }

    if (((O.flags) & Br) != 0)
    {   if (AI.branch_label_number == -1)
        {   error_flag = TRUE;
            AI.branch_label_number = -2;
        }
    }
    else
    {   if (AI.branch_label_number != -1)
        {   error_flag = TRUE;
            AI.branch_label_number = -1;
        }
    }
    if (((O.flags) & St) != 0)
    {   if (AI.store_variable_number == -1)
        {   if (AI.operand_count == 0)
            {   error_flag = TRUE;
                AI.store_variable_number = 255;
            }
            else
            {   AI.store_variable_number
                    = AI.operand[--AI.operand_count].value;
                if (AI.operand[AI.operand_count].type != VARIABLE_OT)
            error("Store destination (the last operand) is not a variable");
            }
        }
    }
    else
    {   if (AI.store_variable_number != -1)
        {   error_flag = TRUE;
            AI.store_variable_number = -1;
        }
    }

    switch(O.no)
    {   case TWO:      min = 2; max = 2;
                       /* Exception for the V6 set_colour, which can take
                          a third argument, thus forcing it into VAR form: */
                       if ((version_number == 6) && (O.code == 0x1b)) max = 3;
                       break;
        case VAR:      min = 0; max = 4; break;
        case VAR_LONG: min = 0; max = 8; break;
        case ONE:      min = 1; max = 1; break;
        case ZERO:     min = 0; max = 0; break;
        case EXT:      min = 0; max = 4; break;
        case EXT_LONG: min = 0; max = 8; break;
    }

    if ((AI.operand_count >= min) && (AI.operand_count <= max))
        assemble_instruction(&AI);
    else error_flag = TRUE;

    if (error_flag)
    {   make_opcode_syntax(O);
        error_named("Assembly mistake: syntax is",
            opcode_syntax_string);
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void asm_begin_pass(void)
{   no_instructions = 0;
    zmachine_pc = 0;
    no_sequence_points = 0;
    next_label = 0;
    next_sequence_point = 0;
    zcode_ha_size = 0;
}

extern void init_asm_vars(void)
{   int i;

    for (i=0;i<16;i++) flags2_requirements[i]=0;

    sequence_point_follows = TRUE;
    label_moved_error_already_given = FALSE;

    initialise_memory_block(&zcode_area);
}

extern void asm_allocate_arrays(void)
{   if ((debugfile_switch) && (MAX_LABELS < 2000)) MAX_LABELS = 2000;

    label_offsets = my_calloc(sizeof(int32), MAX_LABELS, "label offsets");
    label_symbols = my_calloc(sizeof(int32), MAX_LABELS, "label symbols");
    label_next = my_calloc(sizeof(int), MAX_LABELS, "label dll 1");
    label_prev = my_calloc(sizeof(int), MAX_LABELS, "label dll 1");
    sequence_point_labels
        = my_calloc(sizeof(int), MAX_LABELS, "sequence point labels");
    sequence_point_refs
        = my_calloc(sizeof(dbgl), MAX_LABELS, "sequence point refs");

    zcode_holding_area = my_malloc(MAX_ZCODE_SIZE,"compiled routine code area");
    zcode_markers = my_malloc(MAX_ZCODE_SIZE,"compiled routine code area");
}

extern void asm_free_arrays(void)
{
    my_free(&label_offsets, "label offsets");
    my_free(&label_symbols, "label symbols");
    my_free(&label_next, "label dll 1");
    my_free(&label_prev, "label dll 2");
    my_free(&sequence_point_labels, "sequence point labels");
    my_free(&sequence_point_refs, "sequence point refs");

    my_free(&zcode_holding_area, "compiled routine code area");
    my_free(&zcode_markers, "compiled routine code markers");
    deallocate_memory_block(&zcode_area);
}

/* ========================================================================= */
