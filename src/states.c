/* ------------------------------------------------------------------------- */
/*   "states" :  Statement translator                                        */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

static int match_colon(void)
{   get_next_token();
    if (token_type == SEP_TT)
    {   if (token_value == SEMICOLON_SEP)
            warning("Unlike C, Inform uses ':' to divide parts \
of a 'for' loop specification: replacing ';' with ':'");
        else
        if (token_value != COLON_SEP)
        {   ebf_error("':'", token_text);
            panic_mode_error_recovery();
            return(FALSE);
        }
    }
    else
    {   ebf_error("':'", token_text);
        panic_mode_error_recovery();
        return(FALSE);
    }
    return(TRUE);
}

static void match_open_bracket(void)
{   get_next_token();
    if ((token_type == SEP_TT) && (token_value == OPENB_SEP)) return;
    put_token_back();
    ebf_error("'('", token_text);
}

extern void match_close_bracket(void)
{   get_next_token();
    if ((token_type == SEP_TT) && (token_value == CLOSEB_SEP)) return;
    put_token_back();
    ebf_error("')'", token_text);
}

static void parse_action(void)
{   int level = 1, args = 0, codegen_action;
    assembly_operand AO, AO2, AO3, AO4, dump;

    dont_enter_into_symbol_table = TRUE;
    get_next_token();
    if ((token_type == SEP_TT) && (token_value == LESS_SEP))
    {   level = 2; get_next_token();
    }
    dont_enter_into_symbol_table = FALSE;

    if ((token_type==SEP_TT) && (token_value==OPENB_SEP))
    {   put_token_back();
        AO2 = parse_expression(ACTION_Q_CONTEXT);
        codegen_action = TRUE;
    }
    else
    {   codegen_action = FALSE;
        AO2 = action_of_name(token_text);
    }

    get_next_token();
    if (!((token_type == SEP_TT) && (token_value == GREATER_SEP)))
    {   put_token_back();
        args = 1;
        AO3 = parse_expression(ACTION_Q_CONTEXT);

        get_next_token();
        if (!((token_type == SEP_TT) && (token_value == GREATER_SEP)))
        {   put_token_back();
            args = 2;
            AO4 = parse_expression(QUANTITY_CONTEXT);
            get_next_token();
        }
    }
    if (level == 2)
    {   get_next_token();
        if (!((token_type == SEP_TT) && (token_value == GREATER_SEP)))
        {   put_token_back();
            ebf_error("'>>'", token_text);
        }
    }

    AO = veneer_routine(R_Process_VR);
    dump.type = VARIABLE_OT; dump.value = 255; dump.marker = 0;

    switch(args)
    {   case 0:
            if (codegen_action) AO2 = code_generate(AO2, QUANTITY_CONTEXT, -1);
            if (version_number>=5)
                assemble_2(call_2n_zc, AO, AO2);
            else
            if (version_number==4)
                assemble_2_to(call_vs_zc, AO, AO2, dump);
            else
                assemble_2_to(call_zc, AO, AO2, dump);
            break;
        case 1:
            AO3 = code_generate(AO3, QUANTITY_CONTEXT, -1);
            if (codegen_action) AO2 = code_generate(AO2, QUANTITY_CONTEXT, -1);
            if (version_number>=5)
                assemble_3(call_vn_zc, AO, AO2, AO3);
            else
            if (version_number==4)
                assemble_3_to(call_vs_zc, AO, AO2, AO3, dump);
            else
                assemble_3_to(call_zc, AO, AO2, AO3, dump);
            break;
        case 2:
            AO4 = code_generate(AO4, QUANTITY_CONTEXT, -1);
            AO3 = code_generate(AO3, QUANTITY_CONTEXT, -1);
            if (codegen_action) AO2 = code_generate(AO2, QUANTITY_CONTEXT, -1);
            if (version_number>=5)
                assemble_4(call_vn_zc, AO, AO2, AO3, AO4);
            else
            if (version_number==4)
                assemble_4_to(call_vs_zc, AO, AO2, AO3, AO4, dump);
            else
                assemble_4(call_zc, AO, AO2, AO3, AO4);
            break;
    }

    if (level == 2) assemble_0(rtrue_zc);
}

extern int parse_label(void)
{
    get_next_token();

    if ((token_type == SYMBOL_TT) &&
        (stypes[token_value] == LABEL_T))
    {   sflags[token_value] |= USED_SFLAG;
        return(svals[token_value]);
    }

    if ((token_type == SYMBOL_TT) && (sflags[token_value] & UNKNOWN_SFLAG))
    {   assign_symbol(token_value, next_label, LABEL_T);
        define_symbol_label(token_value);
        next_label++;
        sflags[token_value] |= CHANGE_SFLAG + USED_SFLAG;
        return(svals[token_value]);
    }

    ebf_error("label name", token_text);
    return 0;
}

#ifdef VARYING_STRINGS

static char *cur_varying_string; /* for error reporting */

static void compile_output_string(char *start, char *end)
{
   assembly_operand AO;
   char temp;
   if (end == start) return;
   temp = *end;
   *end = 0;
   AO.type = LONG_CONSTANT_OT;
   AO.marker = STRING_MV;
   AO.value  = compile_string(start, FALSE, FALSE);
   *end = temp;
   assemble_1(print_paddr_zc, AO);
}

static int is_varying_string(char *str)
{
   return strchr(str, '{') != NULL;
}

static char *varying_compile_string_nested(char *str);

static char *vary_error(void)
{
   error_named("Unbalanced varying string", cur_varying_string);
   return NULL;
}

