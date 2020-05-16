/* ------------------------------------------------------------------------- */
/*   "veneer" : Compiling the run-time "veneer" of any routines invoked      */
/*              by the compiler (e.g. DefArt) which the program doesn't      */
/*              provide                                                      */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

int veneer_mode;                      /*  Is the code currently being
                                          compiled from the veneer?          */

extern void compile_initial_routine(void)
{
    /*  The first routine present in memory in any Inform game, beginning
        at the code area start position, always has 0 local variables
        (since the interpreter begins execution with an empty stack frame):
        and it must "quit" rather than "return".

        In order not to impose these restrictions on "Main", we compile a
        trivial routine consisting of a call to "Main" followed by "quit".   */

    int32 j;
    assembly_operand AO, AO2; dbgl null_dbgl;
    null_dbgl.b1 = 0; null_dbgl.b2 = 0; null_dbgl.b3 = 0; null_dbgl.cc = 0;

    assign_symbol(j = symbol_index("Main__", -1),
        assemble_routine_header(0, FALSE, "Main__", &null_dbgl),
        ROUTINE_T);
    sflags[j] |= SYSTEM_SFLAG + USED_SFLAG;

    AO.value = 0; AO.type = LONG_CONSTANT_OT; AO.marker = MAIN_MV;
    AO2.value = 255; AO2.type = VARIABLE_OT; AO2.marker = 0;

    sequence_point_follows = FALSE;

    if (version_number > 3)
        assemble_1_to(call_vs_zc, AO, AO2);
    else
        assemble_1_to(call_zc, AO, AO2);

    assemble_0(quit_zc);
    assemble_routine_end(FALSE, &null_dbgl);
}

/* ------------------------------------------------------------------------- */
/*   The rest of the veneer is applied at the end of the pass, as required.  */
/* ------------------------------------------------------------------------- */

static int   veneer_routine_needs_compilation[VENEER_ROUTINES];
int32 veneer_routine_address[VENEER_ROUTINES];

typedef struct VeneerRoutine_s
{   char *name;
    char *source1;
    char *source2;
    char *source3;
    char *source4;
    char *source5;
    char *source6;
} VeneerRoutine;

static char *veneer_source_area;

