/* ------------------------------------------------------------------------- */
/*   "bpatch" : Keeps track of, and finally acts on, backpatch markers,      */
/*              correcting symbol values not known at compilation time       */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

memory_block zcode_backpatch_table, zmachine_backpatch_table;
int32 zcode_backpatch_size, zmachine_backpatch_size;

/* ------------------------------------------------------------------------- */
/*   The mending operation                                                   */
/* ------------------------------------------------------------------------- */

int backpatch_marker, backpatch_size, backpatch_error_flag;

extern int32 backpatch_value(int32 value)
{   /*  Corrects the quantity "value" according to backpatch_marker  */

    if (asm_trace_level >= 4)
        printf("BP %s applied to %04x giving ",
            describe_mv(backpatch_marker), value);

    switch(backpatch_marker)
    {   case STRING_MV:
            value += strings_offset/scale_factor; break;
        case ARRAY_MV:
            value += variables_offset; break;
        case IROUTINE_MV:
            value += code_offset/scale_factor; break;
        case VROUTINE_MV:
            if ((value<0) || (value>=VENEER_ROUTINES))
            {   if (no_link_errors > 0) break;
                if (compiler_error
                    ("Backpatch veneer routine number out of range"))
                {   printf("Illegal BP veneer routine number: %d\n", value);
                    backpatch_error_flag = TRUE;
                }
                value = 0;
                break;
            }
            value = veneer_routine_address[value] + code_offset/scale_factor;
            break;
        case NO_OBJS_MV:
            value = no_objects; break;
        case INCON_MV:
            if ((value<0) || (value>=12))
            {   if (no_link_errors > 0) break;
                if (compiler_error
                    ("Backpatch system constant number out of range"))
                {   printf("Illegal BP system constant number: %d\n", value);
                    backpatch_error_flag = TRUE;
                }
                value = 0;
                break;
            }
            value = value_of_system_constant(value); break;
        case DWORD_MV:
            value = dictionary_offset + 7 +
                    final_dict_order[value]*((version_number==3)?7:9);
            break;
        case ACTION_MV:
            break;
        case INHERIT_MV:
            value = 256*zmachine_paged_memory[value + prop_values_offset]
                    + zmachine_paged_memory[value + prop_values_offset + 1];
            break;
        case INHERIT_INDIV_MV:
            value = 256*zmachine_paged_memory[value
                        + individuals_offset]
                    + zmachine_paged_memory[value
                        + individuals_offset + 1];
            break;
        case INDIVPT_MV:
            value += individuals_offset;
            break;
        case MAIN_MV:
            value = symbol_index("Main", -1);
            if (stypes[value] != ROUTINE_T)
                error("No 'Main' routine has been defined");
            sflags[value] |= USED_SFLAG;
            value = svals[value] + code_offset/scale_factor;
            break;
        case SYMBOL_MV:
            if ((value<0) || (value>=no_symbols))
            {   if (no_link_errors > 0) break;
                if (compiler_error("Backpatch symbol number out of range"))
                {   printf("Illegal BP symbol number: %d\n", value);
                    backpatch_error_flag = TRUE;
                }
                value = 0;
                break;
            }
            if (sflags[value] & UNKNOWN_SFLAG)
            {   if (!(sflags[value] & UERROR_SFLAG))
                {   sflags[value] |= UERROR_SFLAG;
                    error_named_at("No such constant as",
                        (char *) symbs[value], slines[value]);
                }
            }
            else
            if (sflags[value] & CHANGE_SFLAG)
            {   sflags[value] &= (~(CHANGE_SFLAG));
                backpatch_marker = (svals[value]/0x10000);
                if ((backpatch_marker < 0)
                    || (backpatch_marker > LARGEST_BPATCH_MV))
                {   
                    if (no_link_errors == 0)
                    {   compiler_error_named(
                        "Illegal backpatch marker attached to symbol",
                        (char *) symbs[value]);
                        backpatch_error_flag = TRUE;
                    }
                }
                else
                    svals[value] = backpatch_value((svals[value]) % 0x10000);
            }

            sflags[value] |= USED_SFLAG;
            {   int t = stypes[value];
                value = svals[value];
                switch(t)
                {   case ROUTINE_T: value += code_offset/scale_factor; break;
                    case ARRAY_T: value += variables_offset; break;
                }
            }
            break;
        default:
            if (no_link_errors > 0) break;
            if (compiler_error("Illegal backpatch marker"))
            {   printf("Illegal backpatch marker %d value %04x\n",
                    backpatch_marker, value);
                backpatch_error_flag = TRUE;
            }
            break;
    }

    if (asm_trace_level >= 4) printf(" %04x\n", value);

    return(value);
}