/* advance cyclically through the substrings each time it is printed */
static char *varying_compile_string_advance(char *str, int wrap)
{
   int ln;
   assembly_operand arr, number, zero, stack;

   stack.value  = 0;
   stack.type   = VARIABLE_OT;
   stack.marker = 0;

   zero.value  = 0;
   zero.type   = SHORT_CONSTANT_OT;
   zero.marker = 0;

   number.value  = 0;
   number.type   = SHORT_CONSTANT_OT;
   number.marker = 0;

   arr.value  = begin_word_array();
   arr.type   = LONG_CONSTANT_OT;
   arr.marker = ARRAY_MV;
   array_entry(0, number);
   finish_array(1);

   /* array holds the 'state' of this printing routine */

   ln = next_label++;  /* target after all substrings */

   do {
      assemble_2_to(loadw_zc, arr, zero, stack);
      assemble_2_branch(je_zc, stack, number, next_label, FALSE);

      str = varying_compile_string_nested(str);
      if (str == NULL) return str;

      ++number.value;

      if (str[0] == '|') {
         /* str[-1] == '|', so there's a following alternative */
         assemble_3(storew_zc, arr, zero, number);
         assemble_jump(ln);
      } else if (str[0] == 0) {
         return vary_error();
      } else if (wrap) {
         assemble_3(storew_zc, arr, zero, zero);
      }  /* else do nothing */
      ++str;

      assemble_label_no(next_label++);   
   } while (str[-1] != '}');

   assemble_label_no(ln);

   return str;
}

/* select randomly from the substrings each time it is printed */
static char *varying_compile_string_random(char *str, int avoid_last)
{
   int ln;
   assembly_operand arr, number, zero, stack;
   assembly_operand temp_var1, temp_var2, temp_var3;

   temp_var1.type = VARIABLE_OT;
   temp_var1.value = 255;
   temp_var1.marker = 0;
   temp_var2.type = VARIABLE_OT;
   temp_var2.value = 254;
   temp_var2.marker = 0;
   temp_var3.type = VARIABLE_OT;
   temp_var3.value = 253;
   temp_var3.marker = 0;

   stack.value  = 0;
   stack.type   = VARIABLE_OT;
   stack.marker = 0;

   zero.value  = 0;
   zero.type   = SHORT_CONSTANT_OT;
   zero.marker = 0;

   number.value  = 0;
   number.type   = SHORT_CONSTANT_OT;
   number.marker = 0;

   arr.value  = begin_word_array();
   arr.type   = LONG_CONSTANT_OT;
   arr.marker = ARRAY_MV;

   finish_array(2);

   number.value = 1;

   if (!avoid_last) {
      /* pick equally */
      /* result = random(number)-1 */
      assemble_2_to(loadw_zc, arr, number, temp_var2);
      assemble_1_to(random_zc, temp_var2, temp_var1);
      assemble_dec(temp_var1);
      assemble_3(storew_zc, arr, zero, temp_var1);
   } else {
      /* disallow previous */
      /* result = (result + random(number-1)) mod number */
      assemble_2_to(loadw_zc, arr, number, temp_var2);
      assemble_dec(temp_var2);
      assemble_1_to(random_zc, temp_var2, temp_var1);
      assemble_inc(temp_var2);
      assemble_2_to(loadw_zc, arr, zero, temp_var3);
      assemble_2_to(add_zc, temp_var3, temp_var1, temp_var1);
      assemble_2_to(mod_zc, temp_var1, temp_var2, temp_var1);
      assemble_3(storew_zc, arr, zero, temp_var1);
   }

   ln = next_label++;  /* target after all substrings */

   number.value = 0;

   do {
      int ln2 = next_label++;
      assemble_2_to(loadw_zc, arr, zero, stack);
      assemble_2_branch(je_zc, stack, number, ln2, FALSE);

      str = varying_compile_string_nested(str);
      if (str == NULL) return str;

      ++number.value;

      if (str[0] == '|') {
         assemble_jump(ln);
      } else if (str[0] == 0) {
         return vary_error();
      }
      ++str;

      assemble_label_no(ln2);   
   } while (str[-1] != '}');

   assemble_label_no(ln);
   patch_word_array(arr.value, 1, number.value);
   patch_word_array(arr.value, 0, number.value-1);

   return str;
}

static char *varying_compile_string_base(char *str)
{
   switch(str[1]) {
      case '%': str = varying_compile_string_random (str+2, FALSE); break;
      case '!': str = varying_compile_string_random (str+2, TRUE ); break;
      case '&': str = varying_compile_string_advance(str+2, TRUE ); break;
      default:  str = varying_compile_string_advance(str+1, FALSE); break;
   }
   return str;
}

static char *varying_compile_string_nested(char *text)
{
   char *s = text;
   while (*s && *s != '}' && *s != '|') {
      if (*s == '{') {
         compile_output_string(text, s);
         s = text = varying_compile_string_base(s);
         if (s == NULL) return s;
      } else
         ++s;
   }
   compile_output_string(text, s);
   return s;
}

static void varying_compile_string(char *text)
{
   char *s = text;
   cur_varying_string = text;  /* for error reporting */
   while (*s) {
      if (*s == '{') {
         compile_output_string(text, s);
         s = text = varying_compile_string_base(s);
         if (s == NULL) return;
      } else if (*s == '}' || *s == '|') {
         vary_error();
         return;
      } else
         ++s;
   }
   compile_output_string(text, s);
}
#endif