static VeneerRoutine VRs[VENEER_ROUTINES] =
{
    /*  Box__Routine:  the only veneer routine used in the implementation of
                       an actual statement ("box", of course), written in a
                       hybrid of Inform and assembly language.  Note the
                       transcription of the box text to the transcript
                       output stream (-1, or $ffff).                         */

    {   "Box__Routine",
        "maxw table n w w2 line lc t;\
         n = table --> 0;\
         @add n 6 -> sp;\
         @split_window sp;\
         @set_window 1;\
         w = 0 -> 33;\
         if (w == 0) w=80;\
         w2 = (w - maxw)/2;\
         style reverse;\
         @sub w2 2 -> w;\
         line = 5;\
         lc = 1;\
         @set_cursor 4 w;\
         spaces maxw + 4;",
        "do\
         {   @set_cursor line w;\
             spaces maxw + 4;\
             @set_cursor line w2;\
             t = table --> lc;\
             if (t~=0) print (string) t;\
             line++; lc++;\
         } until (lc > n);\
         @set_cursor line w;\
         spaces maxw + 4;\
         @buffer_mode 1;\
         style roman;\
         @set_window 0;\
         @split_window 1;\
         @output_stream $ffff;\
         print \"[ \";\
         lc = 1;",
        "do\
         {   w = table --> lc;\
             if (w ~= 0) print (string) w;\
             lc++;\
             if (lc > n)\
             {   print \"]^^\";\
                 break;\
             }\
             print \"^  \";\
         } until (false);\
         @output_stream 1;\
         ]", "", "", ""
    },

    /*  This batch of routines is expected to be defined (rather better) by
        the Inform library: these minimal forms here are provided to prevent
        tiny non-library-using programs from failing to compile when certain
        legal syntaxes (such as <<Action a b>>;) are used.                   */

    {   "R_Process",
        "a b c; print \"Action <\", a, \" \", b, \" \", c, \">^\";\
         ]", "", "", "", "", ""
    },
    {   "DefArt",
        "obj; print \"the \", obj; ]", "", "", "", "", ""
    },
    {   "InDefArt",
        "obj; print \"a \", obj; ]", "", "", "", "", ""
    },
    {   "CDefArt",
        "obj; print \"The \", obj; ]", "", "", "", "", ""
    },
    {   "PrintShortName",
        "obj; @print_obj obj; ]", "", "", "", "", ""
    },
    {   "EnglishNumber",
        "obj; print obj; ]", "", "", "", "", ""
    },
    {   "Print__PName",
        "prop p size cla i;\
         if (prop & $c000)\
         {   cla = #classes_table-->(prop & $ff);\
             print (name) cla, \"::\";\
             if ((prop & $8000) == 0) prop = (prop & $3f00)/$100;\
             else\
             {   prop = (prop & $7f00)/$100;\
                 i = cla.3;\
                 while ((i-->0 ~= 0) && (prop>0))\
                 {   i = i + i->2 + 3;\
                     prop--;\
                 }\
                 prop = (i-->0) & $7fff;\
             }\
         }",
        "p = #identifiers_table;\
         size = p-->0;\
         if (prop<=0 || prop>=size || p-->prop==0)\
             print \"<number \", prop, \">\";\
         else print (string) p-->prop;\
         ]", "", "", "", ""
    },

    /*  The remaining routines make up the run-time half of the object
        orientation system, and need never be present for Inform 5 programs. */

    {
        /*  WV__Pr:  write a value to the property for the given
                     object having the given identifier                      */

        "WV__Pr",
        "obj identifier value x;\
         x = obj..&identifier;\
         if (x==0) { RT__Err(\"write to\", obj, identifier); return; }\
         x-->0 = value;\
         ]", "", "", "", "", ""
    },
    {
        /*  RV__Pr:  read a value from the property for the given
                     object having the given identifier                      */

        "RV__Pr",
        "obj identifier x;\
         x = obj..&identifier;\
         if (x==0)\
         {   if (identifier >= 1 && identifier < 64)\
                 return obj.identifier;\
             RT__Err(\"read\", obj, identifier); return; }\
         return x-->0;\
         ]", "", "", "", "", ""
    },
    {   /*  CA__Pr:  call, that is, print-or-run-or-read, a property:
                     this exactly implements obj..prop(...).  Note that
                     classes (members of Class) have 5 built-in properties
                     inherited from Class: create, recreate, destroy,
                     remaining and copy.  Implementing these here prevents
                     the need for a full metaclass inheritance scheme.      */

        "CA__Pr",
        "obj id a b c d e f x y z s s2 n m;\
         if (obj < 1 || obj > #largest_object-255)\
         {   switch(Z__Region(obj))\
             { 2: if (id == call)\
                   { s = sender; sender = self; self = obj;\
                     #ifdef action;sw__var=action;#endif;\
                     x = indirect(obj, a, b, c, d, e, f);\
                     self = sender; sender = s; return x; }\
                   jump Call__Error;",
              "3: if (id == print) { @print_paddr obj; rtrue; }\
                   if (id == print_to_array)\
                   { @output_stream 3 a; @print_paddr obj; @output_stream -3;\
                     return a-->0; }\
                   jump Call__Error;\
             }\
             jump Call__Error;\
         }\
         @check_arg_count 3 ?~A__x;y++;@check_arg_count 4 ?~A__x;y++;\
         @check_arg_count 5 ?~A__x;y++;@check_arg_count 6 ?~A__x;y++;\
         @check_arg_count 7 ?~A__x;y++;@check_arg_count 8 ?~A__x;y++;.A__x;",
        "#ifdef DEBUG;#ifdef InformLibrary;\
         if (debug_flag & 1 ~= 0)\
         { debug_flag--;\
           print \"[ ~\", (name) obj, \"~.\", (property) id, \"(\";\
     switch(y) { 1: print a; 2: print a,\",\",b; 3: print a,\",\",b,\",\",c;\
     4: print a,\",\",b,\",\",c,\",\",d;\
     5: print a,\",\",b,\",\",c,\",\",d,\",\",e;\
     6: print a,\",\",b,\",\",c,\",\",d,\",\",e,\",\",f; }\
           print \") ]^\"; debug_flag++;\
         }\
         #endif;#endif;",
        "if (id >= 0 && id < 64)\
         { x = obj.&id; if (x==0) n=2; else n = obj.#id; }\
         else\
         { if (id>=64 && id<69 && obj in Class)\
             return Cl__Ms(obj,id,y,a,b,c,d);\
           x = obj..&id;\
           if (x == 0) { .Call__Error;\
             RT__Err(\"send message\", obj, id); return; }\
           n = 0->(x-1);\
           if (id&$C000==$4000)\
             switch (n&$C0) { 0: n=1; $40: n=2; $80: n=n&$3F; }\
         }",
        "for (:2*m<n:m++)\
         {  if (x-->m==$ffff) rfalse;\
            switch(Z__Region(x-->m))\
            { 2: s = sender; sender = self; self = obj; s2 = sw__var;\
               #ifdef LibSerial;\
               if (id==life) sw__var=reason_code; else sw__var=action;\
               #endif;\
     switch(y) { 0: z = indirect(x-->m); 1: z = indirect(x-->m, a);\
     2: z = indirect(x-->m, a, b); 3: z = indirect(x-->m, a, b, c);",
    "4: z = indirect(x-->m, a, b, c, d); 5:z = indirect(x-->m, a, b, c, d, e);\
     6: z = indirect(x-->m, a, b, c, d, e, f); }\
                 self = sender; sender = s; sw__var = s2;\
                 if (z ~= 0) return z;\
              3: print_ret (string) x-->m;\
        default: return x-->m;\
            }\
         }\
         rfalse;\
         ]"
    },
    {
        /*  IB__Pr:  ++(individual property)                                 */

        "IB__Pr",
        "obj identifier x;\
         x = obj..&identifier;\
         if (x==0) { RT__Err(\"increment\", obj, identifier); return; }\
         return ++(x-->0);\
         ]", "", "", "", "", ""
    },
    {
        /*  IA__Pr:  (individual property)++                                 */

        "IA__Pr",
        "obj identifier x;\
         x = obj..&identifier;\
         if (x==0) { RT__Err(\"increment\", obj, identifier); return; }\
         return (x-->0)++;\
         ]", "", "", "", "", ""
    },
    {
        /*  DB__Pr:  --(individual property)                                 */

        "DB__Pr",
        "obj identifier x;\
         x = obj..&identifier;\
         if (x==0) { RT__Err(\"decrement\", obj, identifier); return; }\
         return --(x-->0);\
         ]", "", "", "", "", ""
    },
    {
        /*  DA__Pr:  (individual property)--                                 */

        "DA__Pr",
        "obj identifier x;\
         x = obj..&identifier;\
         if (x==0) { RT__Err(\"decrement\", obj, identifier); return; }\
         return (x-->0)--;\
         ]", "", "", "", "", ""
    },
    {
        /*  RA__Pr:  read the address of a property value for a given object,
                     returning 0 if it doesn't provide this individual
                     property                                                */

        "RA__Pr",
        "obj identifier i otherid cla;\
         if (identifier<64 && identifier>0) return obj.&identifier;\
         if (identifier & $8000 ~= 0)\
         {   cla = #classes_table-->(identifier & $ff);\
             if (cla.&3 == 0) rfalse;\
             if (~~(obj ofclass cla)) rfalse;\
             identifier = (identifier & $7f00) / $100;\
             i = cla.3;\
             while (identifier>0)\
             {   identifier--;\
                 i = i + i->2 + 3;\
             }\
             return i+3;\
         }",
        "if (identifier & $4000 ~= 0)\
         {   cla = #classes_table-->(identifier & $ff);\
             identifier = (identifier & $3f00) / $100;\
             if (~~(obj ofclass cla)) rfalse; i=0-->5;\
             if (cla == 2) return i+2*identifier-2;\
             i = 0-->((i+124+cla*14)/2);\
             i = CP__Tab(i + 2*(0->i) + 1, -1)+6;\
             return CP__Tab(i, identifier);\
         }\
         if (obj.&3 == 0) rfalse;\
         if (obj in 1)\
         {   if (identifier<64 || identifier>=72) rfalse;\
         }",
        "if (self == obj)\
             otherid = identifier | $8000;\
         i = obj.3;\
         while (i-->0 ~= 0)\
         {   if (i-->0 == identifier or otherid)\
                 return i+3;\
             i = i + i->2 + 3;\
         }\
         rfalse;\
         ]", "", "", ""
    },
    {
        /*  RL__Pr:  read the property length of an individual property value,
                     returning 0 if it isn't provided by the given object    */

        "RL__Pr",
        "obj identifier x;\
         if (identifier<64 && identifier>0) return obj.#identifier;\
         x = obj..&identifier;\
         if (x==0) rfalse;\
         if (identifier&$C000==$4000)\
             switch (((x-1)->0)&$C0)\
             {  0: return 1;  $40: return 2;  $80: return ((x-1)->0)&$3F; }\
         return (x-1)->0;\
         ]", "", "", "", "", ""
    },
    {
        /*  RA__Sc:  implement the "superclass" (::) operator,
                     returning an identifier                                 */

        "RA__Sc",
        "cla identifier otherid i j k;\
         if (cla notin 1 && cla > 4)\
         {   RT__Err(\"be a '::' superclass\", cla, -1); rfalse; }\
         if (self ofclass cla) otherid = identifier | $8000;\
         for (j=0: #classes_table-->j ~= 0: j++)\
         {   if (cla==#classes_table-->j)\
             {   if (identifier < 64) return $4000 + identifier*$100 + j;\
                 if (cla.&3 == 0) break;\
                 i = cla.3;",
                "while (i-->0 ~= 0)\
                 {   if (i-->0 == identifier or otherid)\
                         return $8000 + k*$100 + j;\
                     i = i + i->2 + 3;\
                     k++;\
                 }\
                 break;\
             }\
         }\
         RT__Err(\"make use of\", cla, identifier);\
         rfalse;\
         ]", "", "", "", ""
    },
    {
        /*  OP__Pr:  test whether or not given object provides individual
                     property with the given identifier code                 */

        "OP__Pr",
        "obj identifier;\
         if (obj<1 || obj > (#largest_object-255))\
         {   if (identifier ~= print or print_to_array or call) rfalse;\
             switch(Z__Region(obj))\
             {   2: if (identifier == call) rtrue;\
                 3: if (identifier == print or print_to_array) rtrue;\
             }\
             rfalse;\
         }",
        "if (identifier<64)\
         {   if (obj.&identifier ~= 0) rtrue;\
             rfalse;\
         }\
         if (obj..&identifier ~= 0) rtrue;\
         if (identifier<72 && obj in 1) rtrue;\
         rfalse;\
         ]", "", "", "", ""
    },
    {
        /*  OC__Cl:  test whether or not given object is of the given class  */

        "OC__Cl",
        "obj cla j a n;\
         if (obj<1 || obj > (#largest_object-255))\
         {   if (cla ~= 3 or 4) rfalse;\
             if (Z__Region(obj) == cla-1) rtrue;\
             rfalse;\
         }\
         switch(cla)\
         {   1: if (obj<=4) rtrue;\
                if (obj in 1) rtrue;\
                rfalse;\
             2: if (obj<=4) rfalse;\
                if (obj in 1) rfalse;\
                rtrue;\
             3, 4: rfalse;\
         }",
        "if (cla notin 1) { RT__Err(\"apply 'ofclass' for\", cla, -1);rfalse;}\
         a = obj.&2;\
         if (a==0) rfalse;\
         n = obj.#2;\
         for (j=0: j<n/2: j++)\
         {   if (a-->j == cla) rtrue;\
         }\
         rfalse;\
         ]", "", "", "", ""
    },
    {   /*  Copy__Primitive:  routine to "deep copy" objects                 */

        "Copy__Primitive",
        "o1 o2 a1 a2 n m l size identifier;\
         for (n=0:n<48:n++)\
         {   if (o2 has n) give o1 n;\
             else give o1 ~n;\
         }\
         for (n=1:n<64:n++) if (n~=3)\
         {   a1 = o1.&n; a2 = o2.&n; size = o1.#n;\
             if (a1~=0 && a2~=0 && size==o2.#n)\
             {   for (m=0:m<size:m++) a1->m=a2->m;\
             }\
         }",
        "if (o1.&3 == 0 || o2.&3 == 0) return;\
         for (n=o2.3: n-->0 ~= 0: n = n + size + 3)\
         {   identifier = n-->0;\
             size = n->2;\
             for (m=o1.3: m-->0 ~= 0: m = m + m->2 + 3)\
                 if ((identifier & $7fff == (m-->0) & $7fff) && size==m->2)\
                     for (l=3: l<size+3: l++) m->l = n->l;\
         }\
         ]", "", "", "", ""
    },
    {   /*  RT__Err:  for run-time errors occurring in the above: e.g.,
                      an attempt to write to a non-existent individual
                      property                                               */

        "RT__Err",
        "crime obj id size p;\
         print \"^** Run-time error: \";\
         if (crime==1) { print \"class \"; @print_obj obj;\
         print_ret \": 'create' can have 0 to 3 parameters only **\";}",
        "if (obj in Class) print \"Class \";\
         @print_obj obj;\
         print \" (object number \", obj, \") \";\
         if (id<0)\
             print \"is not of class \", (name) -id;",
        "else\
         {   print \" has no property \", (property) id;\
             p = #identifiers_table;\
             size = p-->0;\
             if (id<0 || id>=size)\
                 print \" (and nor has any other object)\";\
         }\
         print \" to \", (string) crime, \" **^\";\
         ]", "", "", ""
    },
    {   /*  Z__Region:  Determines whether a value is:
                        1  an object number
                        2  a code address
                        3  a string address
                        0  none of the above                                 */

        "Z__Region",
        "addr;\
         if (addr==0) rfalse;\
         if (addr>=1 && addr<=(#largest_object-255)) rtrue;\
         if (Unsigned__Compare(addr, #strings_offset)>=0) return 3;\
         if (Unsigned__Compare(addr, #code_offset)>=0) return 2;\
         rfalse;\
         ]", "", "", "", "", ""
    },
    {   /*  Unsigned__Compare:  returns 1 if x>y, 0 if x=y, -1 if x<y        */

        "Unsigned__Compare",
        "x y u v;\
         if (x==y) return 0;\
         if (x<0 && y>=0) return 1;\
         if (x>=0 && y<0) return -1;\
         u = x&$7fff; v= y&$7fff;\
         if (u>v) return 1;\
         return -1;\
         ]", "", "", "", "", ""
    },
    {   /*  Meta__class:  returns the metaclass of an object                 */

        "Meta__class",
        "obj;\
         switch(Z__Region(obj))\
         {   2: return Routine;\
             3: return String;\
             1: if (obj in 1 || obj <= 4) return Class;\
                return Object;\
         }\
         rfalse;\
         ]", "", "", "", "", ""
    },
    {   /*  CP__Tab:  searches a common property table for the given
                      identifier, thus imitating the get_prop_addr opcode.
                      Returns 0 if not provided, except:
                      if the identifier supplied is -1, then returns
                      the address of the first byte after the table.         */

        "CP__Tab",
        "x id n l;\
         while ((n=0->x) ~= 0)\
         {   if (n & $80) { x++; l = (0->x) & $3f; }\
             else { if (n & $40) l=2; else l=1; }\
             x++;\
             if ((n & $3f) == id) return x;\
             x = x + l;\
         }\
         if (id<0) return x+1; rfalse; ]", "", "", "", "", ""
    },
    {   /*  Cl__Ms:   the five message-receiving properties of Classes       */

        "Cl__Ms",
        "obj id y a b c d x;\
         switch(id)\
         {   create:\
                 if (children(obj)<=1) rfalse; x=child(obj);\
                 remove x; if (x provides create) { if (y==0) x..create();\
                 if (y==1) x..create(a); if (y==2) x..create(a,b);\
                 if (y>3) RT__Err(1,obj); if (y>=3) x..create(a,b,c);}\
                 return x;\
             recreate:\
                 if (~~(a ofclass obj))\
                 { RT__Err(\"recreate\", a, -obj); rfalse; }\
                 Copy__Primitive(a, child(obj));\
                 if (a provides create) { if (y==1) a..create();\
                 if (y==2) a..create(b); if (y==3) a..create(b,c);\
                 if (y>4) RT__Err(1,obj); if (y>=4) a..create(b,c,d);\
                 } rfalse;",
            "destroy:\
                 if (~~(a ofclass obj))\
                 { RT__Err(\"destroy\", a, -obj); rfalse; }\
                 if (a provides destroy) a..destroy();\
                 Copy__Primitive(a, child(obj));\
                 move a to obj; rfalse;\
             remaining:\
                 return children(obj)-1;",
            "copy:\
                 if (~~(a ofclass obj))\
                 { RT__Err(\"copy\", a, -obj); rfalse; }\
                 if (~~(b ofclass obj))\
                 { RT__Err(\"copy\", b, -obj); rfalse; }\
                 Copy__Primitive(a, b); rfalse;\
         }\
         ]", "", "", ""
    }
};

