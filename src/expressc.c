/* ------------------------------------------------------------------------- */
/*   "expressc" :  The expression code generator                             */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

int vivc_flag;                      /*  TRUE if the last code-generated
                                        expression produced a "value in void
                                        context" error: used to help the syntax
                                        analyser recover from unknown-keyword
                                        errors, since unknown keywords are
                                        treated as yet-to-be-defined constants
                                        and thus as values in void context  */

static assembly_operand stack_pointer, temp_var1, temp_var2, temp_var3,
                 zero_operand, one_operand, valueless_operand;

static void make_operands(void)
{   stack_pointer.type = VARIABLE_OT;
    stack_pointer.value = 0;
    stack_pointer.marker = 0;
    temp_var1.type = VARIABLE_OT;
    temp_var1.value = 255;
    temp_var1.marker = 0;
    temp_var2.type = VARIABLE_OT;
    temp_var2.value = 254;
    temp_var2.marker = 0;
    temp_var3.type = VARIABLE_OT;
    temp_var3.value = 253;
    temp_var3.marker = 0;
    zero_operand.type = SHORT_CONSTANT_OT;
    zero_operand.value = 0;
    zero_operand.marker = 0;
    one_operand.type = SHORT_CONSTANT_OT;
    one_operand.value = 1;
    one_operand.marker = 0;
    valueless_operand.type = OMITTED_OT;
    valueless_operand.value = 0;
    valueless_operand.marker = 0;
}

/* ------------------------------------------------------------------------- */
/*  The table of operators.

    The ordering in this table is not significant except that it must match
    the #define's in "header.h"                                              */

