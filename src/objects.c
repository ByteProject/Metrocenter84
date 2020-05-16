/* ------------------------------------------------------------------------- */
/*   "objects" :  [1] the object-maker, which constructs objects and enters  */
/*                    them into the tree, given a low-level specification;   */
/*                                                                           */
/*                [2] the parser of Object/Nearby/Class directives, which    */
/*                    checks syntax and translates such directives into      */
/*                    specifications for the object-maker.                   */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

/* ------------------------------------------------------------------------- */
/*   Objects.                                                                */
/* ------------------------------------------------------------------------- */

int no_objects;                        /* Number of objects made so far      */

static int no_embedded_routines;       /* Used for naming routines which
                                          are given as property values: these
                                          are called EmbeddedRoutine__1, ... */

static fpropt full_object;             /* "fpropt" is a typedef for a struct
                                          containing an array to hold the
                                          attribute and property values of
                                          a single object.  We only keep one
                                          of these, for the current object
                                          being made, and compile it into
                                          Z-machine tables when each object
                                          definition is complete, since
                                          sizeof(fpropt) is about 6200 bytes */
static char shortname_buffer[256];     /* Text buffer to hold the short name
                                          (which is read in first, but
                                          written almost last)               */
static int parent_of_this_obj;

static char *classname_text, *objectname_text;
                                       /* For printing names of embedded
                                          routines only                      */

/* ------------------------------------------------------------------------- */
/*   Classes.                                                                */
/* ------------------------------------------------------------------------- */
/*   Arrays defined below:                                                   */
/*                                                                           */
/*    int32 class_begins_at[n]            offset of properties block for     */
/*                                        nth class (always an offset        */
/*                                        inside the properties_table)       */
/*    int   classes_to_inherit_from[]     The list of classes to inherit     */
/*                                        from as taken from the current     */
/*                                        Nearby/Object/Class definition     */
/*    int   class_object_numbers[n]       The number of the prototype-object */
/*                                        for the nth class                  */
/* ------------------------------------------------------------------------- */

int        no_classes;                 /* Number of class defns made so far  */

static int current_defn_is_class,      /* TRUE if current Nearby/Object/Class
                                          defn is in fact a Class definition */
           no_classes_to_inherit_from; /* Number of classes in the list
                                          of classes to inherit in the
                                          current Nearby/Object/Class defn   */

/* ------------------------------------------------------------------------- */
/*   Making attributes and properties.                                       */
/* ------------------------------------------------------------------------- */

int no_attributes,                 /* Number of attributes defined so far    */
    no_properties;                 /* Number of properties defined so far,
                                      plus 1 (properties are numbered from
                                      1 and Inform creates "name" and two
                                      others itself, so the variable begins
                                      the compilation pass set to 4)         */

static void trace_s(char *name, int32 number, int f)
{   if (!printprops_switch) return;
    printf("%s  %02ld  ",(f==0)?"Attr":"Prop",(long int) number);
    if (f==0) printf("  ");
    else      printf("%s%s",(prop_is_long[number])?"L":" ",
                            (prop_is_additive[number])?"A":" ");
    printf("  %s\n",name);
}

extern void make_attribute(void)
{   int i; char *name;

    if (no_attributes==((version_number==3)?32:48))
    {   if (version_number==3)
            error("All 32 attributes already declared (compile as Advanced \
game to get an extra 16)");
        else
            error("All 48 attributes already declared");
        panic_mode_error_recovery(); return;
    }

    get_next_token();
    i = token_value; name = token_text;
    if ((token_type != SYMBOL_TT) || (!(sflags[i] & UNKNOWN_SFLAG)))
    {   ebf_error("new attribute name", token_text);
        panic_mode_error_recovery(); return;
    }

    directive_keywords.enabled = TRUE;
    get_next_token();
    directive_keywords.enabled = FALSE;

    if ((token_type == DIR_KEYWORD_TT) && (token_value == ALIAS_DK))
    {   get_next_token();
        if (!((token_type == SYMBOL_TT)
              && (stypes[token_value] == ATTRIBUTE_T)))
        {   ebf_error("an existing attribute name after 'alias'",
                token_text); panic_mode_error_recovery(); return;
        }
        assign_symbol(i, svals[token_value], ATTRIBUTE_T);
        sflags[token_value] |= ALIASED_SFLAG;
        sflags[i] |= ALIASED_SFLAG;
    }
    else
    {   assign_symbol(i, no_attributes++, ATTRIBUTE_T);
        put_token_back();
    }

    trace_s(name, svals[i], 0);
    return;
}

