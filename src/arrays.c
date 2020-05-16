/* ------------------------------------------------------------------------- */
/*   "arrays" :  Parses array declarations and constructs arrays from them;  */
/*               likewise global variables, which are in some ways a         */
/*               simpler form of the same thing.                             */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

/* ------------------------------------------------------------------------- */
/*   Arrays defined below:                                                   */
/*                                                                           */
/*    int    dynamic_array_area[]         Initial values for the bytes of    */
/*                                        the dynamic array area             */
/*    int32  global_initial_value[n]      The initialised value of the nth   */
/*                                        global variable (counting 0 - 239) */
/*                                                                           */
/*   The "dynamic array area" is the Z-machine area holding the current      */
/*   values of the global variables (in 240x2 = 480 bytes) followed by any   */
/*   (dynamic) arrays which may be defined.  Owing to a poor choice of name  */
/*   some years ago, this is also called the "static data area", which is    */
/*   why the memory setting for its maximum extent is "MAX_STATIC_DATA".     */
/* ------------------------------------------------------------------------- */
int     *dynamic_array_area;           /* See above                          */
int32   *global_initial_value;

int no_globals,                        /* Number of global variables used
                                          by the programmer (Inform itself
                                          uses the top seven -- but these do
                                          not count)                         */
    dynamic_array_area_size;           /* Size in bytes                      */

static int array_entry_size,           /* 1 for byte array, 2 for word array */
           array_base;                 /* Offset in dynamic array area of the
                                          array being constructed.  During the
                                          same time, dynamic_array_area_size
                                          is the offset of the initial entry
                                          in the array: so for "table" and
                                          "string" arrays, these numbers are
                                          different (by 2 and 1 bytes resp)  */

extern void finish_array(int32 i)
{
    /*  Write the array size into the 0th byte/word of the array, if it's
        a "table" or "string" array                                          */

    if (array_base!=dynamic_array_area_size)
    {   if (array_entry_size==2)
        {   dynamic_array_area[array_base]   = i/256;
            dynamic_array_area[array_base+1] = i%256;
        }
        else
        {   if (i>=256)
                error("A 'string' array can have at most 256 entries");
            dynamic_array_area[array_base] = i;
        }
    }

    /*  Move on the dynamic array size so that it now points to the next
        available free space                                                 */

    dynamic_array_area_size += i*array_entry_size;
}

extern void array_entry(int32 i, assembly_operand VAL)
{
    /*  Array entry i (initial entry has i=0) is set to Z-machine value j    */

    if (dynamic_array_area_size+i*array_entry_size >= MAX_STATIC_DATA)
        memoryerror("MAX_STATIC_DATA", MAX_STATIC_DATA);

    if (array_entry_size==1)
    {   dynamic_array_area[dynamic_array_area_size+i] = (VAL.value)%256;

        if (VAL.marker != 0)
           error("Entries in byte arrays and strings must be known constants");

        /*  If the entry is too large for a byte array, issue a warning
            and truncate the value                                           */
        else
        if (VAL.value >= 256)
            warning("Entry in '->' or 'string' array not in range 0 to 255");
    }
    else
    {   dynamic_array_area[dynamic_array_area_size + 2*i]   = (VAL.value)/256;
        dynamic_array_area[dynamic_array_area_size + 2*i+1] = (VAL.value)%256;
        if (VAL.marker != 0)
            backpatch_zmachine(VAL.marker, DYNAMIC_ARRAY_ZA,
                dynamic_array_area_size + 2*i);
    }
}

/* ------------------------------------------------------------------------- */
/*   Global and Array directives.                                            */
/*                                                                           */
/*      Global <variablename> |                                              */
/*                            | = <value>                                    */
/*                            | <array specification>                        */
/*                                                                           */
/*      Array <arrayname> <array specification>                              */
/*                                                                           */
/*   where an array specification is:                                        */
/*                                                                           */
/*      | ->       |  <number-of-entries>                                    */
/*      | -->      |  <entry-1> ... <entry-n>                                */
/*      | string   |  [ <entry-1> [,] [;] <entry-2> ... <entry-n> ];         */
/*      | table                                                              */
/*                                                                           */
/* ------------------------------------------------------------------------- */

extern void set_variable_value(int i, int32 v)
{   global_initial_value[i]=v;
}

/*  Inform supports four array types...                                      */

#define BYTE_ARRAY      0
#define WORD_ARRAY      1
#define STRING_ARRAY    2
#define TABLE_ARRAY     3

/*  ...each of which can be initialised in four ways:                        */

#define UNSPECIFIED_AI  -1
#define NULLS_AI        0
#define DATA_AI         1
#define ASCII_AI        2
#define BRACKET_AI      3