operator operators[9*NUM_OPERATORS] =
{
                         /* ------------------------ */
                         /*  Level 0:  ,             */
                         /* ------------------------ */

  { 0, SEP_TT, COMMA_SEP,       IN_U, L_A, 0, -1, 0, 0, "comma" },

                         /* ------------------------ */
                         /*  Level 1:  =             */
                         /* ------------------------ */

  { 1, SEP_TT, SETEQUALS_SEP,   IN_U, R_A, 1, -1, 1, 0,
      "assignment operator '='" },

                         /* ------------------------ */
                         /*  Level 2:  ~~  &&  ||    */
                         /* ------------------------ */

  { 2, SEP_TT, LOGAND_SEP,      IN_U, L_A, 0, -1, 0, LOGOR_OP,
      "logical conjunction '&&'" },
  { 2, SEP_TT, LOGOR_SEP,       IN_U, L_A, 0, -1, 0, LOGAND_OP,
      "logical disjunction '||'" },
  { 2, SEP_TT, LOGNOT_SEP,     PRE_U, R_A, 0, -1, 0, LOGNOT_OP,
      "logical negation '~~'" },

                         /* ------------------------ */
                         /*  Level 3:  ==  ~=        */
                         /*            >  >=  <  <=  */
                         /*            has  hasnt    */
                         /*            in  notin     */
                         /*            provides      */
                         /*            ofclass       */
                         /* ------------------------ */

  { 3,     -1, -1,                -1, 0, 0, 400 + jz_zc, 0, NONZERO_OP,
      "expression used as condition then negated" },
  { 3,     -1, -1,                -1, 0, 0, 800 + jz_zc, 0, ZERO_OP,
      "expression used as condition" },
  { 3, SEP_TT, CONDEQUALS_SEP,  IN_U, 0, 0, 400 + je_zc, 0, NOTEQUAL_OP,
      "'==' condition" },
  { 3, SEP_TT, NOTEQUAL_SEP,    IN_U, 0, 0, 800 + je_zc, 0, CONDEQUALS_OP,
      "'~=' condition" },
  { 3, SEP_TT, GE_SEP,          IN_U, 0, 0, 800 + jl_zc, 0, LESS_OP,
      "'>=' condition" },
  { 3, SEP_TT, GREATER_SEP,     IN_U, 0, 0, 400 + jg_zc, 0, LE_OP,
      "'>' condition" },
  { 3, SEP_TT, LE_SEP,          IN_U, 0, 0, 800 + jg_zc, 0, GREATER_OP,
      "'<=' condition" },
  { 3, SEP_TT, LESS_SEP,        IN_U, 0, 0, 400 + jl_zc, 0, GE_OP,
      "'<' condition" },
  { 3, CND_TT, HAS_COND,        IN_U, 0, 0, 400 + test_attr_zc, 0, HASNT_OP,
      "'has' condition" },
  { 3, CND_TT, HASNT_COND,      IN_U, 0, 0, 800 + test_attr_zc, 0, HAS_OP,
      "'hasnt' condition" },
  { 3, CND_TT, IN_COND,         IN_U, 0, 0, 400 + jin_zc, 0, NOTIN_OP,
      "'in' condition" },
  { 3, CND_TT, NOTIN_COND,      IN_U, 0, 0, 800 + jin_zc, 0, IN_OP,
      "'notin' condition" },
  { 3, CND_TT, OFCLASS_COND,    IN_U, 0, 0, 600, 0, NOTOFCLASS_OP,
      "'ofclass' condition" },
  { 3, CND_TT, PROVIDES_COND,   IN_U, 0, 0, 601, 0, NOTPROVIDES_OP,
      "'provides' condition" },
  { 3,     -1, -1,                -1, 0, 0, 1000, 0, OFCLASS_OP,
      "negated 'ofclass' condition" },
  { 3,     -1, -1,                -1, 0, 0, 1001, 0, PROVIDES_OP,
      "negated 'provides' condition" },

                         /* ------------------------ */
                         /*  Level 4:  or            */
                         /* ------------------------ */

  { 4, CND_TT, OR_COND,         IN_U, L_A, 0, -1, 0, 0, "'or'" },

                         /* ------------------------ */
                         /*  Level 5:  +  binary -   */
                         /* ------------------------ */

  { 5, SEP_TT, PLUS_SEP,        IN_U, L_A, 0, add_zc, 0, 0, "'+'" },
  { 5, SEP_TT, MINUS_SEP,       IN_U, L_A, 0, sub_zc, 0, 0, "'-'" },

                         /* ------------------------ */
                         /*  Level 6:  *  /  %       */
                         /*            &  |  ~       */
                         /* ------------------------ */

  { 6, SEP_TT, TIMES_SEP,       IN_U, L_A, 0, mul_zc, 0, 0, "'*'" },
  { 6, SEP_TT, DIVIDE_SEP,      IN_U, L_A, 0, div_zc, 0, 0, "'/'" },
  { 6, SEP_TT, REMAINDER_SEP,   IN_U, L_A, 0, mod_zc, 0, 0,
      "remainder after division '%'" },
  { 6, SEP_TT, ARTAND_SEP,      IN_U, L_A, 0, and_zc, 0, 0,
      "bitwise AND '&'" },
  { 6, SEP_TT, ARTOR_SEP,       IN_U, L_A, 0, or_zc, 0, 0,
      "bitwise OR '|'" },
  { 6, SEP_TT, ARTNOT_SEP,     PRE_U, R_A, 0, -1, 0, 0,
      "bitwise NOT '~'" },

                         /* ------------------------ */
                         /*  Level 7:  ->  -->       */
                         /* ------------------------ */

  { 7, SEP_TT, ARROW_SEP,       IN_U, L_A, 0, loadb_zc, 0, 0,
      "byte array operator '->'" },
  { 7, SEP_TT, DARROW_SEP,      IN_U, L_A, 0, loadw_zc, 0, 0,
      "word array operator '-->'" },

                         /* ------------------------ */
                         /*  Level 8:  unary -       */
                         /* ------------------------ */

  { 8, SEP_TT, UNARY_MINUS_SEP, PRE_U, R_A, 0, -1, 0, 0,
      "unary minus" },

                         /* ------------------------ */
                         /*  Level 9:  ++  --        */
                         /*  (prefix or postfix)     */
                         /* ------------------------ */

  { 9, SEP_TT, INC_SEP,         PRE_U, R_A, 2, -1, 1, 0,
      "pre-increment operator '++'" },
  { 9, SEP_TT, POST_INC_SEP,   POST_U, R_A, 3, -1, 1, 0,
      "post-increment operator '++'" },
  { 9, SEP_TT, DEC_SEP,         PRE_U, R_A, 4, -1, 1, 0,
      "pre-decrement operator '--'" },
  { 9, SEP_TT, POST_DEC_SEP,   POST_U, R_A, 5, -1, 1, 0,
      "post-decrement operator '--'" },

                         /* ------------------------ */
                         /*  Level 10: .&  .#        */
                         /*            ..&  ..#      */
                         /* ------------------------ */

  {10, SEP_TT, PROPADD_SEP,     IN_U, L_A, 0, get_prop_addr_zc, 0, 0,
      "property address operator '.&'" },
  {10, SEP_TT, PROPNUM_SEP,     IN_U, L_A, 0, -1, 0, 0,
      "property length operator '.#'" },
  {10, SEP_TT, MPROPADD_SEP,    IN_U, L_A, 0, -1, 0, 0,
      "individual property address operator '..&'" },
  {10, SEP_TT, MPROPNUM_SEP,    IN_U, L_A, 0, -1, 0, 0,
      "individual property length operator '..#'" },

                         /* ------------------------ */
                         /*  Level 11:  function (   */
                         /* ------------------------ */

  {11, SEP_TT, OPENB_SEP,       IN_U, L_A, 0, -1, 1, 0,
      "function call" },

                         /* ------------------------ */
                         /*  Level 12:  .  ..        */
                         /* ------------------------ */

  {12, SEP_TT, MESSAGE_SEP,     IN_U, L_A, 0, -1, 0, 0,
      "individual property selector '..'" },
  {12, SEP_TT, PROPERTY_SEP,    IN_U, L_A, 0, get_prop_zc, 0, 0,
      "property selector '.'" },

                         /* ------------------------ */
                         /*  Level 13:  ::           */
                         /* ------------------------ */

  {13, SEP_TT, SUPERCLASS_SEP,  IN_U, L_A, 0, -1, 0, 0,
      "superclass operator '::'" },

                         /* ------------------------ */
                         /*  Miscellaneous operators */
                         /*  generated at lvalue     */
                         /*  checking time           */
                         /* ------------------------ */

  { 1,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ->   =   */
      "byte array entry assignment" },
  { 1,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      -->  =   */
      "word array entry assignment" },
  { 1,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ..   =   */
      "individual property assignment" },
  { 1,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      .    =   */
      "common property assignment" },

  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   ++ ->       */
      "byte array entry preincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   ++ -->      */
      "word array entry preincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   ++ ..       */
      "individual property preincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   ++ .        */
      "common property preincrement" },

  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   -- ->       */
      "byte array entry predecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   -- -->      */
      "word array entry predecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   -- ..       */
      "individual property predecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   -- .        */
      "common property predecrement" },

  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ->  ++   */
      "byte array entry postincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      --> ++   */
      "word array entry postincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ..  ++   */
      "individual property postincrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      .   ++   */
      "common property postincrement" },

  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ->  --   */
      "byte array entry postdecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      --> --   */
      "word array entry postdecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      ..  --   */
      "individual property postdecrement" },
  { 9,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*      .   --   */
      "common property postdecrement" },

  {11,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   x.y(args)   */
      "call to common property" },
  {11,     -1, -1,              -1,   -1,  0, -1, 1, 0,     /*   x..y(args)  */
      "call to individual property" }
};