static void parse_print(int finally_return)
{   int count = 0; assembly_operand AO, AO2;

    /*  print <printlist> -------------------------------------------------- */
    /*  print_ret <printlist> ---------------------------------------------- */
    /*  <literal-string> --------------------------------------------------- */
    /*                                                                       */
    /*  <printlist> is a comma-separated list of items:                      */
    /*                                                                       */
    /*       <literal-string>                                                */
    /*       <other-expression>                                              */
    /*       (char) <expression>                                             */
    /*       (address) <expression>                                          */
    /*       (string) <expression>                                           */
    /*       (a) <expression>                                                */
    /*       (the) <expression>                                              */
    /*       (The) <expression>                                              */
    /*       (name) <expression>                                             */
    /*       (number) <expression>                                           */
    /*       (property) <expression>                                         */
    /*       (<routine>) <expression>                                        */
    /*       (object) <expression>     (for use in low-level code only)      */
    /* --------------------------------------------------------------------- */

    do
    {   AI.text = token_text;
        if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)) break;
        switch(token_type)
        {   case DQ_TT:
#ifndef VARYING_STRINGS
              if (strlen(token_text) > 32)
#else
              if (strlen(token_text) > 32 || is_varying_string(token_text))
#endif
              {
#ifndef VARYING_STRINGS
                  AO.marker = STRING_MV;
                  AO.type   = LONG_CONSTANT_OT;
                  AO.value  = compile_string(token_text, FALSE, FALSE);
                  assemble_1(print_paddr_zc, AO);
#else
                  varying_compile_string(token_text);
#endif
                  if (finally_return)
                  {   get_next_token();
                      if ((token_type == SEP_TT)
                          && (token_value == SEMICOLON_SEP))
                      {   assemble_0(new_line_zc);
                          assemble_0(rtrue_zc);
                          return;
                      }
                      put_token_back();
                  }
                  break;
              }
              if (finally_return)
              {   get_next_token();
                  if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                  {   assemble_0(print_ret_zc); return;
                  }
                  put_token_back();
              }
              assemble_0(print_zc);
              break;

            case SEP_TT:
              if (token_value == OPENB_SEP)
              {   misc_keywords.enabled = TRUE;
                  get_next_token();
                  get_next_token();
                  if ((token_type == SEP_TT) && (token_value == CLOSEB_SEP))
                  {   put_token_back(); put_token_back();
                      local_variables.enabled = FALSE;
                      get_next_token();
                      misc_keywords.enabled = FALSE;
                      local_variables.enabled = TRUE;

                      if ((token_type == STATEMENT_TT)
                          &&(token_value == STRING_CODE))
                      {   token_type = MISC_KEYWORD_TT;
                          token_value = STRING_MK;
                      }

                      switch(token_type)
                      {
                        case MISC_KEYWORD_TT:
                          switch(token_value)
                          {   case CHAR_MK:
                                  get_next_token();
                                  assemble_1(print_char_zc,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                  QUANTITY_CONTEXT, -1));
                                  goto PrintTermDone;
                              case ADDRESS_MK:
                                  get_next_token();
                                  assemble_1(print_addr_zc,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                  QUANTITY_CONTEXT, -1));
                                  goto PrintTermDone;
                              case STRING_MK:
                                  get_next_token();
                                  assemble_1(print_paddr_zc,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                  QUANTITY_CONTEXT, -1));
                                  goto PrintTermDone;
                              case OBJECT_MK:
                                  get_next_token();
                                  assemble_1(print_obj_zc,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                  QUANTITY_CONTEXT, -1));
                                  goto PrintTermDone;
                              case THE_MK:
                                  AO = veneer_routine(DefArt_VR);
                                  goto PrintByRoutine;
                              case A_MK:
                                  AO = veneer_routine(InDefArt_VR);
                                  goto PrintByRoutine;
                              case CAP_THE_MK:
                                  AO = veneer_routine(CDefArt_VR);
                                  goto PrintByRoutine;
                              case NAME_MK:
                                  AO = veneer_routine(PrintShortName_VR);
                                  goto PrintByRoutine;
                              case NUMBER_MK:
                                  AO = veneer_routine(EnglishNumber_VR);
                                  goto PrintByRoutine;
                              case PROPERTY_MK:
                                  AO = veneer_routine(Print__Pname_VR);
                                  goto PrintByRoutine;
                              default:
               error_named("A reserved word was used as a print specification:",
                                      token_text);
                          }
                          break;

                        case SYMBOL_TT:
                          if (sflags[token_value] & UNKNOWN_SFLAG)
                          {   AO.type = LONG_CONSTANT_OT;
                              AO.value = token_value;
                              AO.marker = SYMBOL_MV;
                          }
                          else
                          {   AO.type = LONG_CONSTANT_OT;
                              AO.value = svals[token_value];
                              AO.marker = IROUTINE_MV;
                              if (stypes[token_value] != ROUTINE_T)
                                ebf_error("printing routine name", token_text);
                          }
                          sflags[token_value] |= USED_SFLAG;

                          PrintByRoutine:

                          AO2.type = VARIABLE_OT;
                          AO2.value = 255;
                          AO2.marker = 0;
                          get_next_token();
                          if (version_number >= 5)
                            assemble_2(call_2n_zc, AO,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                QUANTITY_CONTEXT, -1));
                          else if (version_number == 4)
                            assemble_2_to(call_vs_zc, AO,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                QUANTITY_CONTEXT, -1), AO2);
                          else
                            assemble_2_to(call_zc, AO,
                              code_generate(parse_expression(QUANTITY_CONTEXT),
                                QUANTITY_CONTEXT, -1), AO2);
                          goto PrintTermDone;

                        default: ebf_error("print specification", token_text);
                          get_next_token();
                          assemble_1(print_num_zc,
                          code_generate(parse_expression(QUANTITY_CONTEXT),
                                QUANTITY_CONTEXT, -1));
                          goto PrintTermDone;
                      }
                  }
                  put_token_back(); put_token_back(); put_token_back();
                  misc_keywords.enabled = FALSE;
                  assemble_1(print_num_zc,
                      code_generate(parse_expression(QUANTITY_CONTEXT),
                          QUANTITY_CONTEXT, -1));
                  break;
              }

            default:
              put_token_back(); misc_keywords.enabled = FALSE;
              assemble_1(print_num_zc,
                  code_generate(parse_expression(QUANTITY_CONTEXT),
                      QUANTITY_CONTEXT, -1));
              break;
        }

        PrintTermDone: misc_keywords.enabled = FALSE;

        count++;
        get_next_token();
        if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)) break;
        if ((token_type != SEP_TT) || (token_value != COMMA_SEP))
        {   ebf_error("comma", token_text);
            panic_mode_error_recovery(); return;
        }
        else get_next_token();
    } while(TRUE);

    if (count == 0) ebf_error("something to print", token_text);
    if (finally_return)
    {   assemble_0(new_line_zc);
        assemble_0(rtrue_zc);
    }
}

