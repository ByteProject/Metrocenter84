/* ------------------------------------------------------------------------- */
/*   "tables" :  Constructs the story file or module (the output) up to the  */
/*               end of dynamic memory, gluing together all the required     */
/*               tables.                                                     */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

uchar *zmachine_paged_memory;          /* Where we shall store the story file
                                          constructed (contains all of paged
                                          memory, i.e. all but code and the
                                          static strings: allocated only when
                                          we know how large it needs to be,
                                          at the end of the compilation pass */

/* ------------------------------------------------------------------------- */
/*   Offsets of various areas in the Z-machine: these are set to nominal     */
/*   values before the compilation pass, and to their calculated final       */
/*   values only when construct_storyfile() happens.  These are then used to */
/*   backpatch the incorrect values now existing in the Z-machine which      */
/*   used these nominal values.                                              */
/*   Most of the nominal values are 0x800 because this is guaranteed to      */
/*   be assembled as a long constant if it's needed in code, since the       */
/*   largest possible value of scale_factor is 8 and 0x800/8 = 256.          */
/* ------------------------------------------------------------------------- */

int32 code_offset,
      actions_offset,
      preactions_offset,
      dictionary_offset,
      adjectives_offset,
      variables_offset,
      strings_offset,
      class_numbers_offset,
      individuals_offset,
      identifier_names_offset,
      prop_defaults_offset,
      prop_values_offset;

int32 Out_Size, Write_Code_At, Write_Strings_At;

/* ------------------------------------------------------------------------- */
/*   Story file header settings.   (Written to in "directs.c" and "asm.c".)  */
/* ------------------------------------------------------------------------- */

int release_number,                    /* Release number game is to have     */
    statusline_flag,                   /* Either TIME_STYLE or SCORE_STYLE   */

    serial_code_given_in_program;      /* If TRUE, a Serial directive has    */
char serial_code_buffer[7];            /* specified this 6-digit serial code */
                                       /* (overriding the usual date-stamp)  */
int flags2_requirements[16];           /* An array of which bits in Flags 2 of
                                          the header will need to be set:
                                          e.g. if the save_undo / restore_undo
                                          opcodes are ever assembled, we have
                                          to set the "games want UNDO" bit.
                                          Values are 0 or 1.                 */

/* ------------------------------------------------------------------------- */
/*   Construct story/module file (up to code area start).                    */
/*                                                                           */
/*   (To understand what follows, you really need to look at the run-time    */
/*   system's specification, the Z-Machine Standards document.)              */
/* ------------------------------------------------------------------------- */

extern void write_serial_number(char *buffer)
{
    /*  Note that this function may require modification for "ANSI" compilers
        which do not provide the standard time functions: what is needed is
        the ability to work out today's date                                 */

    time_t tt;  tt=time(0);
    if (serial_code_given_in_program)
        strcpy(buffer, serial_code_buffer);
    else
#ifdef TIME_UNAVAILABLE
        sprintf(buffer,"970000");
#else
        strftime(buffer,10,"%y%m%d",localtime(&tt));
#endif
}

static void percentage(char *name, int32 x, int32 total)
{   printf("   %-20s %2d.%d%%\n",name,x*100/total,(x*1000/total)%10);
}

static char *version_name(int v)
{   switch(v)
    {   case 3: return "Standard";
        case 4: return "Plus";
        case 5: return "Advanced";
        case 6: return "Graphical";
        case 8: return "Extended";
    }
    return "experimental format";
}

static int32 rough_size_of_paged_memory(void)
{
    /*  This function calculates a modest over-estimate of the amount of
        memory required to store the Z-machine's paged memory area
        (that is, everything up to the start of the code area).              */

    int32 total, i;

    total = 64                                                     /* header */
            + 2 + subtract_pointers(low_strings_top, low_strings)
                                                         /* low strings pool */
            + 6*32;                                   /* abbreviations table */

    total += 8;                                    /* header extension table */

    if (alphabet_modified) total += 78;               /* character set table */

    if (zscii_defn_modified)                    /* Unicode translation table */
        total += 2 + 2*zscii_high_water_mark;

    total += 2*((version_number==3)?31:63)        /* property default values */
            + no_objects*((version_number==3)?9:14)     /* object tree table */
            + properties_table_size            /* property values of objects */
            + (no_classes+1)*(module_switch?4:2)
                                               /* class object numbers table */
            + no_individual_properties*2 + 2         /* table for prop names */
            + 48*2 + no_actions*2          /* and attribute and action names */
            + individuals_length                 /* tables of prop variables */
            + dynamic_array_area_size;               /* variables and arrays */

    for (i=0; i<no_Inform_verbs; i++)
        total += 2 + 1 +                        /* address of grammar table, */
                                                  /* number of grammar lines */
                 (grammar_version_number == 1)?
                 (8*Inform_verbs[i].lines):0;               /* grammar lines */

    if (grammar_version_number != 1)
        total += grammar_lines_top;            /* size of grammar lines area */

    total +=  2 + 4*no_adjectives                        /* adjectives table */
              + 2*no_actions                              /* action routines */
              + 2*no_grammar_token_routines;     /* general parsing routines */

    total += (subtract_pointers(dictionary_top, dictionary))  /* dictionary */
             + ((module_switch)?18:0);                        /* module map */

    total += scale_factor*0x100            /* maximum null bytes before code */
            + 1000;             /* fudge factor (in case the above is wrong) */

    return(total);
}