/* --- Condition annotater ------------------------------------------------- */

static void annotate_for_conditions(int n, int a, int b)
{   int i, opnum = ET[n].operator_number;

    ET[n].label_after = -1;
    ET[n].to_expression = FALSE;
    ET[n].true_label = a;
    ET[n].false_label = b;

    if (ET[n].down == -1) return;

    if ((operators[opnum].precedence == 2)
        || (operators[opnum].precedence == 3))
    {   if ((a == -1) && (b == -1))
        {   if (opnum == LOGAND_OP)
            {   b = next_label++;
                ET[n].false_label = b;
                ET[n].to_expression = TRUE;
            }
            else
            {   a = next_label++;
                ET[n].true_label = a;
                ET[n].to_expression = TRUE;
            }
        }
    }

    switch(opnum)
    {   case LOGAND_OP:
            if (b == -1)
            {   b = next_label++;
                ET[n].false_label = b;
                ET[n].label_after = b;
            }
            annotate_for_conditions(ET[n].down, -1, b);
            if (b == ET[n].label_after)
                 annotate_for_conditions(ET[ET[n].down].right, a, -1);
            else annotate_for_conditions(ET[ET[n].down].right, a, b);
            return;
        case LOGOR_OP:
            if (a == -1)
            {   a = next_label++;
                ET[n].true_label = a;
                ET[n].label_after = a;
            }
            annotate_for_conditions(ET[n].down, a, -1);
            if (a == ET[n].label_after)
                 annotate_for_conditions(ET[ET[n].down].right, -1, b);
            else annotate_for_conditions(ET[ET[n].down].right, a, b);
            return;
    }

    i = ET[n].down;
    while (i != -1)
    {   annotate_for_conditions(i, -1, -1); i = ET[i].right; }
}

/* --- Code generator ------------------------------------------------------ */

static void value_in_void_context(assembly_operand AO)
{   char *t;
    switch(AO.type)
    {   case LONG_CONSTANT_OT:
        case SHORT_CONSTANT_OT:
            t = "<constant>";
            if (AO.marker == SYMBOL_MV)
                t = (char *) (symbs[AO.value]);
            break;
        default:
            t = (char *) (symbs[variable_tokens[AO.value]]);
            break;
    }
    vivc_flag = TRUE;

    if (strcmp(t, "print_paddr") == 0)
    obsolete_warning("ignoring 'print_paddr': use 'print (string)' instead");
    else
    if (strcmp(t, "print_addr") == 0)
    obsolete_warning("ignoring 'print_addr': use 'print (address)' instead");
    else
    if (strcmp(t, "print_char") == 0)
    obsolete_warning("ignoring 'print_char': use 'print (char)' instead");
    else
    ebf_error("assignment or statement", t);
}

static void write_result(assembly_operand to, assembly_operand from)
{   if (to.value == from.value) return;
    if (to.value == 0) assemble_1(push_zc, from);
    else               assemble_store(to, from);
}