extern void backpatch_zmachine(int mv, int zmachine_area, int32 offset)
{   if (module_switch)
    {   if (zmachine_area == PROP_DEFAULTS_ZA) return;
    }
    else
    {   if (mv == OBJECT_MV) return;
        if (mv == IDENT_MV) return;
        if (mv == ACTION_MV) return;
    }

/*    printf("MV %d ZA %d Off %04x\n", mv, zmachine_area, offset);  */

    write_byte_to_memory_block(&zmachine_backpatch_table,
        zmachine_backpatch_size++, mv);
    write_byte_to_memory_block(&zmachine_backpatch_table,
        zmachine_backpatch_size++, zmachine_area);
    write_byte_to_memory_block(&zmachine_backpatch_table,
        zmachine_backpatch_size++, offset/256);
    write_byte_to_memory_block(&zmachine_backpatch_table,
        zmachine_backpatch_size++, offset%256);
}

extern void backpatch_zmachine_image(void)
{   int bm = 0, zmachine_area; int32 offset, value, addr;
    backpatch_error_flag = FALSE;
    while (bm < zmachine_backpatch_size)
    {   backpatch_marker
            = read_byte_from_memory_block(&zmachine_backpatch_table, bm);
        zmachine_area
            = read_byte_from_memory_block(&zmachine_backpatch_table, bm+1);
        offset
          = 256*read_byte_from_memory_block(&zmachine_backpatch_table,bm+2)
            + read_byte_from_memory_block(&zmachine_backpatch_table, bm+3);
        bm += 4;

        switch(zmachine_area)
        {   case PROP_DEFAULTS_ZA:   addr = prop_defaults_offset; break;
            case PROP_ZA:            addr = prop_values_offset; break;
            case INDIVIDUAL_PROP_ZA: addr = individuals_offset; break;
            case DYNAMIC_ARRAY_ZA:   addr = variables_offset; break;
            default:
                if (no_link_errors == 0)
                    if (compiler_error("Illegal area to backpatch"))
                        backpatch_error_flag = TRUE;
        }
        addr += offset;

        value = 256*zmachine_paged_memory[addr]
                + zmachine_paged_memory[addr+1];
        value = backpatch_value(value);
        zmachine_paged_memory[addr] = value/256;
        zmachine_paged_memory[addr+1] = value%256;

        if (backpatch_error_flag)
        {   backpatch_error_flag = FALSE;
            if (no_link_errors == 0)
                printf("*** MV %d ZA %d Off %04x ***\n",
                    backpatch_marker, zmachine_area, offset);
        }
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_bpatch_vars(void)
{   initialise_memory_block(&zcode_backpatch_table);
    initialise_memory_block(&zmachine_backpatch_table);
}

extern void bpatch_begin_pass(void)
{   zcode_backpatch_size = 0;
    zmachine_backpatch_size = 0;
}

extern void bpatch_allocate_arrays(void)
{
}

extern void bpatch_free_arrays(void)
{   deallocate_memory_block(&zcode_backpatch_table);
    deallocate_memory_block(&zmachine_backpatch_table);
}

/* ========================================================================= */