extern void parse_statement(int break_label, int continue_label)
{   int ln, ln2, ln3, flag;
    assembly_operand AO, AO2, AO3, AO4;
    dbgl spare_dbgl1, spare_dbgl2;

    if ((token_type == SEP_TT) && (token_value == PROPERTY_SEP))
    {   /*  That is, a full stop, signifying a label  */

        get_next_token();
        if (token_type == SYMBOL_TT)
        {
            if (sflags[token_value] & UNKNOWN_SFLAG)
            {   assign_symbol(token_value, next_label, LABEL_T);
                sflags[token_value] |= USED_SFLAG;
                assemble_label_no(next_label);
                define_symbol_label(token_value);
                next_label++;
            }
            else
            {   if (stypes[token_value] != LABEL_T) goto LabelError;
                if (sflags[token_value] & CHANGE_SFLAG)
                {   sflags[token_value] &= (~(CHANGE_SFLAG));
                    assemble_label_no(svals[token_value]);
                    define_symbol_label(token_value);
                }
                else error_named("Duplicate definition of label:", token_text);
            }

            get_next_token();
            if ((token_type != SEP_TT) || (token_value != SEMICOLON_SEP))
            {   ebf_error("';'", token_text);
                put_token_back(); return;
            }

            /*  Interesting point of Inform grammar: a statement can only
                consist solely of a label when it is immediately followed
                by a "}".                                                    */

            get_next_token();
            if ((token_type == SEP_TT) && (token_value == CLOSE_BRACE_SEP))
            {   put_token_back(); return;
            }
            parse_statement(break_label, continue_label);
            return;
        }
        LabelError: ebf_error("label name", token_text);
    }

    if ((token_type == SEP_TT) && (token_value == HASH_SEP))
    {   parse_directive(TRUE);
        parse_statement(break_label, continue_label); return;
    }

    if ((token_type == SEP_TT) && (token_value == AT_SEP))
    {   parse_assembly(); return;
    }

    if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP)) return;

    if (token_type == DQ_TT)
    {   parse_print(TRUE); return;
    }

    if ((token_type == SEP_TT) && (token_value == LESS_SEP))
    {   parse_action(); goto StatementTerminator; }

    if (token_type == EOF_TT)
    {   ebf_error("statement", token_text); return; }

    if (token_type != STATEMENT_TT)
    {   put_token_back();
        AO = parse_expression(VOID_CONTEXT);
        code_generate(AO, VOID_CONTEXT, -1);
        if (vivc_flag) { panic_mode_error_recovery(); return; }
        goto StatementTerminator;
    }

    statements.enabled = FALSE;

    switch(token_value)
    {
    /*  -------------------------------------------------------------------- */
    /*  box <string-1> ... <string-n> -------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case BOX_CODE:
             if (version_number == 3)
             warning("The 'box' statement has no effect in a version 3 game");
                 AO3.type = LONG_CONSTANT_OT;
                 AO3.value = begin_table_array();
                 AO3.marker = ARRAY_MV;
                 ln = 0; ln2 = 0;
                 do
                 {   get_next_token();
                     if ((token_type==SEP_TT)&&(token_value==SEMICOLON_SEP))
                         break;
                     if (token_type != DQ_TT)
                         ebf_error("text of box line in double-quotes",
                             token_text);
                     {   int i, j;
                         for (i=0, j=0; token_text[i] != 0; j++)
                             if (token_text[i] == '@')
                             {   if (token_text[i+1] == '@')
                                 {   i = i + 2;
                                     while (isdigit(token_text[i])) i++;
                                 }
                                 else
                                 {   i++;
                                     if (token_text[i] != 0) i++;
                                     if (token_text[i] != 0) i++;
                                 }
                             }
                             else i++;
                         if (j > ln2) ln2 = j;
                     }
                     put_token_back();
                     array_entry(ln++,parse_expression(CONSTANT_CONTEXT));
                 } while (TRUE);
                 finish_array(ln);
                 if (ln == 0)
                     error("No lines of text given for 'box' display");

                 if (version_number == 3) return;

                 AO2.type = SHORT_CONSTANT_OT; AO2.value = ln2; AO2.marker = 0;
                 AO4.type = VARIABLE_OT; AO4.value = 255; AO4.marker = 0;
                 assemble_3_to(call_vs_zc, veneer_routine(Box__Routine_VR),
                     AO2, AO3, AO4);
                 return;

    /*  -------------------------------------------------------------------- */
    /*  break -------------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case BREAK_CODE:
                 if (break_label == -1)
                 error("'break' can only be used in a loop or 'switch' block");
                 else
                     assemble_jump(break_label);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  continue ----------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case CONTINUE_CODE:
                 if (continue_label == -1)
                 error("'continue' can only be used in a loop block");
                 else
                     assemble_jump(continue_label);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  do <codeblock> until (<condition>) --------------------------------- */
    /*  -------------------------------------------------------------------- */

        case DO_CODE:
                 assemble_label_no(ln = next_label++);
                 ln2 = next_label++; ln3 = next_label++;
                 parse_code_block(ln3, ln2, 0);
                 statements.enabled = TRUE;
                 get_next_token();
                 if ((token_type == STATEMENT_TT)
                     && (token_value == UNTIL_CODE))
                 {   assemble_label_no(ln2);
                     match_open_bracket();
                     AO = parse_expression(CONDITION_CONTEXT);
                     match_close_bracket();
                     code_generate(AO, CONDITION_CONTEXT, ln);
                 }
                 else error("'do' without matching 'until'");

                 assemble_label_no(ln3);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  font on/off -------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case FONT_CODE:
                 misc_keywords.enabled = TRUE;
                 get_next_token();
                 misc_keywords.enabled = FALSE;
                 if ((token_type != MISC_KEYWORD_TT)
                     || ((token_value != ON_MK)
                         && (token_value != OFF_MK)))
                 {   ebf_error("'on' or 'off'", token_text);
                     panic_mode_error_recovery();
                     break;
                 }

                 AO.type = SHORT_CONSTANT_OT;
                 AO.value = 0;
                 AO.marker = 0;
                 AO2.type = SHORT_CONSTANT_OT;
                 AO2.value = 8;
                 AO2.marker = 0;
                 AO3.type = VARIABLE_OT;
                 AO3.value = 255;
                 AO3.marker = 0;
                 assemble_2_to(loadw_zc, AO, AO2, AO3);

                 if (token_value == ON_MK)
                 {   AO4.type = LONG_CONSTANT_OT;
                     AO4.value = 0xfffd;
                     AO4.marker = 0;
                     assemble_2_to(and_zc, AO4, AO3, AO3);
                 }
                 else
                 {   AO4.type = SHORT_CONSTANT_OT;
                     AO4.value = 2;
                     AO4.marker = 0;
                     assemble_2_to(or_zc, AO4, AO3, AO3);
                 }

                 assemble_3(storew_zc, AO, AO2, AO3);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  for (<initialisation> : <continue-condition> : <updating>) --------- */
    /*  -------------------------------------------------------------------- */

        /*  Note that it's legal for any or all of the three sections of a
            'for' specification to be empty.  This 'for' implementation
            often wastes 3 bytes with a redundant branch rather than keep
            expression parse trees for long periods (as previous versions
            of Inform did, somewhat crudely by simply storing the textual
            form of a 'for' loop).  It is adequate for now.                  */

        case FOR_CODE:
                 match_open_bracket();
                 get_next_token();

                 /*  Initialisation code  */

                 if (!((token_type==SEP_TT)&&(token_value==COLON_SEP)))
                 {   put_token_back();
                     if (!((token_type==SEP_TT)&&(token_value==SUPERCLASS_SEP)))
                     {   sequence_point_follows = TRUE;
                         debug_line_ref = token_line_ref;
                         code_generate(parse_expression(FORINIT_CONTEXT),
                             VOID_CONTEXT, -1);
                     }
                     get_next_token();
                     if ((token_type==SEP_TT)&&(token_value == SUPERCLASS_SEP))
                     {   get_next_token();
                         if ((token_type==SEP_TT)&&(token_value == CLOSEB_SEP))
                         {   assemble_label_no(ln = next_label++);
                             ln2 = next_label++;
                             parse_code_block(ln2, ln, 0);
                             sequence_point_follows = FALSE;
                             if (!execution_never_reaches_here)
                                 assemble_jump(ln);
                             assemble_label_no(ln2);
                             return;
                         }
                         AO.type = OMITTED_OT;
                         goto ParseUpdate;
                     }
                     put_token_back();
                     if (!match_colon()) break;
                 }

                 get_next_token();
                 AO.type = OMITTED_OT;
                 if (!((token_type==SEP_TT)&&(token_value==COLON_SEP)))
                 {   put_token_back();
                     spare_dbgl1 = token_line_ref;
                     AO = parse_expression(CONDITION_CONTEXT);
                     if (!match_colon()) break;
                 }
                 get_next_token();

                 ParseUpdate:
                 AO2.type = OMITTED_OT; flag = 0;
                 if (!((token_type==SEP_TT)&&(token_value==CLOSEB_SEP)))
                 {   put_token_back();
                     spare_dbgl2 = token_line_ref;
                     AO2 = parse_expression(VOID_CONTEXT);
                     match_close_bracket();
                     flag = test_for_incdec(AO2);
                 }

                 ln = next_label++;
                 ln2 = next_label++;
                 ln3 = next_label++;

                 if ((AO2.type == OMITTED_OT) || (flag != 0))
                 {
                     assemble_label_no(ln);
                     if (flag==0) assemble_label_no(ln2);

                     /*  The "finished yet?" condition  */

                     if (AO.type != OMITTED_OT)
                     {   sequence_point_follows = TRUE;
                         debug_line_ref = spare_dbgl1;
                         code_generate(AO, CONDITION_CONTEXT, ln3);
                     }

                 }
                 else
                 {
                     /*  This is the jump which could be avoided with the aid
                         of long-term expression storage  */

                     sequence_point_follows = FALSE;
                     assemble_jump(ln2);

                     /*  The "update" part  */

                     assemble_label_no(ln);
                     sequence_point_follows = TRUE;
                     debug_line_ref = spare_dbgl2;
                     code_generate(AO2, VOID_CONTEXT, -1);

                     assemble_label_no(ln2);

                     /*  The "finished yet?" condition  */

                     if (AO.type != OMITTED_OT)
                     {   sequence_point_follows = TRUE;
                         debug_line_ref = spare_dbgl1;
                         code_generate(AO, CONDITION_CONTEXT, ln3);
                     }
                 }

                 if (flag != 0)
                 {
                     /*  In this optimised case, update code is at the end
                         of the loop block, so "continue" goes there  */

                     parse_code_block(ln3, ln2, 0);
                     assemble_label_no(ln2);

                     sequence_point_follows = TRUE;
                     debug_line_ref = spare_dbgl2;
                     if (flag > 0)
                     {   AO3.type = SHORT_CONSTANT_OT;
                         AO3.value = flag;
                         if (module_switch
                             && (flag>=16) && (flag<LOWEST_SYSTEM_VAR_NUMBER))
                             AO3.marker = VARIABLE_MV;
                         else AO3.marker = 0;
                         assemble_1(inc_zc, AO3);
                     }
                     else
                     {   AO3.type = SHORT_CONSTANT_OT;
                         AO3.value = -flag;
                         if ((module_switch) && (flag>=16)
                             && (flag<LOWEST_SYSTEM_VAR_NUMBER))
                             AO3.marker = VARIABLE_MV;
                         else AO3.marker = 0;
                         assemble_1(dec_zc, AO3);
                     }
                     assemble_jump(ln);
                 }
                 else
                 {
                     /*  In the unoptimised case, update code is at the
                         start of the loop block, so "continue" goes there  */

                     parse_code_block(ln3, ln, 0);
                     if (!execution_never_reaches_here)
                     {   sequence_point_follows = FALSE;
                         assemble_jump(ln);
                     }
                 }

                 assemble_label_no(ln3);
                 return;

    /*  -------------------------------------------------------------------- */
    /*  give <expression> [~]attr [, [~]attr [, ...]] ---------------------- */
    /*  -------------------------------------------------------------------- */

        case GIVE_CODE:
                 AO = code_generate(parse_expression(QUANTITY_CONTEXT),
                          QUANTITY_CONTEXT, -1);
                 if ((AO.type == VARIABLE_OT) && (AO.value == 0))
                 {   AO.value = 252;
                     AO.marker = 0;
                     AO.type = SHORT_CONSTANT_OT;
                     if (version_number != 6) assemble_1(pull_zc, AO);
                     else assemble_0_to(pull_zc, AO);
                     AO.type = VARIABLE_OT;
                 }

                 do
                 {   get_next_token();
                     if ((token_type == SEP_TT)&&(token_value == SEMICOLON_SEP))
                         return;
                     if ((token_type == SEP_TT)&&(token_value == ARTNOT_SEP))
                         ln = clear_attr_zc;
                     else
                     {   if ((token_type == SYMBOL_TT)
                             && (stypes[token_value] != ATTRIBUTE_T))
                           warning_named("This is not a declared Attribute:",
                             token_text);
                         ln = set_attr_zc;
                         put_token_back();
                     }
                     AO2 = code_generate(parse_expression(QUANTITY_CONTEXT),
                               QUANTITY_CONTEXT, -1);
                     assemble_2(ln, AO, AO2);
                 } while(TRUE);

    /*  -------------------------------------------------------------------- */
    /*  if (<condition>) <codeblock> [else <codeblock>] -------------------- */
    /*  -------------------------------------------------------------------- */

        case IF_CODE:
                 flag = FALSE;

                 match_open_bracket();
                 AO = parse_expression(CONDITION_CONTEXT);
                 match_close_bracket();

                 statements.enabled = TRUE;
                 get_next_token();
                 if ((token_type == STATEMENT_TT)&&(token_value == RTRUE_CODE))
                     ln = -4;
                 else
                 if ((token_type == STATEMENT_TT)&&(token_value == RFALSE_CODE))
                     ln = -3;
                 else
                 {   put_token_back();
                     ln = next_label++;
                 }

                 code_generate(AO, CONDITION_CONTEXT, ln);

                 if (ln >= 0) parse_code_block(break_label, continue_label, 0);
                 else
                 {   get_next_token();
                     if ((token_type != SEP_TT)
                         || (token_value != SEMICOLON_SEP))
                     {   ebf_error("';'", token_text);
                         put_token_back();
                     }
                 }

                 statements.enabled = TRUE;
                 get_next_token();
                 if ((token_type == STATEMENT_TT) && (token_value == ELSE_CODE))
                 {   flag = TRUE;
                     if (ln >= 0)
                     {   ln2 = next_label++;
                         if (!execution_never_reaches_here)
                         {   sequence_point_follows = FALSE;
                             assemble_jump(ln2);
                         }
                     }
                 }
                 else put_token_back();

                 if (ln >= 0) assemble_label_no(ln);

                 if (flag)
                 {   parse_code_block(break_label, continue_label, 0);
                     if (ln >= 0) assemble_label_no(ln2);
                 }

                 return;

    /*  -------------------------------------------------------------------- */
    /*  inversion ---------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case INVERSION_CODE:
                 AO.marker = 0;
                 AO.type   = SHORT_CONSTANT_OT;
                 AO.value  = 0;

                 AO2.marker = 0;
                 AO2.type   = SHORT_CONSTANT_OT;

                 AO3.marker = 0;
                 AO3.type = VARIABLE_OT;
                 AO3.value = 255;

                 AO2.value  = 60;
                 assemble_2_to(loadb_zc, AO, AO2, AO3);
                 assemble_1(print_char_zc, AO3);
                 AO2.value  = 61;
                 assemble_2_to(loadb_zc, AO, AO2, AO3);
                 assemble_1(print_char_zc, AO3);
                 AO2.value  = 62;
                 assemble_2_to(loadb_zc, AO, AO2, AO3);
                 assemble_1(print_char_zc, AO3);
                 AO2.value  = 63;
                 assemble_2_to(loadb_zc, AO, AO2, AO3);
                 assemble_1(print_char_zc, AO3);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  jump <label> ------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case JUMP_CODE:
                 assemble_jump(parse_label());
                 break;

    /*  -------------------------------------------------------------------- */
    /*  move <expression> to <expression> ---------------------------------- */
    /*  -------------------------------------------------------------------- */

        case MOVE_CODE:
                 misc_keywords.enabled = TRUE;
                 AO = parse_expression(QUANTITY_CONTEXT);

                 get_next_token();
                 misc_keywords.enabled = FALSE;
                 if ((token_type != MISC_KEYWORD_TT)
                     || (token_value != TO_MK))
                 {   ebf_error("'to'", token_text);
                     panic_mode_error_recovery();
                     return;
                 }

                 AO2 = code_generate(parse_expression(QUANTITY_CONTEXT),
                     QUANTITY_CONTEXT, -1);
                 AO = code_generate(AO, QUANTITY_CONTEXT, -1);
                 assemble_2(insert_obj_zc, AO, AO2);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  new_line ----------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case NEW_LINE_CODE:  assemble_0(new_line_zc); break;

    /*  -------------------------------------------------------------------- */
    /*  objectloop (<initialisation>) <codeblock> -------------------------- */
    /*  -------------------------------------------------------------------- */

        case OBJECTLOOP_CODE:

                 match_open_bracket();
                 get_next_token();
                 if (token_type == LOCAL_VARIABLE_TT)
                     AO.value = token_value;
                 else
                 if ((token_type == SYMBOL_TT) &&
                     (stypes[token_value] == GLOBAL_VARIABLE_T))
                     AO.value = svals[token_value];
                 else
                 {   ebf_error("'objectloop' variable", token_text);
                     panic_mode_error_recovery(); break;
                 }
                 AO.type = VARIABLE_OT;
                 if ((module_switch) && (AO.value >= 16)
                     && (AO.value < LOWEST_SYSTEM_VAR_NUMBER))
                     AO.marker = VARIABLE_MV;
                 else AO.marker = 0;
                 misc_keywords.enabled = TRUE;
                 get_next_token(); flag = TRUE;
                 misc_keywords.enabled = FALSE;
                 if ((token_type == SEP_TT) && (token_value == CLOSEB_SEP))
                     flag = FALSE;

                 ln = 0;
                 if ((token_type == MISC_KEYWORD_TT)
                     && (token_value == NEAR_MK)) ln = 1;
                 if ((token_type == MISC_KEYWORD_TT)
                     && (token_value == FROM_MK)) ln = 2;
                 if ((token_type == CND_TT) && (token_value == IN_COND))
                 {   get_next_token();
                     get_next_token();
                     if ((token_type == SEP_TT) && (token_value == CLOSEB_SEP))
                         ln = 3;
                     put_token_back();
                     put_token_back();
                 }

                 if (ln > 0)
                 {   /*  Old style (Inform 5) objectloops: note that we
                         implement objectloop (a in b) in the old way since
                         this runs through objects in a different order from
                         the new way, and there may be existing Inform code
                         relying on this.                                    */

                     sequence_point_follows = TRUE;
                     AO2 = code_generate(parse_expression(QUANTITY_CONTEXT),
                         QUANTITY_CONTEXT, -1);
                     match_close_bracket();
                     if (ln == 1)
                     {   AO3.type = VARIABLE_OT; AO3.value = 0; AO3.marker = 0;
                         assemble_1_to(get_parent_zc, AO2, AO3);
                         assemble_objcode(get_child_zc, AO3, AO3, -2, TRUE);
                         AO2 = AO3;
                     }
                     if (ln == 3)
                     {   AO3.type = VARIABLE_OT; AO3.value = 0; AO3.marker = 0;
                         assemble_objcode(get_child_zc, AO2, AO3, -2, TRUE);
                         AO2 = AO3;
                     }
                     assemble_store(AO, AO2);
                     assemble_1_branch(jz_zc, AO, ln2 = next_label++, TRUE);
                     assemble_label_no(ln = next_label++);
                     parse_code_block(ln2, ln3 = next_label++, 0);
                     sequence_point_follows = FALSE;
                     assemble_label_no(ln3);
                     assemble_objcode(get_sibling_zc, AO, AO, ln, TRUE);
                     assemble_label_no(ln2);
                     return;
                 }

                 sequence_point_follows = TRUE;
                 AO2.type = SHORT_CONSTANT_OT; AO2.value = 1; AO2.marker = 0;
                 assemble_store(AO, AO2);

                 assemble_label_no(ln = next_label++);
                 ln2 = next_label++;
                 ln3 = next_label++;
                 if (flag)
                 {   put_token_back();
                     put_token_back();
                     sequence_point_follows = TRUE;
                     code_generate(parse_expression(CONDITION_CONTEXT),
                         CONDITION_CONTEXT, ln3);
                     match_close_bracket();
                 }
                 parse_code_block(ln2, ln3, 0);

                 sequence_point_follows = FALSE;
                 assemble_label_no(ln3);
                 assemble_inc(AO);
                 AO2.type = LONG_CONSTANT_OT; AO2.value = no_objects;
                 AO2.marker = NO_OBJS_MV;
                 assemble_2_branch(jg_zc, AO, AO2, ln2, TRUE);
                 assemble_jump(ln);
                 assemble_label_no(ln2);
                 return;

    /*  -------------------------------------------------------------------- */
    /*  (see routine above) ------------------------------------------------ */
    /*  -------------------------------------------------------------------- */

        case PRINT_CODE:
            get_next_token();
            parse_print(FALSE); return;
        case PRINT_RET_CODE:
            get_next_token();
            parse_print(TRUE); return;

    /*  -------------------------------------------------------------------- */
    /*  quit --------------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case QUIT_CODE:      assemble_0(quit_zc); break;

    /*  -------------------------------------------------------------------- */
    /*  read <expression> <expression> [<Routine>] ------------------------- */
    /*  -------------------------------------------------------------------- */

        case READ_CODE:
                 AO.type = VARIABLE_OT; AO.value = 252; AO.marker = 0;
                 assemble_store(AO,
                     code_generate(parse_expression(QUANTITY_CONTEXT),
                                   QUANTITY_CONTEXT, -1));
                 if (version_number > 3)
                 {   AO3.type = SHORT_CONSTANT_OT; AO3.value = 1;AO3.marker = 0;
                     AO4.type = SHORT_CONSTANT_OT; AO4.value = 0;AO4.marker = 0;
                     assemble_3(storeb_zc, AO, AO3, AO4);
                 }
                 AO2 = code_generate(parse_expression(QUANTITY_CONTEXT),
                           QUANTITY_CONTEXT, -1);

                 get_next_token();
                 if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                     put_token_back();
                 else
                 {   if (version_number == 3)
                         error(
"In Version 3 no status-line drawing routine can be given");
                     else
                     {   assembly_operand AO5, AO6;
                         put_token_back();
                         AO5 = parse_expression(CONSTANT_CONTEXT);

                         if (version_number >= 5)
                             assemble_1(call_1n_zc, AO5);
                         else
                         {   AO6.type = VARIABLE_OT; AO6.value = 255;
                             AO6.marker = 0;
                             assemble_1_to(call_1n_zc, AO5, AO6);
                         }
                     }
                 }

                 if (version_number > 3)
                 {   AO3.type = VARIABLE_OT; AO3.value = 255; AO3.marker = 0;
                     assemble_2_to(aread_zc, AO, AO2, AO3);
                 }
                 else assemble_2(sread_zc, AO, AO2);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  remove <expression> ------------------------------------------------ */
    /*  -------------------------------------------------------------------- */

        case REMOVE_CODE:
                 assemble_1(remove_obj_zc,
                     code_generate(parse_expression(QUANTITY_CONTEXT),
                         QUANTITY_CONTEXT, -1));
                 break;

    /*  -------------------------------------------------------------------- */
    /*  restore <label> ---------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case RESTORE_CODE:
                 if (version_number < 5)
                     assemble_0_branch(restore_zc, parse_label(), TRUE);
                 else
                 {   AO.type = VARIABLE_OT; AO.value = 255; AO.marker = 0;
                     AO2.type = SHORT_CONSTANT_OT; AO2.value = 2;
                     AO2.marker = 0;
                     assemble_0_to(restore_zc, AO);
                     assemble_2_branch(je_zc, AO, AO2, parse_label(), TRUE);
                 }
                 break;

    /*  -------------------------------------------------------------------- */
    /*  return [<expression>] ---------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case RETURN_CODE:
                 get_next_token();
                 if ((token_type == SEP_TT) && (token_value == SEMICOLON_SEP))
                 {   assemble_0(rtrue_zc); return; }
                 put_token_back();
                 AO = code_generate(parse_expression(QUANTITY_CONTEXT),
                     QUANTITY_CONTEXT, -1);
                 if ((AO.type == SHORT_CONSTANT_OT) && (AO.value == 0)
                     && (AO.marker == 0))
                 {   assemble_0(rfalse_zc); break; }
                 if ((AO.type == SHORT_CONSTANT_OT) && (AO.value == 1)
                     && (AO.marker == 0))
                 {   assemble_0(rtrue_zc); break; }
                 if ((AO.type == VARIABLE_OT) && (AO.value == 0))
                 {   assemble_0(ret_popped_zc); break; }
                 assemble_1(ret_zc, AO);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  rfalse ------------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case RFALSE_CODE:  assemble_0(rfalse_zc); break;

    /*  -------------------------------------------------------------------- */
    /*  rtrue -------------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case RTRUE_CODE:   assemble_0(rtrue_zc); break;

    /*  -------------------------------------------------------------------- */
    /*  save <label> ------------------------------------------------------- */
    /*  -------------------------------------------------------------------- */

        case SAVE_CODE:
                 if (version_number < 5)
                     assemble_0_branch(save_zc, parse_label(), TRUE);
                 else
                 {   AO.type = VARIABLE_OT; AO.value = 255; AO.marker = 0;
                     assemble_0_to(save_zc, AO);
                     assemble_1_branch(jz_zc, AO, parse_label(), FALSE);
                 }
                 break;

    /*  -------------------------------------------------------------------- */
    /*  spaces <expression> ------------------------------------------------ */
    /*  -------------------------------------------------------------------- */

        case SPACES_CODE:
                 AO = code_generate(parse_expression(QUANTITY_CONTEXT),
                     QUANTITY_CONTEXT, -1);
                 AO2.type = VARIABLE_OT; AO2.value = 255; AO2.marker = 0;

                 assemble_store(AO2, AO);

                 AO.type = SHORT_CONSTANT_OT; AO.value = 32; AO.marker = 0;
                 AO3.type = SHORT_CONSTANT_OT; AO3.value = 1; AO3.marker = 0;

                 assemble_2_branch(jl_zc, AO2, AO3, ln = next_label++, TRUE);
                 assemble_label_no(ln2 = next_label++);
                 assemble_1(print_char_zc, AO);
                 assemble_dec(AO2);
                 assemble_1_branch(jz_zc, AO2, ln2, FALSE);
                 assemble_label_no(ln);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  string <expression> <literal-string> ------------------------------- */
    /*  -------------------------------------------------------------------- */

        case STRING_CODE:
                 AO.type = SHORT_CONSTANT_OT; AO.value = 0; AO.marker = 0;
                 AO2.type = SHORT_CONSTANT_OT; AO2.value = 12; AO2.marker = 0;
                 AO3.type = VARIABLE_OT; AO3.value = 252; AO3.marker = 0;
                 assemble_2_to(loadw_zc, AO, AO2, AO3);
                 AO2 = code_generate(parse_expression(QUANTITY_CONTEXT),
                     QUANTITY_CONTEXT, -1);
                 get_next_token();
                 if (token_type == DQ_TT)
                 {   AO4.value = compile_string(token_text, TRUE, TRUE);
                     AO4.marker = 0;
                     AO4.type = LONG_CONSTANT_OT;
                 }
                 else
                 {   put_token_back();
                     AO4 = parse_expression(CONSTANT_CONTEXT);
                 }
                 assemble_3(storew_zc, AO3, AO2, AO4);
                 break;

    /*  -------------------------------------------------------------------- */
    /*  style roman/reverse/bold/underline/fixed --------------------------- */
    /*  -------------------------------------------------------------------- */

        case STYLE_CODE:
                 if (version_number==3)
                 {   error(
"The 'style' statement cannot be used for Version 3 games");
                     panic_mode_error_recovery();
                     break;
                 }

                 misc_keywords.enabled = TRUE;
                 get_next_token();
                 misc_keywords.enabled = FALSE;
                 if ((token_type != MISC_KEYWORD_TT)
                     || ((token_value != ROMAN_MK)
                         && (token_value != REVERSE_MK)
                         && (token_value != BOLD_MK)
                         && (token_value != UNDERLINE_MK)
                         && (token_value != FIXED_MK)))
                 {   ebf_error(
"'roman', 'bold', 'underline', 'reverse' or 'fixed'",
                         token_text);
                     panic_mode_error_recovery();
                     break;
                 }

                 AO.type = SHORT_CONSTANT_OT; AO.marker = 0;
                 switch(token_value)
                 {   case ROMAN_MK: AO.value = 0; break;
                     case REVERSE_MK: AO.value = 1; break;
                     case BOLD_MK: AO.value = 2; break;
                     case UNDERLINE_MK: AO.value = 4; break;
                     case FIXED_MK: AO.value = 8; break;
                 }
                 assemble_1(set_text_style_zc, AO); break;

    /*  -------------------------------------------------------------------- */
    /*  switch (<expression>) <codeblock> ---------------------------------- */
    /*  -------------------------------------------------------------------- */

        case SWITCH_CODE:
                 match_open_bracket();
                 AO = code_generate(parse_expression(QUANTITY_CONTEXT),
                     QUANTITY_CONTEXT, -1);
                 match_close_bracket();

                 AO2.type = VARIABLE_OT; AO2.value = 255; AO2.marker =  0;
                 assemble_store(AO2, AO);

                 parse_code_block(ln = next_label++, continue_label, 1);
                 assemble_label_no(ln);
                 return;

    /*  -------------------------------------------------------------------- */
    /*  while (<condition>) <codeblock> ------------------------------------ */
    /*  -------------------------------------------------------------------- */

        case WHILE_CODE:
                 assemble_label_no(ln = next_label++);
                 match_open_bracket();

                 code_generate(parse_expression(CONDITION_CONTEXT),
                     CONDITION_CONTEXT, ln2 = next_label++);
                 match_close_bracket();

                 parse_code_block(ln2, ln, 0);
                 sequence_point_follows = FALSE;
                 assemble_jump(ln);
                 assemble_label_no(ln2);
                 return;

    /*  -------------------------------------------------------------------- */

        case SDEFAULT_CODE:
                 error("'default' without matching 'switch'"); break;
        case ELSE_CODE:
                 error("'else' without matching 'if'"); break;
        case UNTIL_CODE:
                 error("'until' without matching 'do'");
                 panic_mode_error_recovery(); return;
    }

    StatementTerminator:

    get_next_token();
    if ((token_type != SEP_TT) || (token_value != SEMICOLON_SEP))
    {   ebf_error("';'", token_text);
        put_token_back();
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_states_vars(void)
{
}

extern void states_begin_pass(void)
{
}

extern void states_allocate_arrays(void)
{
}

extern void states_free_arrays(void)
{
}

/* ========================================================================= */