static void compile_conditional(int oc,
    assembly_operand AO1, assembly_operand AO2, int label, int flag)
{   assembly_operand AO3; int the_zc;

    if (oc<200)
    {   assemble_2_branch(oc, AO1, AO2, label, flag); return;
    }

    AO3.type = VARIABLE_OT; AO3.value = 0; AO3.marker = 0;

    the_zc = (version_number == 3)?call_zc:call_vs_zc;
    if (oc == 201)
    assemble_3_to(the_zc, veneer_routine(OP__Pr_VR), AO1, AO2, AO3);
    else
    assemble_3_to(the_zc, veneer_routine(OC__Cl_VR), AO1, AO2, AO3);

    assemble_1_branch(jz_zc, AO3, label, !flag);
}

static void generate_code_from(int n, int void_flag)
{
    /*  When void, this must not leave anything on the stack. */

    int i, j, below, above, opnum, arity; assembly_operand Result;

    below = ET[n].down; above = ET[n].up;
    if (below == -1)
    {   if ((void_flag) && (ET[n].value.type != OMITTED_OT))
            value_in_void_context(ET[n].value);
        return;
    }

    opnum = ET[n].operator_number;

    if (opnum == COMMA_OP)
    {   generate_code_from(below, TRUE);
        generate_code_from(ET[below].right, void_flag);
        ET[n].value = ET[ET[below].right].value;
        goto OperatorGenerated;
    }

    if ((opnum == LOGAND_OP) || (opnum == LOGOR_OP))
    {   generate_code_from(below, FALSE);
        generate_code_from(ET[below].right, FALSE);
        goto OperatorGenerated;
    }

    if (opnum == -1)
    {
        /*  Signifies a SETEQUALS_OP which has already been done */

        ET[n].down = -1; return;
    }

    /*  Note that (except in the cases of comma and logical and/or) it
    	is essential to code generate the operands right to left, because
    	of the peculiar way the Z-machine's stack works:

    	    @sub sp sp -> a;

        (for instance) pulls to the first operand, then the second.  So

            @mul a 2 -> sp;
            @add b 7 -> sp;
    	    @sub sp sp -> a;

        calculates (b+7)-(a*2), not the other way around (as would be more
        usual in stack machines evaluating expressions written in reverse
        Polish notation).  (Basically this is because the Z-machine was
        designed to implement a LISP-like language naturally expressed
        in forward Polish notation: (PLUS 3 4), for instance.)               */

    i=below; arity = 0;
    while (i != -1)
    {   i = ET[i].right; arity++;
    }
    for (j=arity;j>0;j--)
    {   int k = 1;
        i = below;
        while (k<j)
        {   k++; i = ET[i].right;
        }
        generate_code_from(i, FALSE);
    }


    /*  Check this again, because code generation lower down may have
        stubbed it into -1  */

    if (ET[n].operator_number == -1)
    {   ET[n].down = -1; return;
    }

    if (operators[opnum].opcode_number >= 400)
    {
        /*  Conditional terms such as '==': */

        int a = ET[n].true_label, b = ET[n].false_label,
            branch_away, branch_other,
            make_jump_away = FALSE, make_branch_label = FALSE;
        int oc = operators[opnum].opcode_number-400, flag = TRUE;

        if (oc >= 400) { oc = oc - 400; flag = FALSE; }

        if ((oc == je_zc) && (arity == 2))
        {   i = ET[ET[n].down].right;
            if ((ET[i].value.value == zero_operand.value)
                && (ET[i].value.type == zero_operand.type))
                oc = jz_zc;
        }

        /*  If the condition has truth state flag, branch to
            label a, and if not, to label b.  Possibly one of a, b
            equals -1, meaning "continue from this instruction".

            branch_away is the label which is a branch away (the one
            which isn't immediately after) and flag is the truth
            state to branch there.

            Note that when multiple instructions are needed (because
            of the use of the 'or' operator) the branch_other label
            is created if need be.
        */

        /*  Reduce to the case where the branch_away label does exist:  */

        if (a == -1) { a = b; b = -1; flag = !flag; }

        branch_away = a; branch_other = b;
        if (branch_other != -1) make_jump_away = TRUE;

        if ((((oc != je_zc)&&(arity > 2)) || (arity > 4)) && (flag == FALSE))
        {
            /*  In this case, we have an 'or' situation where multiple
                instructions are needed and where the overall condition
                is negated.  That is, we have, e.g.

                   if not (A cond B or C or D) then branch_away

                which we transform into

                   if (A cond B) then branch_other
                   if (A cond C) then branch_other
                   if not (A cond D) then branch_away
                  .branch_other                                          */

            if (branch_other == -1)
            {   branch_other = next_label++; make_branch_label = TRUE;
            }
        }

        if (oc == jz_zc)
            assemble_1_branch(jz_zc, ET[below].value, branch_away, flag);
        else
        {   assembly_operand left_operand;

            if (arity == 2)
                compile_conditional(oc, ET[below].value,
                    ET[ET[below].right].value, branch_away, flag);
            else
            {   /*  The case of a condition using "or".
                    First: if the condition tests the stack pointer,
                    and it can't always be done in a single test, move
                    the value off the stack and into temporary variable
                    storage.  */

                if (((ET[below].value.type == VARIABLE_OT)
                     && (ET[below].value.value == 0))
                    && ((oc != je_zc) || (arity>4)) )
                {   left_operand.type = VARIABLE_OT;
                    left_operand.value = 255;
                    left_operand.marker = 0;
                    assemble_store(left_operand, ET[below].value);
                }
                else left_operand = ET[below].value;
                i = ET[below].right; arity--;

                /*  "left_operand" now holds the quantity to be tested;
                    "i" holds the right operand reached so far;
                    "arity" the number of right operands.  */

                while (i != -1)
                {   if ((oc == je_zc) && (arity>1))
                    {
                        /*  je_zc is an especially good case since the
                            Z-machine implements "or" for up to three
                            right operands automatically, though it's an
                            especially bad case to generate code for!  */

                        if (arity == 2)
                        {   assemble_3_branch(je_zc,
                              left_operand, ET[i].value,
                              ET[ET[i].right].value, branch_away, flag);
                            i = ET[i].right; arity--;
                        }
                        else
                        {   if ((arity == 3) || flag)
                              assemble_4_branch(je_zc, left_operand,
                                ET[i].value,
                                ET[ET[i].right].value,
                                ET[ET[ET[i].right].right].value,
                                branch_away, flag);
                            else
                              assemble_4_branch(je_zc, left_operand,
                                ET[i].value,
                                ET[ET[i].right].value,
                                ET[ET[ET[i].right].right].value,
                                branch_other, !flag);
                            i = ET[ET[i].right].right; arity -= 2;
                        }
                    }
                    else
                    {   /*  Otherwise we can compare the left_operand with
                            only one right operand at the time.  There are
                            two cases: it's the last right operand, or it
                            isn't.  */

                        if ((arity == 1) || flag)
                            compile_conditional(oc, left_operand,
                                ET[i].value, branch_away, flag);
                        else
                            compile_conditional(oc, left_operand,
                                ET[i].value, branch_other, !flag);
                    }
                    i = ET[i].right; arity--;
                }

            }
        }

        /*  NB: These two conditions cannot both occur, fortunately!  */

        if (make_branch_label) assemble_label_no(branch_other);
        if (make_jump_away) assemble_jump(branch_other);

        goto OperatorGenerated;
    }

    /*  The operator is now definitely one which produces a value  */

    if (void_flag && (!(operators[opnum].side_effect)))
        error_named("Evaluating this has no effect:",
            operators[opnum].description);

    /*  Where shall we put the resulting value?  */

    if (void_flag) Result = temp_var1;  /*  Throw it away  */
    else
    {   if ((above != -1) && (ET[above].operator_number == SETEQUALS_OP))
        {
            /*  If the node above is "set variable equal to", then
                make that variable the place to put the result, and
                delete the SETEQUALS_OP node since its effect has already
                been accomplished.  */

            ET[above].operator_number = -1;
            Result = ET[ET[above].down].value;
            ET[above].value = Result;
        }
        else Result = stack_pointer;  /*  Otherwise, put it on the stack  */
    }

    if (operators[opnum].opcode_number != -1)
    {
        /*  Operators directly translatable into Z-code opcodes: infix ops
            take two operands whereas pre/postfix operators take only one */

        if (operators[opnum].usage == IN_U)
            assemble_2_to(operators[opnum].opcode_number, ET[below].value,
                ET[ET[below].right].value, Result);

        else
            assemble_1_to(operators[opnum].opcode_number, ET[below].value,
                Result);
    }
    else
    switch(opnum)
    {
        case UNARY_MINUS_OP:
             assemble_2_to(sub_zc, zero_operand, ET[below].value, Result);
             break;
        case ARTNOT_OP:
             assemble_1_to(not_zc, ET[below].value, Result);
             break;

        case PROP_NUM_OP:
             assemble_2_to(get_prop_addr_zc, ET[below].value,
                 ET[ET[below].right].value, temp_var1);
             assemble_1_branch(jz_zc, temp_var1, next_label++, TRUE);
             assemble_1_to(get_prop_len_zc, temp_var1, temp_var1);
             assemble_label_no(next_label-1);
             if (!void_flag) write_result(Result, temp_var1);
             break;

        case MESSAGE_OP:
             j=1; AI.operand[0] = veneer_routine(RV__Pr_VR);
             goto GenFunctionCall;
        case MPROP_ADD_OP:
             j=1; AI.operand[0] = veneer_routine(RA__Pr_VR);
             goto GenFunctionCall;
        case MPROP_NUM_OP:
             j=1; AI.operand[0] = veneer_routine(RL__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_SETEQUALS_OP:
             j=1; AI.operand[0] = veneer_routine(WV__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_INC_OP:
             j=1; AI.operand[0] = veneer_routine(IB__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_DEC_OP:
             j=1; AI.operand[0] = veneer_routine(DB__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_POST_INC_OP:
             j=1; AI.operand[0] = veneer_routine(IA__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_POST_DEC_OP:
             j=1; AI.operand[0] = veneer_routine(DA__Pr_VR);
             goto GenFunctionCall;
        case SUPERCLASS_OP:
             j=1; AI.operand[0] = veneer_routine(RA__Sc_VR);
             goto GenFunctionCall;
        case PROP_CALL_OP:
             j=1; AI.operand[0] = veneer_routine(CA__Pr_VR);
             goto GenFunctionCall;
        case MESSAGE_CALL_OP:
             j=1; AI.operand[0] = veneer_routine(CA__Pr_VR);
             goto GenFunctionCall;


        case FCALL_OP:
             j = 0;

             if ((ET[below].value.type == VARIABLE_OT)
                 && (ET[below].value.value >= 256))
             {   int sf_number = ET[below].value.value - 256;

                 i = ET[below].right;
                 if (i == -1)
                 {   error("Argument to system function missing");
                     AI.operand[0] = one_operand;
                     AI.operand_count = 1;
                 }
                 else
                 {   j=0;
                     while (i != -1) { j++; i = ET[i].right; }

                     if (((sf_number != INDIRECT_SYSF) &&
                         (sf_number != RANDOM_SYSF) && (j > 1))
                         || ((sf_number == INDIRECT_SYSF) && (j>7)))
                     {   j=1;
                         error("System function given with too many arguments");
                     }
                     if (sf_number != RANDOM_SYSF)
                     {   int jcount;
                         i = ET[below].right;
                         for (jcount = 0; jcount < j; jcount++)
                         {   AI.operand[jcount] = ET[i].value;
                             i = ET[i].right;
                         }
                         AI.operand_count = j;
                     }
                 }
                 AI.store_variable_number = Result.value;
                 AI.branch_label_number = -1;

                 switch(sf_number)
                 {   case RANDOM_SYSF:
                         if (j>1)
                         {  assembly_operand AO, AO2; int arg_c, arg_et;
                            AO.value = j; AO.marker = 0;
                                AO.type = SHORT_CONSTANT_OT;
                            AO2.type = LONG_CONSTANT_OT;
                            AO2.value = begin_word_array();
                            AO2.marker = ARRAY_MV;

                            for (arg_c=0, arg_et = ET[below].right;arg_c<j;
                                 arg_c++, arg_et = ET[arg_et].right)
                            {   if (ET[arg_et].value.type == VARIABLE_OT)
              error("Only constants can be used as possible 'random' results");
                                array_entry(arg_c, ET[arg_et].value);
                            }
                            finish_array(arg_c);

                            assemble_1_to(random_zc, AO, temp_var1);
                            assemble_dec(temp_var1);
                            assemble_2_to(loadw_zc, AO2, temp_var1, Result);
                         }
                         else
                         assemble_1_to(random_zc,
                             ET[ET[below].right].value, Result);
                         break;

                     case PARENT_SYSF:
                         assemble_1_to(get_parent_zc,
                             ET[ET[below].right].value, Result);
                         break;

                     case ELDEST_SYSF:
                     case CHILD_SYSF:
                         assemble_objcode(get_child_zc,
                             ET[ET[below].right].value,
                             Result, -2, TRUE);
                         break;

                     case YOUNGER_SYSF:
                     case SIBLING_SYSF:
                         assemble_objcode(get_sibling_zc,
                             ET[ET[below].right].value,
                             Result, -2, TRUE);
                         break;

                     case INDIRECT_SYSF:
                         j=0; i = ET[below].right;
                         goto IndirectFunctionCall;

                     case CHILDREN_SYSF:
                         assemble_store(temp_var1, zero_operand);
                         assemble_objcode(get_child_zc,
                             ET[ET[below].right].value,
                             stack_pointer, next_label+1, FALSE);
                         assemble_label_no(next_label);
                         assemble_inc(temp_var1);
                         assemble_objcode(get_sibling_zc,
                             stack_pointer, stack_pointer, next_label, TRUE);
                         assemble_label_no(next_label+1);
                         assemble_store(temp_var2, stack_pointer);
                         if (!void_flag) write_result(Result, temp_var1);
                         next_label += 2; break;

                     case YOUNGEST_SYSF:
                         assemble_objcode(get_child_zc,
                             ET[ET[below].right].value,
                             temp_var1, next_label+1, FALSE);
                         assemble_1(push_zc, temp_var1);
                         assemble_label_no(next_label);
                         assemble_store(temp_var1, stack_pointer);
                         assemble_objcode(get_sibling_zc,
                             temp_var1, stack_pointer, next_label, TRUE);
                         assemble_label_no(next_label+1);
                         if (!void_flag) write_result(Result, temp_var1);
                         next_label += 2; break;

                     case ELDER_SYSF:
                         assemble_store(temp_var1,
                             ET[ET[below].right].value);
                         assemble_1_to(get_parent_zc, temp_var1, temp_var3);
                         assemble_1_branch(jz_zc, temp_var3,next_label+1,TRUE);
                         assemble_store(temp_var2, temp_var3);
                         assemble_store(temp_var3, zero_operand);
                         assemble_objcode(get_child_zc,
                             temp_var2, temp_var2, next_label, TRUE);
                         assemble_label_no(next_label++);
                         assemble_2_branch(je_zc, temp_var1, temp_var2,
                             next_label, TRUE);
                         assemble_store(temp_var3, temp_var2);
                         assemble_objcode(get_sibling_zc,
                             temp_var2, temp_var2, next_label - 1, TRUE);
                         assemble_label_no(next_label++);
                         if (!void_flag) write_result(Result, temp_var3);
                         break;

                     case METACLASS_SYSF:
                         assemble_2_to((version_number==3)?call_zc:call_vs_zc,
                             veneer_routine(Metaclass_VR),
                             ET[ET[below].right].value, Result);
                         break;
                 }
                 break;
             }

             GenFunctionCall:

             i = below;

             IndirectFunctionCall:

             while ((i != -1) && (j<8))
             {   AI.operand[j++] = ET[i].value;
                 i = ET[i].right;
             }

             if ((j > 4) && (version_number == 3))
             {   error("A function may be called with at most 3 arguments");
                 j = 4;
             }
             if ((j==8) && (i != -1))
             {   error("A function may be called with at most 7 arguments");
             }

             AI.operand_count = j;

             if ((void_flag) && (version_number >= 5))
             {   AI.store_variable_number = -1;
                 switch(j)
                 {   case 1: AI.internal_number = call_1n_zc; break;
                     case 2: AI.internal_number = call_2n_zc; break;
                     case 3: case 4: AI.internal_number = call_vn_zc; break;
                     case 5: case 6: case 7: case 8:
                         AI.internal_number = call_vn2_zc; break;
                 }
             }
             else
             {   AI.store_variable_number = Result.value;
                 if (version_number == 3)
                     AI.internal_number = call_zc;
                 else
                 switch(j)
                 {   case 1: AI.internal_number = call_1s_zc; break;
                     case 2: AI.internal_number = call_2s_zc; break;
                     case 3: case 4: AI.internal_number = call_vs_zc; break;
                     case 5: case 6: case 7: case 8:
                         AI.internal_number = call_vs2_zc; break;
                 }
             }

             AI.branch_label_number = -1;
             assemble_instruction(&AI);
             break;

        case SETEQUALS_OP:
             assemble_store(ET[below].value,
                 ET[ET[below].right].value);
             if (!void_flag) write_result(Result, ET[below].value);
             break;

        case PROPERTY_SETEQUALS_OP:
             if (!void_flag)
             {   assemble_store(temp_var1,
                     ET[ET[ET[below].right].right].value);
                 assemble_3(put_prop_zc, ET[below].value,
                     ET[ET[below].right].value,
                     temp_var1);
                 write_result(Result, temp_var1);
             }
             else
             {   assemble_3(put_prop_zc, ET[below].value,
                     ET[ET[below].right].value,
                     ET[ET[ET[below].right].right].value);
             }
             break;

        case ARROW_SETEQUALS_OP:
             if (!void_flag)
             {   assemble_store(temp_var1,
                     ET[ET[ET[below].right].right].value);
                 assemble_3(storeb_zc, ET[below].value,
                     ET[ET[below].right].value,
                     temp_var1);
                 write_result(Result, temp_var1);
             }
             else
             {   assemble_3(storeb_zc, ET[below].value,
                     ET[ET[below].right].value,
                     ET[ET[ET[below].right].right].value);
             }
             break;

        case DARROW_SETEQUALS_OP:
             if (!void_flag)
             {   assemble_store(temp_var1,
                     ET[ET[ET[below].right].right].value);
                 assemble_3(storew_zc, ET[below].value,
                     ET[ET[below].right].value,
                     temp_var1);
                 write_result(Result, temp_var1);
             }
             else
             {   assemble_3(storew_zc, ET[below].value,
                     ET[ET[below].right].value,
                     ET[ET[ET[below].right].right].value);
             }
             break;

        case INC_OP:
             assemble_inc(ET[below].value);
             if (!void_flag) write_result(Result, ET[below].value);
             break;
        case DEC_OP:
             assemble_dec(ET[below].value);
             if (!void_flag) write_result(Result, ET[below].value);
             break;
        case POST_INC_OP:
             if (!void_flag) write_result(Result, ET[below].value);
             assemble_inc(ET[below].value);
             break;
        case POST_DEC_OP:
             if (!void_flag) write_result(Result, ET[below].value);
             assemble_dec(ET[below].value);
             break;

        case ARROW_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadb_zc, temp_var1, temp_var2, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(storeb_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case ARROW_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadb_zc, temp_var1, temp_var2, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(storeb_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case ARROW_POST_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadb_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(storeb_zc, temp_var1, temp_var2, temp_var3);
             break;

        case ARROW_POST_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadb_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(storeb_zc, temp_var1, temp_var2, temp_var3);
             break;

        case DARROW_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadw_zc, temp_var1, temp_var2, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(storew_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case DARROW_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadw_zc, temp_var1, temp_var2, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(storew_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case DARROW_POST_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadw_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(storew_zc, temp_var1, temp_var2, temp_var3);
             break;

        case DARROW_POST_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(loadw_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(storew_zc, temp_var1, temp_var2, temp_var3);
             break;

        case PROPERTY_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(get_prop_zc, temp_var1, temp_var2, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(put_prop_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case PROPERTY_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(get_prop_zc, temp_var1, temp_var2, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(put_prop_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             break;

        case PROPERTY_POST_INC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(get_prop_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_inc(temp_var3);
             assemble_3(put_prop_zc, temp_var1, temp_var2, temp_var3);
             break;

        case PROPERTY_POST_DEC_OP:
             assemble_store(temp_var1, ET[below].value);
             assemble_store(temp_var2, ET[ET[below].right].value);
             assemble_2_to(get_prop_zc, temp_var1, temp_var2, temp_var3);
             if (!void_flag) write_result(Result, temp_var3);
             assemble_dec(temp_var3);
             assemble_3(put_prop_zc, temp_var1, temp_var2, temp_var3);
             break;

        default:
            printf("** Trouble op = %d i.e. '%s' **\n",
                opnum, operators[opnum].description);
            error("*** Expr code gen: Can't generate yet ***\n");
    }

    ET[n].value = Result;

    OperatorGenerated:

    if (ET[n].to_expression)
    {   if (ET[n].true_label != -1)
        {   assemble_1(push_zc, zero_operand);
            assemble_jump(next_label++);
            assemble_label_no(ET[n].true_label);
            assemble_1(push_zc, one_operand);
            assemble_label_no(next_label-1);
        }
        else
        {   assemble_1(push_zc, one_operand);
            assemble_jump(next_label++);
            assemble_label_no(ET[n].false_label);
            assemble_1(push_zc, zero_operand);
            assemble_label_no(next_label-1);
        }
        ET[n].value = stack_pointer;
    }
    else
        if (ET[n].label_after != -1)
            assemble_label_no(ET[n].label_after);

    ET[n].down = -1;
}

assembly_operand code_generate(assembly_operand AO, int context, int label)
{
    /*  Used in three contexts: VOID_CONTEXT, CONDITION_CONTEXT and
            QUANTITY_CONTEXT.

        If CONDITION_CONTEXT, then compile code branching to label number
            "label" if the condition is false: there's no return value.
        (Except that if label is -3 or -4 (internal codes for rfalse and
        rtrue rather than branch) then this is for branching when the
        condition is true.  This is used for optimising code generation
        for "if" statements.)

        Otherwise return the assembly operand containing the result
        (probably the stack pointer variable but not necessarily:
         e.g. is would be short constant 2 from the expression "j++, 2")     */

    vivc_flag = FALSE;

    if (AO.type != EXPRESSION_OT)
    {   switch(context)
        {   case VOID_CONTEXT:
                value_in_void_context(AO);
                AO.type = OMITTED_OT;
                AO.value = 0;
                break;
            case CONDITION_CONTEXT:
                if (label < -2) assemble_1_branch(jz_zc, AO, label, FALSE);
                else assemble_1_branch(jz_zc, AO, label, TRUE);
                AO.type = OMITTED_OT;
                AO.value = 0;
                break;
        }
        return AO;
    }

    if (expr_trace_level >= 2)
    {   printf("Raw parse tree:\n"); show_tree(AO, FALSE);
    }

    if (context == CONDITION_CONTEXT)
    {   if (label < -2) annotate_for_conditions(AO.value, label, -1);
        else annotate_for_conditions(AO.value, -1, label);
    }
    else annotate_for_conditions(AO.value, -1, -1);

    if (expr_trace_level >= 1)
    {   printf("Code generation for expression in ");
        switch(context)
        {   case VOID_CONTEXT: printf("void"); break;
            case CONDITION_CONTEXT: printf("condition"); break;
            case QUANTITY_CONTEXT: printf("quantity"); break;
            case ASSEMBLY_CONTEXT: printf("assembly"); break;
            case ARRAY_CONTEXT: printf("array initialisation"); break;
            default: printf("* ILLEGAL *"); break;
        }
        printf(" context with annotated tree:\n");
        show_tree(AO, TRUE);
    }

    generate_code_from(AO.value, (context==VOID_CONTEXT));
    return ET[AO.value].value;
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_expressc_vars(void)
{   make_operands();
}

extern void expressc_begin_pass(void)
{
}

extern void expressc_allocate_arrays(void)
{
}

extern void expressc_free_arrays(void)
{
}

/* ========================================================================= */