extern assembly_operand veneer_routine(int code)
{   assembly_operand AO;

    AO.type = LONG_CONSTANT_OT;
    AO.marker = VROUTINE_MV;
    AO.value = code;
    veneer_routine_needs_compilation[code] = TRUE;

    if (code == CA__Pr_VR)
    {   veneer_routine_needs_compilation[Copy__Primitive_VR] = TRUE;
        veneer_routine_needs_compilation[Unsigned__Compare_VR] = TRUE;
        veneer_routine_needs_compilation[Z__Region_VR] = TRUE;
        veneer_routine_needs_compilation[Print__Pname_VR] = TRUE;
        veneer_routine_needs_compilation[Cl__Ms_VR] = TRUE;
        veneer_routine_needs_compilation[OP__Pr_VR] = TRUE;
    }

    if ((code == OC__Cl_VR) || (code == OP__Pr_VR) || (code == CA__Pr_VR)
        || (code == Metaclass_VR))
    {   veneer_routine_needs_compilation[Unsigned__Compare_VR] = TRUE;
        veneer_routine_needs_compilation[Z__Region_VR] = TRUE;
        veneer_routine_needs_compilation[CP__Tab_VR] = TRUE;
        veneer_routine_needs_compilation[RA__Pr_VR] = TRUE;
    }

    if (code >= WV__Pr_VR)
        veneer_routine_needs_compilation[RT__Err_VR] = TRUE;

    if (code == RT__Err_VR)
        veneer_routine_needs_compilation[Print__Pname_VR] = TRUE;

    if (code == RA__Pr_VR)
        veneer_routine_needs_compilation[CP__Tab_VR] = TRUE;

    return(AO);
}