extern void make_property(void)
{   int32 default_value;
    int i, additive_flag=FALSE; char *name;
    assembly_operand AO;

    if (no_properties==((version_number==3)?32:64))
    {   if (version_number==3)
            error("All 30 properties already declared (compile as Advanced \
game to get an extra 62)");
        else
            error("All 62 properties already declared");
        panic_mode_error_recovery(); return;
    }

    do
    {   directive_keywords.enabled = TRUE;
        get_next_token();
        if ((token_type == DIR_KEYWORD_TT) && (token_value == LONG_DK))
            obsolete_warning("all properties are now automatically 'long'");
        else
        if ((token_type == DIR_KEYWORD_TT) && (token_value == ADDITIVE_DK))
            additive_flag = TRUE;
        else break;
    } while (TRUE);

    put_token_back();
    directive_keywords.enabled = FALSE;
    get_next_token();

    i = token_value; name = token_text;
    if ((token_type != SYMBOL_TT) || (!(sflags[i] & UNKNOWN_SFLAG)))
    {   ebf_error("new property name", token_text);
        panic_mode_error_recovery(); return;
    }

    directive_keywords.enabled = TRUE;
    get_next_token();
    directive_keywords.enabled = FALSE;

    if ((token_type == DIR_KEYWORD_TT) && (token_value == ALIAS_DK))
    {   if (additive_flag)
        {   error("'alias' incompatible with 'additive'");
            panic_mode_error_recovery();
            return;
        }
        get_next_token();
        if (!((token_type == SYMBOL_TT)
            && (stypes[token_value] == PROPERTY_T)))
        {   ebf_error("an existing property name after 'alias'",
                token_text); panic_mode_error_recovery(); return;
        }

        assign_symbol(i, svals[token_value], PROPERTY_T);
        trace_s(name, svals[i], 1);
        sflags[token_value] |= ALIASED_SFLAG;
        sflags[i] |= ALIASED_SFLAG;
        return;
    }

    default_value = 0;
    put_token_back();

    if (!((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
    {   AO = parse_expression(CONSTANT_CONTEXT);
        default_value = AO.value;
        if (AO.marker != 0)
            backpatch_zmachine(AO.marker, PROP_DEFAULTS_ZA, no_properties*2-2);
    }

    prop_default_value[no_properties] = default_value;
    prop_is_long[no_properties] = TRUE;
    prop_is_additive[no_properties] = additive_flag;

    assign_symbol(i, no_properties++, PROPERTY_T);
    trace_s(name, svals[i], 1);
}

/* ------------------------------------------------------------------------- */
/*   Properties.                                                             */
/* ------------------------------------------------------------------------- */

int32 prop_default_value[64];          /* Default values for properties      */
int prop_is_long[64],                  /* Property modifiers, TRUE or FALSE:
                                          "long" means "never write a 1-byte
                                          value to this property", and is an
                                          obsolete feature: since Inform 5
                                          all properties have been "long"    */
    prop_is_additive[64];              /* "additive" means that values
                                          accumulate rather than erase each
                                          other during class inheritance     */
char *properties_table;                /* Holds the table of property values
                                          (holding one block for each object
                                          and coming immediately after the
                                          object tree in Z-memory)           */
int properties_table_size;             /* Number of bytes in this table      */

/* ------------------------------------------------------------------------- */
/*   Individual properties                                                   */
/*                                                                           */
/*   Each new i.p. name is given a unique number.  These numbers start from  */
/*   72, since 0 is reserved as a null, 1 to 63 refer to common properties   */
/*   and 64 to 71 are kept for methods of the metaclass Class (for example,  */
/*   64 is "create").                                                        */
/*                                                                           */
/*   An object provides individual properties by having property 3 set to a  */
/*   non-zero value, which must be a byte address of a table in the form:    */
/*                                                                           */
/*       <record-1> ... <record-n> 00 00                                     */
/*                                                                           */
/*   where a <record> looks like                                             */
/*                                                                           */
/*       <identifier>              <size>  <up to 255 bytes of data>         */
/*       or <identifier + 0x8000>                                            */
/*       ----- 2 bytes ----------  1 byte  <size> number of bytes            */
/*                                                                           */
/*   The <identifier> part is the number allocated to the name of what is    */
/*   being provided.  The top bit of this word is set to indicate that       */
/*   although the individual property is being provided, it is provided      */
/*   only privately (so that it is inaccessible except to the object's own   */
/*   embedded routines).                                                     */
/* ------------------------------------------------------------------------- */

       int *property3_lists;           /* p3_l[n] = the offset of prop 3
                                          (i.p. table addr) for object n+1,
                                          or -1 if it has none               */

       int no_individual_properties;   /* Actually equal to the next
                                          identifier number to be allocated,
                                          so this is initially 72 even though
                                          none have been made yet.           */
static int individual_prop_table_size; /* Size of the table of individual
                                          properties so far for current obj  */
       uchar *individuals_table;       /* Table of records, each being the
                                          i.p. table for an object           */
       int i_m;                        /* Write mark position in the above   */
       int individuals_length;         /* Extent of individuals_table        */

/* ------------------------------------------------------------------------- */
/*   Arrays used by this file                                                */
/* ------------------------------------------------------------------------- */

objectt      *objects;
static int   *classes_to_inherit_from;
int          *class_object_numbers;
int32        *class_begins_at;


/* ------------------------------------------------------------------------- */
/*   Tracing for compiler maintenance                                        */
/* ------------------------------------------------------------------------- */

extern void list_object_tree(void)
{   int i;
    printf("obj   par nxt chl   Object tree:\n");
    for (i=0; i<no_objects; i++)
        printf("%3d   %3d %3d %3d\n",
            i+1,objects[i].parent,objects[i].next, objects[i].child);
}

/* ------------------------------------------------------------------------- */
/*   Object and class manufacture begins here.                               */
/*                                                                           */
/*   These definitions have headers (parsed far, far below) and a series     */
/*   of segments, introduced by keywords and optionally separated by commas. */
/*   Each segment has its own parsing routine.  Note that when errors are    */
/*   detected, parsing continues rather than being abandoned, which assists  */
/*   a little in "error recovery" (i.e. in stopping lots more errors being   */
/*   produced for essentially the same mistake).                             */
/* ------------------------------------------------------------------------- */

extern int object_provides(int obj, int id)
{
    /*  Does the given object provide the identifier id?

        This is a matter of searching the object's property 3 list,
        but for speed and convenience we have kept an array of the
        addresses of these.

        Returns: 0 if not provided
                 1 if provided but "private" to that object
                 2 if provided and on public access                          */

    uchar *p; int i=0, b1, b2, c1;

    if (property3_lists[obj-1] == -1) return 0;

    p = individuals_table + property3_lists[obj-1];

    b1 = id/256; b2 = id%256; c1 = b1 | 0x80;

    while ((p[i] != 0) || (p[i+1]!=0))
    {   if ((p[i] == b1) && (p[i+1] == b2)) return 2;
        if ((p[i] == c1) && (p[i+1] == b2)) return 1;
        i = i + p[i+2] + 3;
    }
    return 0;
}

/* ========================================================================= */
/*   [1]  The object-maker: builds an object from a specification, viz.:     */
/*                                                                           */
/*           full_object,                                                    */
/*           shortname_buffer,                                               */
/*           parent_of_this_obj,                                             */
/*           current_defn_is_class (flag)                                    */
/*           classes_to_inherit_from[], no_classes_to_inherit_from,          */
/*           individual_prop_table_size (to date  )                          */
/*                                                                           */
/*   For efficiency's sake, the individual properties table has already been */
/*   created (as far as possible, i.e., all except for inherited individual  */
/*   properties); unless the flag is clear, in which case the actual         */
/*   definition did not specify any individual properties.                   */
/* ========================================================================= */
/*   Property inheritance from classes.                                      */
/* ------------------------------------------------------------------------- */

static void property_inheritance(void)
{
    /*  Apply the property inheritance rules to full_object, which should
        initially be complete (i.e., this routine takes place after the whole
        Nearby/Object/Class definition has been parsed through).

        On exit, full_object contains the final state of the properties to
        be written.                                                          */

    int i, j, k, kmax, class, mark,
        prop_number, prop_length, prop_in_current_defn;
    uchar *class_prop_block;

    for (class=0; class<no_classes_to_inherit_from; class++)
    {
        j=0;
        mark = class_begins_at[classes_to_inherit_from[class]-1];
        class_prop_block = (uchar *) (properties_table + mark);

        while (class_prop_block[j]!=0)
        {   if (version_number == 3)
            {   prop_number = class_prop_block[j]%32;
                prop_length = 1 + class_prop_block[j++]/32;
            }
            else
            {   prop_number = class_prop_block[j]%64;
                prop_length = 1 + class_prop_block[j++]/64;
                if (prop_length > 2)
                    prop_length = class_prop_block[j++]%64;
            }

            /*  So we now have property number prop_number present in the
                property block for the class being read: its bytes are

                class_prop_block[j, ..., j + prop_length - 1]

                Question now is: is there already a value given in the
                current definition under this property name?                 */

            prop_in_current_defn = FALSE;

            kmax = full_object.l;

            for (k=0; k<kmax; k++)
                if (full_object.pp[k].num == prop_number)
                {   prop_in_current_defn = TRUE;

                    /*  (Note that the built-in "name" property is additive) */

                    if ((prop_number==1) || (prop_is_additive[prop_number]))
                    {
                        /*  The additive case: we accumulate the class
                            property values onto the end of the full_object
                            property                                         */

                        for (i=full_object.pp[k].l;
                             i<full_object.pp[k].l+prop_length/2; i++)
                        {   full_object.pp[k].ao[i].value = mark + j;
                            j += 2;
                            full_object.pp[k].ao[i].marker = INHERIT_MV;
                            full_object.pp[k].ao[i].type = LONG_CONSTANT_OT;
                        }
                        full_object.pp[k].l += prop_length/2;
                    }
                    else
                        /*  The ordinary case: the full_object property
                            values simply overrides the class definition,
                            so we skip over the values in the class table    */

                        j+=prop_length;

                    if (prop_number==3)
                    {   int y, z, class_block_offset;
                        uchar *p;

                        /*  Property 3 holds the address of the table of
                            instance variables, so this is the case where
                            the object already has instance variables in its
                            own table but must inherit some more from the
                            class  */

                        if (individuals_length+64 > MAX_INDIV_PROP_TABLE_SIZE)
                            memoryerror("MAX_INDIV_PROP_TABLE_SIZE",
                                        MAX_INDIV_PROP_TABLE_SIZE);

                        class_block_offset = class_prop_block[j-2]*256
                                             + class_prop_block[j-1];

                        p = individuals_table + class_block_offset;
                        z = class_block_offset;
                        while ((p[0]!=0)||(p[1]!=0))
                        {   if (module_switch)
                                backpatch_zmachine(IDENT_MV,
                                    INDIVIDUAL_PROP_ZA, i_m);
                            individuals_table[i_m++] = p[0];
                            individuals_table[i_m++] = p[1];
                            individuals_table[i_m++] = p[2];
                            for (y=0;y < p[2]/2;y++)
                            {   individuals_table[i_m++] = (z+3+y*2)/256;
                                individuals_table[i_m++] = (z+3+y*2)%256;
                                backpatch_zmachine(INHERIT_INDIV_MV,
                                    INDIVIDUAL_PROP_ZA, i_m-2);
                            }
                            z += p[2] + 3;
                            p += p[2] + 3;
                        }
                        individuals_length = i_m;
                    }

                    /*  For efficiency we exit the loop now (this property
                        number has been dealt with)                          */

                    break;
                }

            if (!prop_in_current_defn)
            {
                /*  The case where the class defined a property which wasn't
                    defined at all in full_object: we copy out the data into
                    a new property added to full_object                      */

                k=full_object.l++;
                full_object.pp[k].num = prop_number;
                full_object.pp[k].l = prop_length/2;
                for (i=0; i<prop_length/2; i++)
                {   full_object.pp[k].ao[i].value = mark + j;
                    j+=2;
                    full_object.pp[k].ao[i].marker = INHERIT_MV;
                    full_object.pp[k].ao[i].type = LONG_CONSTANT_OT;
                }

                if (prop_number==3)
                {   int y, z, class_block_offset;
                    uchar *p;

                    /*  Property 3 holds the address of the table of
                        instance variables, so this is the case where
                        the object had no instance variables of its own
                        but must inherit some more from the class  */

                    if (individuals_length+64 > MAX_INDIV_PROP_TABLE_SIZE)
                        memoryerror("MAX_INDIV_PROP_TABLE_SIZE",
                                    MAX_INDIV_PROP_TABLE_SIZE);

                    if (individual_prop_table_size++ == 0)
                    {   full_object.pp[k].num = 3;
                        full_object.pp[k].l = 1;
                        full_object.pp[k].ao[0].value
                            = individuals_length;
                        full_object.pp[k].ao[0].marker = INDIVPT_MV;
                        full_object.pp[k].ao[0].type = LONG_CONSTANT_OT;
                        i_m = individuals_length;
                    }
                    class_block_offset = class_prop_block[j-2]*256
                                         + class_prop_block[j-1];

                    p = individuals_table + class_block_offset;
                    z = class_block_offset;
                    while ((p[0]!=0)||(p[1]!=0))
                    {   if (module_switch)
                        backpatch_zmachine(IDENT_MV, INDIVIDUAL_PROP_ZA, i_m);
                        individuals_table[i_m++] = p[0];
                        individuals_table[i_m++] = p[1];
                        individuals_table[i_m++] = p[2];
                        for (y=0;y < p[2]/2;y++)
                        {   individuals_table[i_m++] = (z+3+y*2)/256;
                            individuals_table[i_m++] = (z+3+y*2)%256;
                            backpatch_zmachine(INHERIT_INDIV_MV,
                                INDIVIDUAL_PROP_ZA, i_m-2);
                        }
                        z += p[2] + 3;
                        p += p[2] + 3;
                    }
                    individuals_length = i_m;
                }
            }
        }
    }

    if (individual_prop_table_size > 0)
    {   individuals_table[i_m++] = 0;
        individuals_table[i_m++] = 0;
        individuals_length += 2;
    }
}

/* ------------------------------------------------------------------------- */
/*   Construction of Z-machine-format property blocks.                       */
/* ------------------------------------------------------------------------- */

static int write_properties_between(uchar *p, int mark, int from, int to)
{   int j, k, prop_number, prop_length;
    for (prop_number=to; prop_number>=from; prop_number--)
    {   for (j=0; j<full_object.l; j++)
        {   if ((full_object.pp[j].num == prop_number)
                && (full_object.pp[j].l != 100))
            {   prop_length = 2*full_object.pp[j].l;
                if (version_number == 3)
                    p[mark++] = prop_number + (prop_length - 1)*32;
                else
                {   switch(prop_length)
                    {   case 1:
                          p[mark++] = prop_number; break;
                        case 2:
                          p[mark++] = prop_number + 0x40; break;
                        default:
                          p[mark++] = prop_number + 0x80;
                          p[mark++] = prop_length + 0x80; break;
                    }
                }

                for (k=0; k<full_object.pp[j].l; k++)
                {   if (full_object.pp[j].ao[k].marker != 0)
                        backpatch_zmachine(full_object.pp[j].ao[k].marker,
                            PROP_ZA, mark);
                    p[mark++] = full_object.pp[j].ao[k].value/256;
                    p[mark++] = full_object.pp[j].ao[k].value%256;
                }
            }
        }
    }

    p[mark++]=0;
    return(mark);
}

static int write_property_block(char *shortname)
{
    /*  Compile the (now complete) full_object properties into a
        property-table block at "p" in Inform's memory.
        "shortname" is the object's short name, if specified; otherwise
        NULL.

        Return the number of bytes written to the block.                     */

    int32 mark = properties_table_size, i;
    uchar *p = (uchar *) properties_table;

    /* printf("Object at %04x\n", mark); */

    if (shortname != NULL)
    {   uchar *tmp = translate_text(p+mark+1,shortname);
        i = subtract_pointers(tmp,(p+mark+1));
        p[mark] = i/2;
        mark += i+1;
    }
    if (current_defn_is_class)
    {   mark = write_properties_between(p,mark,3,3);
        for (i=0;i<6;i++)
            p[mark++] = full_object.atts[i];
        class_begins_at[no_classes++] = mark;
    }

    mark = write_properties_between(p, mark, 1, (version_number==3)?31:63);

    i = mark - properties_table_size;
    properties_table_size = mark;

    return(i);
}

/* ------------------------------------------------------------------------- */
/*   The final stage in Nearby/Object/Class definition processing.           */
/* ------------------------------------------------------------------------- */

static void manufacture_object(void)
{   int i, j;

    segment_markers.enabled = FALSE;
    directives.enabled = TRUE;

    property_inheritance();

    objects[no_objects].parent = parent_of_this_obj;
    objects[no_objects].next = 0;
    objects[no_objects].child = 0;

    if ((parent_of_this_obj > 0) && (parent_of_this_obj != 0x7fff))
    {   i = objects[parent_of_this_obj-1].child;
        if (i == 0)
            objects[parent_of_this_obj-1].child = no_objects + 1;
        else
        {   while(objects[i-1].next != 0) i = objects[i-1].next;
            objects[i-1].next = no_objects+1;
        }
    }

        /*  The properties table consists simply of a sequence of property
            blocks, one for each object in order of definition, exactly as
            it will appear in the final Z-machine.                           */

    j = write_property_block(shortname_buffer);

    objects[no_objects].propsize = j;
    if (properties_table_size >= MAX_PROP_TABLE_SIZE)
        memoryerror("MAX_PROP_TABLE_SIZE",MAX_PROP_TABLE_SIZE);

    if (current_defn_is_class)
        for (i=0;i<6;i++) objects[no_objects].atts[i] = 0;
    else
        for (i=0;i<6;i++)
            objects[no_objects].atts[i] = full_object.atts[i];

    no_objects++;
}

/* ========================================================================= */
/*   [2]  The Object/Nearby/Class directives parser: translating the syntax  */
/*        into object specifications and then triggering off the above.      */
/* ========================================================================= */
/*   Properties ("with" or "private") segment.                               */
/* ------------------------------------------------------------------------- */

static int defined_this_segment[128], def_t_s;

static void properties_segment(int this_segment)
{
    /*  Parse through the "with" part of an object/class definition:

        <prop-1> <values...>, <prop-2> <values...>, ..., <prop-n> <values...>

        This routine also handles "private", with this_segment being equal
        to the token value for the introductory word ("private" or "with").  */


    int   i, property_name_symbol, property_number, next_prop, length,
          individual_property, this_identifier_number;

    do
    {   get_next_token();
        if ((token_type == SEGMENT_MARKER_TT)
            || (token_type == EOF_TT)
            || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
        {   put_token_back(); return;
        }

        if (token_type != SYMBOL_TT)
        {   ebf_error("property name", token_text);
            return;
        }

        individual_property = (stypes[token_value] != PROPERTY_T);

        if (individual_property)
        {   if (sflags[token_value] & UNKNOWN_SFLAG)
            {   this_identifier_number = no_individual_properties++;
                assign_symbol(token_value, this_identifier_number,
                    INDIVIDUAL_PROPERTY_T);
            }
            else
            {   if (stypes[token_value]==INDIVIDUAL_PROPERTY_T)
                    this_identifier_number = svals[token_value];
                else
                {   char already_error[128];
                    sprintf(already_error,
                        "\"%s\" is a name already in use (with type %s) \
and may not be used as a property name too",
                        token_text, typename(stypes[token_value]));
                    error(already_error);
                    return;
                }
            }

            defined_this_segment[def_t_s++] = token_value;

            if (individual_prop_table_size++ == 0)
            {   full_object.pp[full_object.l].num = 3;
                full_object.pp[full_object.l].l = 1;
                full_object.pp[full_object.l].ao[0].value
                    = individuals_length;
                full_object.pp[full_object.l].ao[0].type = LONG_CONSTANT_OT;
                full_object.pp[full_object.l].ao[0].marker = INDIVPT_MV;

                i_m = individuals_length;
                if (!current_defn_is_class)
                    property3_lists[no_objects] = i_m;
                full_object.l++;
            }
            individuals_table[i_m] = this_identifier_number/256;
            if (this_segment == PRIVATE_SEGMENT)
                individuals_table[i_m] |= 0x80;
            individuals_table[i_m+1] = this_identifier_number%256;
            if (module_switch)
                backpatch_zmachine(IDENT_MV, INDIVIDUAL_PROP_ZA, i_m);
            individuals_table[i_m+2] = 0;
        }
        else
        {   if (sflags[token_value] & UNKNOWN_SFLAG)
            {   error_named("No such property name as", token_text);
                return;
            }
            if (this_segment == PRIVATE_SEGMENT)
                error_named("Property should be declared in 'with', \
not 'private':", token_text);
            defined_this_segment[def_t_s++] = token_value;
            property_number = svals[token_value];

            next_prop=full_object.l++;
            full_object.pp[next_prop].num = property_number;
        }

        for (i=0; i<(def_t_s-1); i++)
            if (defined_this_segment[i] == token_value)
            {   error_named("Property given twice in the same declaration:",
                    (char *) symbs[token_value]);
            }
            else
            if (svals[defined_this_segment[i]] == svals[token_value])
            {   char error_b[128];
                sprintf(error_b,
                    "Property given twice in the same declaration, because \
the names '%s' and '%s' actually refer to the same property",
                    (char *) symbs[defined_this_segment[i]],
                    (char *) symbs[token_value]);
                error(error_b);
            }

        property_name_symbol = token_value;
        sflags[token_value] |= USED_SFLAG;

        length=0;
        do
        {   assembly_operand AO;
            get_next_token();
            if ((token_type == EOF_TT)
                || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                || ((token_type == SEP_TT) && (token_value == COMMA_SEP)))
                break;

            if (token_type == SEGMENT_MARKER_TT) { put_token_back(); break; }

            if ((token_type == SEP_TT) && (token_value == OPEN_SQUARE_SEP))
            {   char embedded_name[80];
                if (current_defn_is_class)
                {   sprintf(embedded_name,
                        "%s::%s", classname_text,
                        (char *) symbs[property_name_symbol]);
                }
                else
                {   sprintf(embedded_name,
                        "%s.%s", objectname_text,
                        (char *) symbs[property_name_symbol]);
                }
                AO.value = parse_routine(NULL, TRUE, embedded_name, FALSE);
                AO.type = LONG_CONSTANT_OT;
                AO.marker = IROUTINE_MV;

                directives.enabled = FALSE;
                segment_markers.enabled = TRUE;

                statements.enabled = FALSE;
                misc_keywords.enabled = FALSE;
                local_variables.enabled = FALSE;
                system_functions.enabled = FALSE;
                conditions.enabled = FALSE;
            }
            else

            /*  A special rule applies to values in double-quotes of the
                built-in property "name", which always has number 1: such
                property values are dictionary entries and not static
                strings                                                      */

            if ((!individual_property) &&
                (property_number==1) && (token_type == DQ_TT))
            {   AO.value = dictionary_add(token_text, 0x80, 0, 0);
                AO.type = LONG_CONSTANT_OT;
                AO.marker = DWORD_MV;
            }
            else
            {   if (length!=0)
                {
                    if ((token_type == SYMBOL_TT)
                        && (stypes[token_value]==PROPERTY_T))
                    {
                        /*  This is not necessarily an error: it's possible
                            to imagine a property whose value is a list
                            of other properties to look up, but far more
                            likely that a comma has been omitted in between
                            two property blocks                              */

                        warning_named(
               "Missing ','? Property data seems to contain the property name",
                            token_text);
                    }
                }

                /*  An ordinary value, then:                                 */

                put_token_back();
                AO = parse_expression(ARRAY_CONTEXT);
            }

            if (length == 64)
            {   error_named("Limit (of 32 values) exceeded for property",
                    (char *) symbs[property_name_symbol]);
                break;
            }

            if (individual_property)
            {   if (AO.marker != 0)
                    backpatch_zmachine(AO.marker, INDIVIDUAL_PROP_ZA,
                        i_m+3+length);
                individuals_table[i_m+3+length++] = AO.value/256;
                individuals_table[i_m+3+length++] = AO.value%256;
            }
            else
            {   full_object.pp[next_prop].ao[length++] = AO;
            }

        } while (TRUE);

        /*  People rarely do, but it is legal to declare a property without
            a value at all:

                with  name "fish", number, time_left;

            in which case the properties "number" and "time_left" are
            created as in effect variables and initialised to zero.          */

        if (length == 0)
        {   if (individual_property)
            {   individuals_table[i_m+3+length++] = 0;
                individuals_table[i_m+3+length++] = 0;
            }
            else
            {   full_object.pp[next_prop].ao[length].value = 0;
                full_object.pp[next_prop].ao[length].type  = LONG_CONSTANT_OT;
                full_object.pp[next_prop].ao[length++].marker = 0;
            }
        }

        if ((version_number==3) && (!individual_property))
        {   if (length > 4)
            {
       warning_named("Standard-game limit of 4 values per property exceeded \
(use Advanced to get 32), so truncating property",
                    (char *) symbs[property_name_symbol]);
                full_object.pp[next_prop].l=4;
            }
        }

        if (individual_property)
        {   individuals_table[i_m + 2] = length;
            individuals_length += length+3;
            i_m = individuals_length;
            if (individuals_length+64 > MAX_INDIV_PROP_TABLE_SIZE)
                memoryerror("MAX_INDIV_PROP_TABLE_SIZE",
                    MAX_INDIV_PROP_TABLE_SIZE);
        }
        else
            full_object.pp[next_prop].l = length;

        if ((token_type == EOF_TT)
            || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
        {   put_token_back(); return;
        }

    } while (TRUE);
}

/* ------------------------------------------------------------------------- */
/*   Attributes ("has") segment.                                             */
/* ------------------------------------------------------------------------- */

static void attributes_segment(void)
{
    /*  Parse through the "has" part of an object/class definition:

        [~]<attribute-1> [~]<attribute-2> ... [~]<attribute-n>               */

    int attribute_number, truth_state;
    do
    {   truth_state = TRUE;

        ParseAttrN:

        get_next_token();
        if ((token_type == SEGMENT_MARKER_TT)
            || (token_type == EOF_TT)
            || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
        {   if (!truth_state)
                ebf_error("attribute name after '~'", token_text);
            put_token_back(); return;
        }
        if ((token_type == SEP_TT) && (token_value == COMMA_SEP)) return;

        if ((token_type == SEP_TT) && (token_value == ARTNOT_SEP))
        {   truth_state = !truth_state; goto ParseAttrN;
        }

        if ((token_type != SYMBOL_TT)
            || (stypes[token_value] != ATTRIBUTE_T))
        {   ebf_error("name of an already-declared attribute", token_text);
            return;
        }

        attribute_number = svals[token_value];
        sflags[token_value] |= USED_SFLAG;

        if (truth_state)
            full_object.atts[attribute_number/8]
                |= (1 << (7-attribute_number%8));       /* Set attribute bit */
        else
            full_object.atts[attribute_number/8]
                &= ~(1 << (7-attribute_number%8));    /* Clear attribute bit */
    } while (TRUE);
}

/* ------------------------------------------------------------------------- */
/*   Classes ("class") segment.                                              */
/* ------------------------------------------------------------------------- */

static void add_class_to_inheritance_list(int class_number)
{
    int i;

    /*  The class number is actually the class's object number, which needs
        to be translated into its actual class number:                       */

    for (i=0;i<no_classes;i++)
        if (class_number == class_object_numbers[i])
        {   class_number = i+1;
            break;
        }

    /*  Remember the inheritance list so that property inheritance can
        be sorted out later on, when the definition has been finished:       */

    classes_to_inherit_from[no_classes_to_inherit_from++] = class_number;

    /*  Inheriting attributes from the class at once:                        */

    for (i=0; i<6; i++)
        full_object.atts[i]
            |= properties_table[class_begins_at[class_number-1] - 6 + i];
}

static void classes_segment(void)
{
    /*  Parse through the "class" part of an object/class definition:

        <class-1> ... <class-n>                                              */

    do
    {   get_next_token();
        if ((token_type == SEGMENT_MARKER_TT)
            || (token_type == EOF_TT)
            || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
        {   put_token_back(); return;
        }
        if ((token_type == SEP_TT) && (token_value == COMMA_SEP)) return;

        if ((token_type != SYMBOL_TT)
            || (stypes[token_value] != CLASS_T))
        {   ebf_error("name of an already-declared class", token_text);
            return;
        }

        sflags[token_value] |= USED_SFLAG;
        add_class_to_inheritance_list(svals[token_value]);
    } while (TRUE);
}

/* ------------------------------------------------------------------------- */
/*   Parse the body of a Nearby/Object/Class definition.                     */
/* ------------------------------------------------------------------------- */

static void parse_body_of_definition(void)
{   int commas_in_row;

    def_t_s = 0;

    do
    {   commas_in_row = -1;
        do
        {   get_next_token(); commas_in_row++;
        } while ((token_type == SEP_TT) && (token_value == COMMA_SEP));

        if (commas_in_row>1)
            error("Two commas ',' in a row in object/class definition");

        if ((token_type == EOF_TT)
            || ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)))
        {   if (commas_in_row > 0)
                error("Object/class definition finishes with ','");
            break;
        }

        if (token_type != SEGMENT_MARKER_TT)
        {   error_named("Expected 'with', 'has' or 'class' in \
object/class definition but found", token_text);
            break;
        }
        else
        switch(token_value)
        {   case WITH_SEGMENT:
                properties_segment(WITH_SEGMENT);
                break;
            case PRIVATE_SEGMENT:
                properties_segment(PRIVATE_SEGMENT);
                break;
            case HAS_SEGMENT:
                attributes_segment();
                break;
            case CLASS_SEGMENT:
                classes_segment();
                break;
        }

    } while (TRUE);

}

/* ------------------------------------------------------------------------- */
/*   Class directives:                                                       */
/*                                                                           */
/*        Class <name>  <body of definition>                                 */
/* ------------------------------------------------------------------------- */

static void initialise_full_object(void)
{   full_object.l = 0;
    full_object.atts[0] = 0;
    full_object.atts[1] = 0;
    full_object.atts[2] = 0;
    full_object.atts[3] = 0;
    full_object.atts[4] = 0;
    full_object.atts[5] = 0;
    property3_lists[no_objects] = -1;
}

extern void make_class(char * metaclass_name)
{   int n, duplicates_to_make = 0, class_number = no_objects+1,
        metaclass_flag = (metaclass_name != NULL);
    char duplicate_name[128]; dbgl start_dbgl = token_line_ref;

    current_defn_is_class = TRUE; no_classes_to_inherit_from = 0;
    individual_prop_table_size = 0;

    if (no_classes==MAX_CLASSES)
        memoryerror("MAX_CLASSES", MAX_CLASSES);

    if (no_classes==VENEER_CONSTRAINT_ON_CLASSES)
        fatalerror("Inform's maximum possible number of classes (whatever \
amount of memory is allocated) has been reached. If this causes serious \
inconvenience, please contact the author.");

    directives.enabled = FALSE;

    if (metaclass_flag)
    {   token_text = metaclass_name;
        token_value = symbol_index(token_text, -1);
        token_type = SYMBOL_TT;
    }
    else
    {   get_next_token();
        if ((token_type != SYMBOL_TT)
            || (!(sflags[token_value] & UNKNOWN_SFLAG)))
        {   ebf_error("new class name", token_text);
            panic_mode_error_recovery();
            return;
        }
    }

    /*  Each class also creates a modest object representing itself:         */

    strcpy(shortname_buffer, token_text);

    assign_symbol(token_value, class_number, CLASS_T);
    classname_text = (char *) symbs[token_value];

    if (metaclass_flag) sflags[token_value] |= SYSTEM_SFLAG;

    /*  "Class" (object 1) has no parent, whereas all other classes are
        the children of "Class".  Since "Class" is not present in a module,
        a special value is used which is corrected to 1 by the linker.       */

    if (metaclass_flag) parent_of_this_obj = 0;
    else parent_of_this_obj = (module_switch)?0x7fff:1;

    class_object_numbers[no_classes] = class_number;

    initialise_full_object();

    /*  Give the class the (nameless in Inform syntax) "inheritance" property
        with value its own class number.  (This therefore accumulates onto
        the inheritance property of any object inheriting from the class,
        since property 2 is always set to "additive" -- see below)           */

    full_object.l = 1;
    full_object.pp[0].num = 2;
    full_object.pp[0].l = 1;
    full_object.pp[0].ao[0].value  = no_objects + 1;
    full_object.pp[0].ao[0].type   = LONG_CONSTANT_OT;
    full_object.pp[0].ao[0].marker = OBJECT_MV;

    if (!metaclass_flag)
    {   get_next_token();
        if ((token_type == SEP_TT) && (token_value == OPENB_SEP))
        {   assembly_operand AO;
            AO = parse_expression(CONSTANT_CONTEXT);
            if (module_switch && (AO.marker != 0))
            {   error("Duplicate-number not known at compile time");
                n=0;
            }
            else
                n = AO.value;
            if ((n<0) || (n>10000))
            {   error("The number of duplicates must be 0 to 10000");
                n=0;
            }

            /*  Make one extra duplicate, since the veneer routines need
                always to keep an undamaged prototype for the class in stock */

            duplicates_to_make = n + 1;

            match_close_bracket();
        } else put_token_back();

        /*  Parse the body of the definition:                                */

        parse_body_of_definition();
    }

    if (debugfile_switch)
    {   write_debug_byte(CLASS_DBR);
        write_debug_string(shortname_buffer);
        write_dbgl(start_dbgl);
        write_dbgl(token_line_ref);
    }

    manufacture_object();

    if (individual_prop_table_size >= VENEER_CONSTRAINT_ON_IP_TABLE_SIZE)
        error("This class is too complex: it now carries too many properties. \
You may be able to get round this by declaring some of its property names as \
\"common properties\" using the 'Property' directive.");

    if (duplicates_to_make > 0)
    {   sprintf(duplicate_name, "%s_1", shortname_buffer);
        for (n=1; (duplicates_to_make--) > 0; n++)
        {   if (n>1)
            {   int i = strlen(duplicate_name);
                while (duplicate_name[i] != '_') i--;
                sprintf(duplicate_name+i+1, "%d", n);
            }
            make_object(FALSE, duplicate_name, class_number, class_number, -1);
        }
    }
}

/* ------------------------------------------------------------------------- */
/*   Object/Nearby directives:                                               */
/*                                                                           */
/*       Object  <name-1> ... <name-n> "short name"  [parent]  <body of def> */
/*                                                                           */
/*       Nearby  <name-1> ... <name-n> "short name"  <body of definition>    */
/* ------------------------------------------------------------------------- */

static int end_of_header(void)
{   if (((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
        || ((token_type == SEP_TT) && (token_value == COMMA_SEP))
        || (token_type == SEGMENT_MARKER_TT)) return TRUE;
    return FALSE;
}

extern void make_object(int nearby_flag,
    char *textual_name, int specified_parent, int specified_class,
    int instance_of)
{
    /*  Ordinarily this is called with nearby_flag TRUE for "Nearby",
        FALSE for "Object"; and textual_name NULL, specified_parent and
        specified_class both -1.  The next three arguments are used when
        the routine is called for class duplicates manufacture (see above).
        The last is used to create instances of a particular class.  */

    int i, tree_depth, internal_name_symbol = 0;
    char internal_name[64];
    dbgl start_dbgl = token_line_ref;

    directives.enabled = FALSE;

    if (no_objects==MAX_OBJECTS) memoryerror("MAX_OBJECTS", MAX_OBJECTS);

    sprintf(internal_name, "nameless_obj__%d", no_objects+1);
    objectname_text = internal_name;

    current_defn_is_class = FALSE;

    no_classes_to_inherit_from=0;

    individual_prop_table_size = 0;

    if (nearby_flag) tree_depth=1; else tree_depth=0;

    if (specified_class != -1) goto HeaderPassed;

    get_next_token();

    /*  Read past and count a sequence of "->"s, if any are present          */

    if ((token_type == SEP_TT) && (token_value == ARROW_SEP))
    {   if (nearby_flag)
          error("The syntax '->' is only used as an alternative to 'Nearby'");

        while ((token_type == SEP_TT) && (token_value == ARROW_SEP))
        {   tree_depth++;
            get_next_token();
        }
    }

    sprintf(shortname_buffer, "?");

    segment_markers.enabled = TRUE;

    /*  This first word is either an internal name, or a textual short name,
        or the end of the header part                                        */

    if (end_of_header()) goto HeaderPassed;

    if (token_type == DQ_TT) textual_name = token_text;
    else
    {   if ((token_type != SYMBOL_TT)
            || (!(sflags[token_value] & UNKNOWN_SFLAG)))
            ebf_error("name for new object or its textual short name",
                token_text);
        else
        {   internal_name_symbol = token_value;
            strcpy(internal_name, token_text);
        }
    }

    /*  The next word is either a parent object, or
        a textual short name, or the end of the header part                  */

    get_next_token();
    if (end_of_header()) goto HeaderPassed;

    if (token_type == DQ_TT)
    {   if (textual_name != NULL)
            error("Two textual short names given for only one object");
        else
            textual_name = token_text;
    }
    else
    {   if ((token_type != SYMBOL_TT)
            || (sflags[token_value] & UNKNOWN_SFLAG))
        {   if (textual_name == NULL)
                ebf_error("parent object or the object's textual short name",
                    token_text);
            else
                ebf_error("parent object", token_text);
        }
        else goto SpecParent;
    }

    /*  Finally, it's possible that there is still a parent object           */

    get_next_token();
    if (end_of_header()) goto HeaderPassed;

    if (specified_parent != -1)
        ebf_error("body of object definition", token_text);
    else
    {   SpecParent:
        if ((stypes[token_value] == OBJECT_T)
            || (stypes[token_value] == CLASS_T))
        {   specified_parent = svals[token_value];
            sflags[token_value] |= USED_SFLAG;
        }
        else ebf_error("name of (the parent) object", token_text);
    }

    /*  Now it really has to be the body of the definition.                  */

    get_next_token();
    if (end_of_header()) goto HeaderPassed;

    ebf_error("body of object definition", token_text);

    HeaderPassed:
    if (specified_class == -1) put_token_back();

    if (internal_name_symbol > 0)
        assign_symbol(internal_name_symbol, no_objects + 1, OBJECT_T);

    if (listobjects_switch)
        printf("%3d \"%s\"\n", no_objects+1,
            (textual_name==NULL)?"(with no short name)":textual_name);
    if (textual_name == NULL)
    {   if (internal_name_symbol > 0)
            sprintf(shortname_buffer, "(%s)",
                (char *) symbs[internal_name_symbol]);
        else
            sprintf(shortname_buffer, "(%d)", no_objects+1);
    }
    else strcpy(shortname_buffer, textual_name);

    if (specified_parent != -1)
    {   if (tree_depth > 0)
            error("Use of '->' (or 'Nearby') clashes with giving a parent");
        parent_of_this_obj = specified_parent;
    }
    else
    {   parent_of_this_obj = 0;
        if (tree_depth>0)
        {
            /*  We have to set the parent object to the most recently defined
                object at level (tree_depth - 1) in the tree.

                A complication is that objects are numbered 1, 2, ... in the
                Z-machine (and in the objects[].parent, etc., fields) but
                0, 1, 2, ... internally (and as indices to object[]).        */

            for (i=no_objects-1; i>=0; i--)
            {   int j = i, k = 0;

                /*  Metaclass or class objects cannot be '->' parents:  */
                if (((!module_switch) && (i<4)) || (objects[i].parent == 1))
                    continue;

                while (objects[j].parent != 0)
                {   j = objects[j].parent - 1; k++; }

                if (k == tree_depth - 1)
                {   parent_of_this_obj = i+1;
                    break;
                }
            }
            if (parent_of_this_obj == 0)
            {   if (tree_depth == 1)
    error("'->' (or 'Nearby') fails because there is no previous object");
                else
    error("'-> -> ...' fails because no previous object is deep enough");
            }
        }
    }

    initialise_full_object();
    if (instance_of != -1) add_class_to_inheritance_list(instance_of);

    if (specified_class == -1) parse_body_of_definition();
    else add_class_to_inheritance_list(specified_class);

    if (debugfile_switch)
    {   write_debug_byte(OBJECT_DBR);
        write_debug_byte((no_objects+1)/256);
        write_debug_byte((no_objects+1)%256);
        write_debug_string(internal_name);
        write_dbgl(start_dbgl);
        write_dbgl(token_line_ref);
    }

    manufacture_object();
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_objects_vars(void)
{
    properties_table = NULL;

    objects = NULL;
    classes_to_inherit_from = NULL;
    class_begins_at = NULL;
}

extern void objects_begin_pass(void)
{
    properties_table_size=0;
    prop_is_long[1] = TRUE; prop_is_additive[1] = TRUE;            /* "name" */
    prop_is_long[2] = TRUE; prop_is_additive[2] = TRUE;  /* inheritance prop */
    prop_is_long[3] = TRUE; prop_is_additive[3] = FALSE;
                                         /* instance variables table address */
    no_properties = 4;

    no_attributes = 0;

    no_objects = 0;
    objects[0].parent = 0; objects[0].child = 0; objects[0].next = 0;
    no_classes = 0;

    no_embedded_routines = 0;

    no_individual_properties=72;
    individuals_length=0;
}

extern void objects_allocate_arrays(void)
{
    objects               = my_calloc(sizeof(objectt), MAX_OBJECTS, "objects");
    classes_to_inherit_from = my_calloc(sizeof(int), MAX_CLASSES,
                                "inherited classes list");
    class_begins_at       = my_calloc(sizeof(int32), MAX_CLASSES,
                                "pointers to classes");
    class_object_numbers  = my_calloc(sizeof(int),     MAX_CLASSES,
                                "class object numbers");

    properties_table      = my_malloc(MAX_PROP_TABLE_SIZE,"properties table");
    individuals_table     = my_malloc(MAX_INDIV_PROP_TABLE_SIZE,
                                "individual properties table");
    property3_lists       = my_calloc(sizeof(int), MAX_OBJECTS,
                                "property 3 lists");
}

extern void objects_free_arrays(void)
{
    my_free(&objects,          "objects");
    my_free(&class_object_numbers,"class object numbers");
    my_free(&classes_to_inherit_from, "inherited classes list");
    my_free(&class_begins_at,  "pointers to classes");

    my_free(&properties_table, "properties table");
    my_free(&property3_lists,  "property 3 lists");
    my_free(&individuals_table,"individual properties table");

}

/* ========================================================================= */