extern void construct_storyfile(void)
{   uchar *p;
    int32 i, j, k, l, mark, objs, strings_length,
          limit, excess, extend_offset;
    int32 globals_at, link_table_at, dictionary_at, actions_at, preactions_at,
          abbrevs_at, prop_defaults_at, object_tree_at, object_props_at,
          map_of_module, grammar_table_at, charset_at, headerext_at,
          unicode_at;
    char *output_called = (module_switch)?"module":"story file";

    individual_name_strings =
        my_calloc(sizeof(int32), no_individual_properties,
            "identifier name strings");
    action_name_strings =
        my_calloc(sizeof(int32), no_actions,
            "action name strings");
    attribute_name_strings =
        my_calloc(sizeof(int32), 48,
            "attribute name strings");

    write_the_identifier_names();

    /*  We now know how large the buffer to hold our construction has to be  */

    zmachine_paged_memory =
        my_malloc(rough_size_of_paged_memory(), "output buffer");

    /*  Foolish code to make this routine compile on all ANSI compilers      */

    p = (uchar *) zmachine_paged_memory;

    /*  In what follows, the "mark" will move upwards in memory: at various
        points its value will be recorded for milestones like
        "dictionary table start".  It begins at 0x40, just after the header  */

    mark = 0x40;

    /*  ----------------- Low Strings and Abbreviations -------------------- */

    p[mark]=0x80; p[mark+1]=0; mark+=2;        /* Start the low strings pool
                                         with a useful default string, "   " */

    for (i=0; i+low_strings<low_strings_top; mark++, i++) /* Low strings pool */
        p[0x42+i]=low_strings[i];

    abbrevs_at = mark;
    for (i=0; i<3*32; i++)                       /* Initially all 96 entries */
    {   p[mark++]=0; p[mark++]=0x20;                     /* are set to "   " */
    }
    for (i=0; i<no_abbreviations; i++)            /* Write any abbreviations */
    {   j=abbrev_values[i];                            /* into banks 2 and 3 */
        p[abbrevs_at+64+2*i]=j/256;               /* (bank 1 is reserved for */
        p[abbrevs_at+65+2*i]=j%256;                   /* "variable strings") */
    }

    /*  ------------------- Header extension table ------------------------- */

    headerext_at = mark;
    p[mark++] = 0; p[mark++] = 3;                  /* Currently 3 words long */
    for (i=2; i<8; i++) p[mark++] = 0;

    /*  -------------------- Z-character set table ------------------------- */

    if (alphabet_modified)
    {   charset_at = mark;
        for (i=0;i<3;i++) for (j=0;j<26;j++)
        {   if (alphabet[i][j] == '~') p[mark++] = '\"';
            else p[mark++] = alphabet[i][j];
        }
    }

    /*  ------------------ Unicode translation table ----------------------- */

    unicode_at = 0;
    if (zscii_defn_modified)
    {   unicode_at = mark;
        p[mark++] = zscii_high_water_mark;
        for (i=0;i<zscii_high_water_mark;i++)
        {   j = zscii_to_unicode(155 + i);
            p[mark++] = j/256; p[mark++] = j%256;
        }
    }

    /*  -------------------- Objects and Properties ------------------------ */

    prop_defaults_at = mark;

    p[mark++]=0; p[mark++]=0;

    for (i=2; i< ((version_number==3)?32:64); i++)
    {   p[mark++]=prop_default_value[i]/256;
        p[mark++]=prop_default_value[i]%256;
    }

    object_tree_at = mark;

    mark += ((version_number==3)?9:14)*no_objects;

    object_props_at = mark;

    for (i=0; i<properties_table_size; i++)
        p[mark+i]=properties_table[i];

    for (i=0, objs=object_tree_at; i<no_objects; i++)
    {
        if (version_number == 3)
        {   p[objs]=objects[i].atts[0];
            p[objs+1]=objects[i].atts[1];
            p[objs+2]=objects[i].atts[2];
            p[objs+3]=objects[i].atts[3];
            p[objs+4]=objects[i].parent;
            p[objs+5]=objects[i].next;
            p[objs+6]=objects[i].child;
            p[objs+7]=mark/256;
            p[objs+8]=mark%256;
            objs+=9;
        }
        else
        {   p[objs]=objects[i].atts[0];
            p[objs+1]=objects[i].atts[1];
            p[objs+2]=objects[i].atts[2];
            p[objs+3]=objects[i].atts[3];
            p[objs+4]=objects[i].atts[4];
            p[objs+5]=objects[i].atts[5];
            p[objs+6]=(objects[i].parent)/256;
            p[objs+7]=(objects[i].parent)%256;
            p[objs+8]=(objects[i].next)/256;
            p[objs+9]=(objects[i].next)%256;
            p[objs+10]=(objects[i].child)/256;
            p[objs+11]=(objects[i].child)%256;
            if (!module_switch)
            {   p[objs+12]=mark/256;
                p[objs+13]=mark%256;
            }
            else
            {   p[objs+12]=objects[i].propsize/256;
                p[objs+13]=objects[i].propsize%256;
            }
            objs+=14;
        }
        mark+=objects[i].propsize;
    }

    /*  ----------- Table of Class Prototype Object Numbers ---------------- */

    class_numbers_offset = mark;
    for (i=0; i<no_classes; i++)
    {   p[mark++] = class_object_numbers[i]/256;
        p[mark++] = class_object_numbers[i]%256;
        if (module_switch)
        {   p[mark++] = class_begins_at[i]/256;
            p[mark++] = class_begins_at[i]%256;
        }
    }
    p[mark++] = 0;
    p[mark++] = 0;

    /*  ---------------- Table of Indiv Property Values -------------------- */

    identifier_names_offset = mark;

    if (!module_switch)
    {   p[mark++] = no_individual_properties/256;
        p[mark++] = no_individual_properties%256;
        for (i=1; i<no_individual_properties; i++)
        {   p[mark++] = individual_name_strings[i]/256;
            p[mark++] = individual_name_strings[i]%256;
        }

        for (i=0; i<48; i++)
        {   p[mark++] = attribute_name_strings[i]/256;
            p[mark++] = attribute_name_strings[i]%256;
        }

        for (i=0; i<no_actions; i++)
        {   p[mark++] = action_name_strings[i]/256;
            p[mark++] = action_name_strings[i]%256;
        }
    }

    individuals_offset = mark;
    for (i=0; i<individuals_length; i++)
        p[mark++] = individuals_table[i];

    /*  ----------------- Variables and Dynamic Arrays --------------------- */

    globals_at = mark;

    for (i=0; i<dynamic_array_area_size; i++)
        p[mark++] = dynamic_array_area[i];

    for (i=0; i<240; i++)
    {   j=global_initial_value[i];
        p[globals_at+i*2]   = j/256; p[globals_at+i*2+1] = j%256;
    }

    /*  ------------------------ Grammar Table ----------------------------- */

    if (grammar_version_number > 2)
    {   warning("This version of Inform is unable to produce the grammar \
table format requested (producing number 2 format instead)");
        grammar_version_number = 2;
    }

    grammar_table_at = mark;

    mark = mark + no_Inform_verbs*2;

    for (i=0; i<no_Inform_verbs; i++)
    {   p[grammar_table_at + i*2] = (mark/256);
        p[grammar_table_at + i*2 + 1] = (mark%256);
        p[mark++] = Inform_verbs[i].lines;
        for (j=0; j<Inform_verbs[i].lines; j++)
        {   k = Inform_verbs[i].l[j];
            if (grammar_version_number == 1)
            {   int m, n;
                p[mark+7] = grammar_lines[k+1];
                for (m=1;m<=6;m++) p[mark + m] = 0;
                k = k + 2; m = 1; n = 0;
                while ((grammar_lines[k] != 15) && (m<=6))
                {   p[mark + m] = grammar_lines[k];
                    if (grammar_lines[k] < 180) n++;
                    m++; k = k + 3;
                }
                p[mark] = n;
                mark = mark + 8;
            }
            else
            {   int tok;
                p[mark++] = grammar_lines[k++];
                p[mark++] = grammar_lines[k++];
                for (;;)
                {   tok = grammar_lines[k++];
                    p[mark++] = tok;
                    if (tok == 15) break;
                    p[mark++] = grammar_lines[k++];
                    p[mark++] = grammar_lines[k++];
                }
            }
        }
    }

    /*  ------------------- Actions and Preactions ------------------------- */
    /*  (The term "preactions" is traditional: Inform uses the preactions    */
    /*  table for a different purpose than Infocom used to.)                 */
    /*  The values are written later, when the Z-code offset is known.       */
    /*  -------------------------------------------------------------------- */

    actions_at = mark;
    mark += no_actions*2;

    preactions_at = mark;
    if (grammar_version_number == 1)
        mark += no_grammar_token_routines*2;

    /*  ----------------------- Adjectives Table --------------------------- */

    if (grammar_version_number == 1)
    {   p[mark]=0; p[mark+1]=no_adjectives; mark+=2; /* To assist "infodump" */
        adjectives_offset = mark;
        dictionary_offset = mark + 4*no_adjectives;

        for (i=0; i<no_adjectives; i++)
        {   j = final_dict_order[adjectives[no_adjectives-i-1]]
                *((version_number==3)?7:9)
                + dictionary_offset + 7;
            p[mark++]=j/256; p[mark++]=j%256; p[mark++]=0;
            p[mark++]=(256-no_adjectives+i);
        }
    }
    else
    {   p[mark]=0; p[mark+1]=0; mark+=2;
        adjectives_offset = mark;
        dictionary_offset = mark;
    }

    /*  ------------------------- Dictionary ------------------------------- */

    dictionary_at=mark;

    dictionary[0]=3; dictionary[1]='.';        /* Non-space characters which */
                     dictionary[2]=',';                 /* force words apart */
                     dictionary[3]='"';

    dictionary[4]=(version_number==3)?7:9;           /* Length of each entry */
    dictionary[5]=(dict_entries/256);                   /* Number of entries */
    dictionary[6]=(dict_entries%256);

    for (i=0; i<7; i++) p[mark++] = dictionary[i];

    for (i=0; i<dict_entries; i++)
    {   k = 7 + i*((version_number==3)?7:9);
        j = mark + final_dict_order[i]*((version_number==3)?7:9);
        for (l = 0; l<((version_number==3)?7:9); l++)
            p[j++] = dictionary[k++];
    }
    mark += dict_entries * ((version_number==3)?7:9);

    /*  ------------------------- Module Map ------------------------------- */

    if (module_switch)
    {   map_of_module = mark;                             /* Filled in below */
        mark += 30;
    }

    /*  ----------------- A gap before the code area ----------------------- */
    /*  (so that it will start at an exact packed address and so that all    */
    /*  routine packed addresses are >= 256, hence long constants)           */
    /*  -------------------------------------------------------------------- */

    while ((mark%length_scale_factor) != 0) p[mark++]=0;
    while (mark < (scale_factor*0x100)) p[mark++]=0;

    /*  -------------------------- Code Area ------------------------------- */
    /*  (From this point on we don't write any more into the "p" buffer.)    */
    /*  -------------------------------------------------------------------- */

    Write_Code_At = mark;
    mark += zmachine_pc;

    /*  ------------------ Another synchronising gap ----------------------- */

    while ((mark%scale_factor) != 0) mark++;

    /*  ------------------------- Strings Area ----------------------------- */

    Write_Strings_At = mark;
    strings_length = static_strings_extent;
    mark += strings_length;

    /*  --------------------- Module Linking Data -------------------------- */

    if (module_switch)
    {   link_table_at = mark; mark += link_data_size;
        mark += zcode_backpatch_size;
        mark += zmachine_backpatch_size;
    }

    /*  --------------------- Is the file too big? ------------------------- */

    Out_Size = mark;

    switch(version_number)
    {   case 3: excess = Out_Size-((int32) 0x20000L); limit = 128; break;
        case 4:
        case 5: excess = Out_Size-((int32) 0x40000L); limit = 256; break;
        case 6:
        case 8: excess = Out_Size-((int32) 0x80000L); limit = 512; break;
        case 7: excess = 0;                           limit = 320; break;
    }

    if (module_switch)
    {   excess = Out_Size-((int32) 0x10000L); limit=64;
    }

    if (excess > 0)
    {   char memory_full_error[80];
        sprintf(memory_full_error,
            "The %s exceeds version-%d limit (%dK) by %d bytes",
             output_called, version_number, limit, excess);
        fatalerror(memory_full_error);
    }

    /*  --------------------------- Offsets -------------------------------- */

    dictionary_offset = dictionary_at;
    variables_offset = globals_at;
    actions_offset = actions_at;
    preactions_offset = preactions_at;
    prop_defaults_offset = prop_defaults_at;
    prop_values_offset = object_props_at;

    if (extend_memory_map)
    {   extend_offset=256;
        if (no_objects+9 > extend_offset) extend_offset=no_objects+9;
        while ((extend_offset%length_scale_factor) != 0) extend_offset++;
        code_offset = extend_offset*scale_factor;
        strings_offset = code_offset + (Write_Strings_At-Write_Code_At);
    }
    else
    {   code_offset = Write_Code_At;
        strings_offset = Write_Strings_At;
    }

    /*  --------------------------- The Header ----------------------------- */

    for (i=0; i<=0x3f; i++) p[i]=0;             /* Begin with 64 blank bytes */

    p[0] = version_number;                                 /* Version number */
    p[1] = statusline_flag*2;          /* Bit 1 of Flags 1: statusline style */
    p[2] = (release_number/256);
    p[3] = (release_number%256);                                  /* Release */
    p[4] = (Write_Code_At/256);
    p[5] = (Write_Code_At%256);                       /* End of paged memory */
    if (version_number==6)
    {   j=code_offset/scale_factor;            /* Packed address of "Main__" */
        p[6]=(j/256); p[7]=(j%256);
    }
    else
    {   j=Write_Code_At+1;                       /* Initial PC value (bytes) */
        p[6]=(j/256); p[7]=(j%256);            /* (first opcode in "Main__") */
    }
    p[8] = (dictionary_at/256); p[9]=(dictionary_at%256);      /* Dictionary */
    p[10]=prop_defaults_at/256; p[11]=prop_defaults_at%256;       /* Objects */
    p[12]=(globals_at/256); p[13]=(globals_at%256);          /* Dynamic area */
    p[14]=(grammar_table_at/256);
    p[15]=(grammar_table_at%256);                                 /* Grammar */
    for (i=0, j=0, k=1;i<16;i++, k=k*2)         /* Flags 2 as needed for any */
        j+=k*flags2_requirements[i];            /* unusual opcodes assembled */
    p[16]=j/256; p[17]=j%256;
    write_serial_number((char *) (p+18)); /* Serial number: 6 chars of ASCII */
    p[24]=abbrevs_at/256;
    p[25]=abbrevs_at%256;                             /* Abbreviations table */
    p[26]=0; p[27]=0;            /* Length of file to be filled in "files.c" */
    p[28]=0; p[29]=0;                  /* Checksum to be filled in "files.c" */

    if (extend_memory_map)
    {   j=(Write_Code_At - extend_offset*scale_factor)/length_scale_factor;
        p[40]=j/256; p[41]=j%256;                         /* Routines offset */
        p[42]=j/256; p[43]=j%256;                        /* = Strings offset */
    }

    if (alphabet_modified)
    {   j = charset_at;
        p[52]=j/256; p[53]=j%256; }           /* Character set table address */

    j = headerext_at;
    p[54] = j/256; p[55] = j%256;          /* Header extension table address */

    p[60] = '0' + ((RELEASE_NUMBER/100)%10);
    p[61] = '.';
    p[62] = '0' + ((RELEASE_NUMBER/10)%10);
    p[63] = '0' + RELEASE_NUMBER%10;

    /*  ------------------------ Header Extension -------------------------- */

    i = headerext_at + 2;
    p[i++] = 0; p[i++] = 0;                       /* Mouse x-coordinate slot */
    p[i++] = 0; p[i++] = 0;                       /* Mouse y-coordinate slot */
    j = unicode_at;
    p[i++] = j/256; p[i++] = j%256;     /* Unicode translation table address */

    /*  ----------------- The Header: Extras for modules ------------------- */

    if (module_switch)
    {   p[0]=p[0]+64;
        p[1]=MODULE_VERSION_NUMBER;
        p[6]=map_of_module/256;
        p[7]=map_of_module%256;

        mark = map_of_module;                       /*  Module map format:   */

        p[mark++]=object_tree_at/256;               /*  0: Object tree addr  */
        p[mark++]=object_tree_at%256;
        p[mark++]=object_props_at/256;              /*  2: Prop values addr  */
        p[mark++]=object_props_at%256;
        p[mark++]=(Write_Strings_At/scale_factor)/256;  /*  4: Static strs   */
        p[mark++]=(Write_Strings_At/scale_factor)%256;
        p[mark++]=class_numbers_offset/256;         /*  6: Class nos addr    */
        p[mark++]=class_numbers_offset%256;
        p[mark++]=individuals_offset/256;           /*  8: Indiv prop values */
        p[mark++]=individuals_offset%256;
        p[mark++]=individuals_length/256;           /*  10: Length of table  */
        p[mark++]=individuals_length%256;
        p[mark++]=no_symbols/256;                   /*  12: No of symbols    */
        p[mark++]=no_symbols%256;
        p[mark++]=no_individual_properties/256;     /*  14: Max property no  */
        p[mark++]=no_individual_properties%256;
        p[mark++]=no_objects/256;                   /*  16: No of objects    */
        p[mark++]=no_objects%256;
        i = link_table_at;
        p[mark++]=i/256;                            /*  18: Import/exports   */
        p[mark++]=i%256;
        p[mark++]=link_data_size/256;               /*  20: Size of          */
        p[mark++]=link_data_size%256;
        i += link_data_size;
        p[mark++]=i/256;                            /*  22: Code backpatch   */
        p[mark++]=i%256;
        p[mark++]=zcode_backpatch_size/256;         /*  24: Size of          */
        p[mark++]=zcode_backpatch_size%256;
        i += zcode_backpatch_size;
        p[mark++]=i/256;                            /*  26: Image backpatch  */
        p[mark++]=i%256;
        p[mark++]=zmachine_backpatch_size/256;      /*  28: Size of          */
        p[mark++]=zmachine_backpatch_size%256;

        /*  Further space in this table is reserved for future use  */
    }

    /*  ---- Backpatch the Z-machine, now that all information is in ------- */

    if (!module_switch)
    {   backpatch_zmachine_image();
        for (i=1; i< no_actions + 48 + no_individual_properties; i++)
        {   int32 v = 256*p[identifier_names_offset + i*2]
                      + p[identifier_names_offset + i*2 + 1];
            if (v!=0) v += strings_offset/scale_factor;
            p[identifier_names_offset + i*2] = v/256;
            p[identifier_names_offset + i*2 + 1] = v%256;
        }

        mark = actions_at;
        for (i=0; i<no_actions; i++)
        {   j=action_byte_offset[i] + code_offset/scale_factor;
            p[mark++]=j/256; p[mark++]=j%256;
        }

        if (grammar_version_number == 1)
        {   mark = preactions_at;
            for (i=0; i<no_grammar_token_routines; i++)
            {   j=grammar_token_routine[i] + code_offset/scale_factor;
                p[mark++]=j/256; p[mark++]=j%256;
            }
        }
        else
        {   for (l = 0; l<no_Inform_verbs; l++)
            {   k = grammar_table_at + 2*l;
                i = p[k]*256 + p[k+1];
                for (j = p[i++]; j>0; j--)
                {   int topbits; int32 value;
                    i = i + 2;
                    while (p[i] != 15)
                    {   topbits = (p[i]/0x40) & 3;
                        value = p[i+1]*256 + p[i+2];
                        switch(topbits)
                        {   case 1:
                                value = final_dict_order[value]
                                        *((version_number==3)?7:9)
                                        + dictionary_offset + 7;
                                break;
                            case 2:
                                value += code_offset/scale_factor;
                                break;
                        }
                        p[i+1] = value/256; p[i+2] = value%256;
                        i = i + 3;
                    }
                    i++;
                }
            }
        }
    }

    /*  ---- From here on, it's all reportage: construction is finished ---- */

    if (statistics_switch)
    {   int32 k_long, rate; char *k_str="";
        k_long=(Out_Size/1024);
        if ((Out_Size-1024*k_long) >= 512) { k_long++; k_str=""; }
        else if ((Out_Size-1024*k_long) > 0) { k_str=".5"; }
        if (total_bytes_trans == 0) rate = 0;
        else rate=total_bytes_trans*1000/total_chars_trans;

        {   printf("In:\
%3d source code files            %6d syntactic lines\n\
%6d textual lines                %6ld characters ",
            input_file, no_syntax_lines,
            total_source_line_count, (long int) total_chars_read);
            if (character_set_setting == 0) printf("(plain ASCII)\n");
            else
            {   printf("(ISO 8859-%d %s)\n", character_set_setting,
                    name_of_iso_set(character_set_setting));
            }

            printf("Allocated:\n\
%6d symbols (maximum %4d)       %6ld bytes of memory\n\
Out:   Version %d \"%s\" %s %d.%c%c%c%c%c%c (%ld%sK long):\n",
                 no_symbols, MAX_SYMBOLS,
                 (long int) malloced_bytes,
                 version_number,
                 version_name(version_number),
                 output_called,
                 release_number, p[18], p[19], p[20], p[21], p[22], p[23],
                 (long int) k_long, k_str);

            printf("\
%6d classes (maximum %2d)         %6d objects (maximum %3d)\n\
%6d global vars (maximum 233)    %6d variable/array space (maximum %d)\n",
                 no_classes, MAX_CLASSES,
                 no_objects, ((version_number==3)?255:(MAX_OBJECTS-1)),
                 no_globals,
                 dynamic_array_area_size, MAX_STATIC_DATA);

            printf(
"%6d verbs (maximum %3d)          %6d dictionary entries (maximum %d)\n\
%6d grammar lines (version %d)    %6d grammar tokens (unlimited)\n\
%6d actions (maximum %3d)        %6d attributes (maximum %2d)\n\
%6d common props (maximum %2d)    %6d individual props (unlimited)\n",
                 no_Inform_verbs, MAX_VERBS,
                 dict_entries, MAX_DICT_ENTRIES,
                 no_grammar_lines, grammar_version_number,
                 no_grammar_tokens,
                 no_actions, MAX_ACTIONS,
                 no_attributes, ((version_number==3)?32:48),
                 no_properties-2, ((version_number==3)?30:62),
                 no_individual_properties - 64);

            printf(
"%6ld characters used in text      %6ld bytes compressed (rate %d.%3ld)\n\
%6d abbreviations (maximum %d)   %6d routines (unlimited)\n\
%6ld instructions of Z-code       %6d sequence points\n\
%6ld bytes used in Z-machine      %6ld bytes free in Z-machine\n",
                 (long int) total_chars_trans,
                 (long int) total_bytes_trans,
                 (total_chars_trans>total_bytes_trans)?0:1,
                 (long int) rate,
                 no_abbreviations, MAX_ABBREVS,
                 no_routines,
                 (long int) no_instructions, no_sequence_points,
                 (long int) Out_Size,
                 (long int)
                      (((long int) (limit*1024L)) - ((long int) Out_Size)));
        }
    }

    if (offsets_switch)
    {
        {   printf(
"\nOffsets in %s:\n\
%05lx Synonyms     %05lx Defaults     %05lx Objects    %05lx Properties\n\
%05lx Variables    %05lx Parse table  %05lx Actions    %05lx Preactions\n\
%05lx Adjectives   %05lx Dictionary   %05lx Code       %05lx Strings\n",
            output_called,
            (long int) abbrevs_at,
            (long int) prop_defaults_at,
            (long int) object_tree_at,
            (long int) object_props_at,
            (long int) globals_at,
            (long int) grammar_table_at,
            (long int) actions_at,
            (long int) preactions_at,
            (long int) adjectives_offset,
            (long int) dictionary_at,
            (long int) Write_Code_At,
            (long int) Write_Strings_At);
            if (module_switch)
                printf("%05lx Linking data\n",(long int) link_table_at);
        }
    }

    if (debugfile_switch)
    {   write_debug_byte(MAP_DBR);
        write_debug_string("abbreviations table");
          write_debug_address(abbrevs_at);
        write_debug_string("header extension");
          write_debug_address(headerext_at);
        if (alphabet_modified)
        {   write_debug_string("alphabets table");
              write_debug_address(charset_at);
        }
        if (zscii_defn_modified)
        {   write_debug_string("Unicode table");
              write_debug_address(unicode_at);
        }

        write_debug_string("property defaults");
          write_debug_address(prop_defaults_at);
        write_debug_string("object tree");
          write_debug_address(object_tree_at);
        write_debug_string("common properties");
          write_debug_address(object_props_at);
        write_debug_string("class numbers");
          write_debug_address(class_numbers_offset);
        write_debug_string("individual properties");
          write_debug_address(individuals_offset);
        write_debug_string("global variables");
          write_debug_address(globals_at);
        write_debug_string("array space");
          write_debug_address(globals_at+480);
        write_debug_string("grammar table");
          write_debug_address(grammar_table_at);
        write_debug_string("actions table");
          write_debug_address(actions_at);
        write_debug_string("parsing routines");
          write_debug_address(preactions_at);
        write_debug_string("adjectives table");
          write_debug_address(adjectives_offset);
        write_debug_string("dictionary");
          write_debug_address(dictionary_at);
        write_debug_string("code area");
          write_debug_address(Write_Code_At);
        write_debug_string("strings area");
          write_debug_address(Write_Strings_At);
        write_debug_byte(0);
    }

    if (memory_map_switch)
    {
        {
printf("Dynamic +---------------------+   00000\n");
printf("memory  |       header        |\n");
printf("        +---------------------+   00040\n");
printf("        |    abbreviations    |\n");
printf("        + - - - - - - - - - - +   %05lx\n", (long int) abbrevs_at);
printf("        | abbreviations table |\n");
printf("        +---------------------+   %05lx\n", (long int) headerext_at);
printf("        |  header extension   |\n");
            if (alphabet_modified)
            {
printf("        + - - - - - - - - - - +   %05lx\n", (long int) charset_at);
printf("        |   alphabets table   |\n");
            }
            if (zscii_defn_modified)
            {
printf("        + - - - - - - - - - - +   %05lx\n", (long int) unicode_at);
printf("        |    Unicode table    |\n");
            }
printf("        +---------------------+   %05lx\n",
                                          (long int) prop_defaults_at);
printf("        |  property defaults  |\n");
printf("        + - - - - - - - - - - +   %05lx\n", (long int) object_tree_at);
printf("        |       objects       |\n");
printf("        + - - - - - - - - - - +   %05lx\n",
                                          (long int) object_props_at);
printf("        | object short names, |\n");
printf("        | common prop values  |\n");
printf("        + - - - - - - - - - - +   %05lx\n",
                                          (long int) class_numbers_offset);
printf("        | class numbers table |\n");
printf("        + - - - - - - - - - - +   %05lx\n",
                                          (long int) individuals_offset);
printf("        | indiv prop values   |\n");
printf("        +---------------------+   %05lx\n", (long int) globals_at);
printf("        |  global variables   |\n");
printf("        + - - - - - - - - - - +   %05lx\n",
                                          ((long int) globals_at)+480L);
printf("        |       arrays        |\n");
printf("        +=====================+   %05lx\n",
                                          (long int) grammar_table_at);
printf("Static  |    grammar table    |\n");
printf("cached  + - - - - - - - - - - +   %05lx\n", (long int) actions_at);
printf("data    |       actions       |\n");
printf("        + - - - - - - - - - - +   %05lx\n", (long int) preactions_at);
printf("        |   parsing routines  |\n");
printf("        + - - - - - - - - - - +   %05lx\n",
                                          (long int) adjectives_offset);
printf("        |     adjectives      |\n");
printf("        +---------------------+   %05lx\n", (long int) dictionary_at);
printf("        |     dictionary      |\n");
if (module_switch)
{
printf("        + - - - - - - - - - - +   %05lx\n",
                                          (long int) map_of_module);
printf("        | map of module addrs |\n");
}
printf("        +---------------------+   %05lx\n", (long int) Write_Code_At);
printf("Static  |       Z-code        |\n");
printf("paged   +---------------------+   %05lx\n",
                                          (long int) Write_Strings_At);
printf("data    |       strings       |\n");
if (module_switch)
{
printf("        +=====================+   %05lx\n", (long int) link_table_at);
printf("        | module linking data |\n");
}
printf("        +---------------------+   %05lx\n", (long int) Out_Size);
        }
    }
    if (percentages_switch)
    {   printf("Approximate percentage breakdown of %s:\n",
                output_called);
        percentage("Z-code",                zmachine_pc,Out_Size);
        if (module_switch)
            percentage("Linking data",   link_data_size,Out_Size);
        percentage("Static strings",     strings_length,Out_Size);
        percentage("Dictionary",         Write_Code_At-dictionary_at,Out_Size);
        percentage("Objects",            globals_at-prop_defaults_at,Out_Size);
        percentage("Globals",            grammar_table_at-globals_at,Out_Size);
        percentage("Parsing tables",   dictionary_at-grammar_table_at,Out_Size);
        percentage("Header and synonyms", prop_defaults_at,Out_Size);
        percentage("Total of save area", grammar_table_at,Out_Size);
        percentage("Total of text",      total_bytes_trans,Out_Size);
    }
    if (frequencies_switch)
    {
        {   printf("How frequently abbreviations were used, and roughly\n");
            printf("how many bytes they saved:  ('_' denotes spaces)\n");
            for (i=0; i<no_abbreviations; i++)
            {   char abbrev_string[64];
                strcpy(abbrev_string,
                    (char *)abbreviations_at+i*MAX_ABBREV_LENGTH);
                for (j=0; abbrev_string[j]!=0; j++)
                    if (abbrev_string[j]==' ') abbrev_string[j]='_';
                printf("%10s %5d/%5d   ",abbrev_string,abbrev_freqs[i],
                    2*((abbrev_freqs[i]-1)*abbrev_quality[i])/3);
                if ((i%3)==2) printf("\n");
            }
            if ((i%3)!=0) printf("\n");
            if (no_abbreviations==0) printf("None were declared.\n");
        }
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_tables_vars(void)
{
    serial_code_given_in_program = FALSE;
    release_number = 1;
    statusline_flag = SCORE_STYLE;

    zmachine_paged_memory = NULL;

    code_offset = 0x800;
    actions_offset = 0x800;
    preactions_offset = 0x800;
    dictionary_offset = 0x800;
    adjectives_offset = 0x800;
    variables_offset = 0;
    strings_offset = 0xc00;
    individuals_offset=0x800;
    identifier_names_offset=0x800;
    class_numbers_offset = 0x800;
}

extern void tables_begin_pass(void)
{
}

extern void tables_allocate_arrays(void)
{
}

extern void tables_free_arrays(void)
{
    /*  Allocation for this array happens in construct_storyfile() above     */

    my_free(&zmachine_paged_memory,"output buffer");
}

/* ========================================================================= */