extern void compile_veneer(void)
{   int i, j;

    if (module_switch) return;

    /*  Called at the end of the pass to insert as much of the veneer as is
        needed and not elsewhere compiled.  */

    /*  for (i=0; i<VENEER_ROUTINES; i++)   
        printf("%s %d %d %d %d %d %d\n", VRs[i].name,
            strlen(VRs[i].source1), strlen(VRs[i].source2),
            strlen(VRs[i].source3), strlen(VRs[i].source4),
            strlen(VRs[i].source5), strlen(VRs[i].source6)); */

    for (i=0; i<VENEER_ROUTINES; i++)
    {   
        if (veneer_routine_needs_compilation[i])
        {   j = symbol_index(VRs[i].name, -1);
            if (sflags[j] & UNKNOWN_SFLAG)
            {   veneer_mode = TRUE;
                strcpy(veneer_source_area, VRs[i].source1);
                strcat(veneer_source_area, VRs[i].source2);
                strcat(veneer_source_area, VRs[i].source3);
                strcat(veneer_source_area, VRs[i].source4);
                strcat(veneer_source_area, VRs[i].source5);
                strcat(veneer_source_area, VRs[i].source6);
                assign_symbol(j,
                  parse_routine(veneer_source_area, FALSE, VRs[i].name, TRUE),
                  ROUTINE_T);
                veneer_mode = FALSE;
            }
            else
            {   if (stypes[j] != ROUTINE_T)
                error_named("The following name is reserved by Inform for its \
own use as a routine name; you can use it as a routine name yourself (to \
override the standard definition) but cannot use it for anything else:",
                    VRs[i].name);
                else
                    sflags[j] |= USED_SFLAG;
            }
            veneer_routine_address[i] = svals[j];
        }
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_veneer_vars(void)
{
}

extern void veneer_begin_pass(void)
{   int i;
    veneer_mode = FALSE;
    for (i=0; i<VENEER_ROUTINES; i++)
    {   veneer_routine_needs_compilation[i] = FALSE;
        veneer_routine_address[i] = 0;
    }
}

extern void veneer_allocate_arrays(void)
{   veneer_source_area = my_malloc(3072, "veneer source code area");
}

extern void veneer_free_arrays(void)
{   my_free(&veneer_source_area, "veneer source code area");
}

/* ========================================================================= */