extern void make_global(int array_flag, int name_only)
{
    /*  array_flag is TRUE for an Array directive, FALSE for a Global;
        name_only is only TRUE for parsing an imported variable name, so
        array_flag is always FALSE in that case.                             */

    int32 i;
    /*  char *varname;  */
    int array_type, data_type;
    assembly_operand AO;

    directive_keywords.enabled = FALSE;
    get_next_token();
    i = token_value;
    /*  varname = token_text;  */

    if ((token_type==SYMBOL_TT) && (stypes[i]==GLOBAL_VARIABLE_T)
        && (svals[i] >= LOWEST_SYSTEM_VAR_NUMBER))
        goto RedefinitionOfSystemVar;

    if ((token_type != SYMBOL_TT) || (!(sflags[i] & UNKNOWN_SFLAG)))
    {   if (array_flag)
            ebf_error("new array name", token_text);
        else ebf_error("new global variable name", token_text);
        panic_mode_error_recovery(); return;
    }

    if ((!array_flag) && (sflags[i] & USED_SFLAG))
        error_named("Variable must be defined before use:", token_text);

    if (array_flag)
    {   assign_symbol(i, dynamic_array_area_size, ARRAY_T);
    }
    else
    {   if (no_globals==233)
        {   error("All 233 global variables already declared");
            panic_mode_error_recovery();
            return;
        }

        variable_tokens[16+no_globals] = i;
        assign_symbol(i, 16+no_globals, GLOBAL_VARIABLE_T);
        variable_tokens[svals[i]] = i;

        if (debugfile_switch)
        {   write_debug_byte(GLOBAL_DBR); write_debug_byte(no_globals);
            write_debug_string(token_text);
        }
        if (name_only) import_symbol(i);
        else global_initial_value[no_globals++]=0;
    }

    directive_keywords.enabled = TRUE;

    RedefinitionOfSystemVar:

    if (name_only) return;

    get_next_token();

    if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
    {   if (array_flag) ebf_error("array definition", token_text);
        put_token_back();
        return;
    }

    if (!array_flag)
    {
        if ((token_type == SEP_TT) && (token_value == SETEQUALS_SEP))
        {   AO = parse_expression(CONSTANT_CONTEXT);
            if (AO.marker != 0)
                backpatch_zmachine(AO.marker, DYNAMIC_ARRAY_ZA,
                    2*(no_globals-1));
            global_initial_value[no_globals-1] = AO.value;
            return;
        }

        obsolete_warning("more modern to use 'Array', not 'Global'");

        backpatch_zmachine(ARRAY_MV, DYNAMIC_ARRAY_ZA, 2*(no_globals-1));
        global_initial_value[no_globals-1]
            = dynamic_array_area_size+variables_offset;
    }

    array_type = BYTE_ARRAY; data_type = UNSPECIFIED_AI;

         if ((!array_flag) &&
             ((token_type==DIR_KEYWORD_TT)&&(token_value==DATA_DK)))
                 data_type=NULLS_AI;
    else if ((!array_flag) &&
             ((token_type==DIR_KEYWORD_TT)&&(token_value==INITIAL_DK)))
                 data_type=DATA_AI;
    else if ((!array_flag) &&
             ((token_type==DIR_KEYWORD_TT)&&(token_value==INITSTR_DK)))
                 data_type=ASCII_AI;

    else if ((token_type==SEP_TT)&&(token_value==ARROW_SEP))
             array_type = BYTE_ARRAY;
    else if ((token_type==SEP_TT)&&(token_value==DARROW_SEP))
             array_type = WORD_ARRAY;
    else if ((token_type==DIR_KEYWORD_TT)&&(token_value==STRING_DK))
             array_type = STRING_ARRAY;
    else if ((token_type==DIR_KEYWORD_TT)&&(token_value==TABLE_DK))
             array_type = TABLE_ARRAY;
    else {   if (array_flag)
               ebf_error("'->', '-->', 'string' or 'table'", token_text);
             else
               ebf_error("'=', '->', '-->', 'string' or 'table'", token_text);
             panic_mode_error_recovery();
             return;
         }

    array_entry_size=1;
    if ((array_type==WORD_ARRAY) || (array_type==TABLE_ARRAY))
        array_entry_size=2;

    get_next_token();
    if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
    {   error("No array size or initial values given");
        put_token_back();
        return;
    }

    switch(data_type)
    {   case UNSPECIFIED_AI:
            if ((token_type == SEP_TT) && (token_value == OPEN_SQUARE_SEP))
                data_type = BRACKET_AI;
            else
            {   data_type = NULLS_AI;
                if (token_type == DQ_TT) data_type = ASCII_AI;
                get_next_token();
                if (!((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
                    data_type = DATA_AI;
                put_token_back();
                put_token_back();
            }
            break;
        case NULLS_AI: obsolete_warning("use '->' instead of 'data'"); break;
        case DATA_AI:  obsolete_warning("use '->' instead of 'initial'"); break;
        case ASCII_AI: obsolete_warning("use '->' instead of 'initstr'"); break;
    }

    array_base = dynamic_array_area_size;

    /*  Leave room to write the array size in later, if string/table array   */

    if ((array_type==STRING_ARRAY) || (array_type==TABLE_ARRAY))
        dynamic_array_area_size += array_entry_size;

    switch(data_type)
    {
        case NULLS_AI:

            AO = parse_expression(CONSTANT_CONTEXT);

            CalculatedArraySize:

            if (module_switch && (AO.marker != 0))
            {   error("Array sizes must be known now, not externally defined");
                break;
            }

            if ((AO.value <= 0) || (AO.value >= 32768))
            {   error("An array must have between 1 and 32767 entries");
                AO.value = 1;
            }

            {   assembly_operand zero;
                zero.value = 0; zero.type = SHORT_CONSTANT_OT;
                zero.marker = 0;

                for (i=0; i<AO.value; i++) array_entry(i, zero);
            }
            break;

        case DATA_AI:

            /*  In this case the array is initialised to the sequence of
                constant values supplied on the same line                    */

            i=0;
            do
            {   get_next_token();
                if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                    break;

                if ((token_type == SEP_TT)
                    && ((token_value == OPEN_SQUARE_SEP)
                        || (token_value == CLOSE_SQUARE_SEP)))
                {   error(
          "Missing ';' to end the initial array values before \"[\" or \"]\"");
                    return;
                }
                put_token_back();

                AO = parse_expression(ARRAY_CONTEXT);

                if (i == 0)
                {   get_next_token();
                    put_token_back();
                    if ((token_type == SEP_TT)
                        && (token_value == SEMICOLON_SEP))
                    {   data_type = NULLS_AI;
                        goto CalculatedArraySize;
                    }
                }

                array_entry(i, AO);
                i++;
            } while (TRUE);
            put_token_back();
            break;

        case ASCII_AI:

            /*  In this case the array is initialised to the ASCII values of
                the characters of a given "quoted string"                    */

            get_next_token();
            if (token_type != DQ_TT)
            {   ebf_error("literal text in double-quotes", token_text);
                token_text = "error";
            }

            {   assembly_operand chars;
                chars.type = SHORT_CONSTANT_OT; chars.marker = 0;
                for (i=0; token_text[i]!=0; i++)
                {   chars.value = token_text[i];
                    array_entry(i, chars);
                }
            }
            break;

        case BRACKET_AI:

            /*  In this case the array is initialised to the sequence of
                constant values given over a whole range of compiler-lines,
                between square brackets [ and ]                              */

            i = 0;
            while (TRUE)
            {   get_next_token();
                if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                    continue;
                if ((token_type == SEP_TT) && (token_value == CLOSE_SQUARE_SEP))
                    break;
                if ((token_type == SEP_TT) && (token_value == OPEN_SQUARE_SEP))
                {   /*  Minimal error recovery: we assume that a ] has
                        been missed, and the programmer is now starting
                        a new routine                                        */

                    ebf_error("']'", token_text);
                    put_token_back(); break;
                }
                put_token_back();
                array_entry(i, parse_expression(ARRAY_CONTEXT));
                i++;
            }
    }

    finish_array(i);
}

extern int32 begin_table_array(void)
{
    /*  The "box" statement needs to be able to construct (static) table
        arrays of strings like this                                          */

    array_base = dynamic_array_area_size;
    array_entry_size = 2;

    /*  Leave room to write the array size in later                          */

    dynamic_array_area_size += array_entry_size;

    return array_base;
}

extern int32 begin_word_array(void)
{
    /*  The "random(a, b, ...)" function needs to be able to construct
        (static) word arrays like this                                       */

    array_base = dynamic_array_area_size;
    array_entry_size = 2;

    return array_base;
}

#ifdef VARYING_STRINGS
extern void patch_word_array(int array, int offset, int value)
{
 dynamic_array_area[array+offset*2  ] = value/256;
 dynamic_array_area[array+offset*2+1] = value%256;
}
#endif

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_arrays_vars(void)
{   dynamic_array_area = NULL;
    global_initial_value = NULL;
}

extern void arrays_begin_pass(void)
{   no_globals=0; dynamic_array_area_size=0x1e0;
}

extern void arrays_allocate_arrays(void)
{   dynamic_array_area = my_calloc(sizeof(int), MAX_STATIC_DATA, "static data");
    global_initial_value = my_calloc(sizeof(int32), 240, "global values");
}

extern void arrays_free_arrays(void)
{   my_free(&dynamic_array_area, "static data");
    my_free(&global_initial_value, "global values");
}

/* ========================================================================= */
