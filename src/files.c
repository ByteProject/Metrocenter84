/* ------------------------------------------------------------------------- */
/*   "files" : File handling for source code, the transcript file and the    */
/*             debugging information file; file handling and splicing of     */
/*             the output file.                                              */
/*                                                                           */
/*             Note that filenaming conventions are left to the top-level    */
/*             routines in "inform.c", since they are tied up with ICL       */
/*             settings and are very host OS-dependent.                      */
/*                                                                           */
/*   Part of Inform 6.1                                                      */
/*   copyright (c) Graham Nelson 1993, 1994, 1995, 1996, 1997, 1998          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include "header.h"

int input_file;                         /* Number of source files so far     */

int32 total_chars_read;                 /* Characters read in (from all
                                           source files put together)        */

static int checksum_low_byte,           /* For calculating the Z-machine's   */
           checksum_high_byte;          /* "verify" checksum                 */

/* ------------------------------------------------------------------------- */
/*   Most of the information about source files is kept by "lexer.c"; this   */
/*   level is only concerned with file names and handles.                    */
/* ------------------------------------------------------------------------- */

FileId InputFiles[MAX_SOURCE_FILES];    /*  Ids for all the source files     */

/* ------------------------------------------------------------------------- */
/*   File handles and names for temporary files.                             */
/* ------------------------------------------------------------------------- */

FILE *Temp1_fp=NULL, *Temp2_fp=NULL,  *Temp3_fp=NULL;
char Temp1_Name[128], Temp2_Name[128], Temp3_Name[128];

/* ------------------------------------------------------------------------- */
/*   Opening and closing source code files                                   */
/* ------------------------------------------------------------------------- */

extern void load_sourcefile(char *filename_given, int same_directory_flag)
{
    /*  Meaning: open a new file of Inform source.  (The lexer picks up on
        this by noticing that input_file has increased.)                     */

    char name[128]; int x = 0; FILE *handle;

    if (input_file == MAX_SOURCE_FILES)
        fatalerror("Program contains too many source files: \
increase #define MAX_SOURCE_FILES");

    do
    {   x = translate_in_filename(x, name, filename_given, same_directory_flag,
                (input_file==0)?1:0);
        handle = fopen(name,"r");
    } while ((handle == NULL) && (x != 0));

    strcpy(InputFiles[input_file].filename, name);

    if (debugfile_switch)
    {   write_debug_byte(FILE_DBR); write_debug_byte(input_file + 1);
        write_debug_string(filename_given);
        write_debug_string(name);
    }

    InputFiles[input_file].handle = handle;
    if (InputFiles[input_file].handle==NULL)
        fatalerror_named("Couldn't open source file", name);

    if (line_trace_level > 0) printf("\nOpening file \"%s\"\n",name);

    input_file++;
}

static void close_sourcefile(int file_number)
{
    if (InputFiles[file_number-1].handle == NULL) return;

    /*  Close this file.  */

    if (ferror(InputFiles[file_number-1].handle))
        fatalerror_named("I/O failure: couldn't read from source file",
            InputFiles[file_number-1].filename);

    fclose(InputFiles[file_number-1].handle);

    InputFiles[file_number-1].handle = NULL;

    if (line_trace_level > 0) printf("\nClosing file\n");
}

extern void close_all_source(void)
{   int i;
    for (i=0; i<input_file; i++) close_sourcefile(i+1);
}

/* ------------------------------------------------------------------------- */
/*   Feeding source code up into the lexical analyser's buffer               */
/*   (see "lexer.c" for its specification)                                   */
/* ------------------------------------------------------------------------- */

extern int file_load_chars(int file_number, char *buffer, int length)
{
    int read_in; FILE *handle;

    if (file_number-1 > input_file)
    {   buffer[0] = 0; return 1; }

    handle = InputFiles[file_number-1].handle;
    if (handle == NULL)
    {   buffer[0] = 0; return 1; }

    read_in = fread(buffer, 1, length, handle);
    total_chars_read += read_in;

    if (read_in == length) return length;

    close_sourcefile(file_number);

    if (file_number == 1)
    {   buffer[read_in]   = 0;
        buffer[read_in+1] = 0;
        buffer[read_in+2] = 0;
        buffer[read_in+3] = 0;
    }
    else
    {   buffer[read_in]   = '\n';
        buffer[read_in+1] = ' ';
        buffer[read_in+2] = ' ';
        buffer[read_in+3] = ' ';
    }

    return(-(read_in+4));
}

/* ------------------------------------------------------------------------- */
/*   Final assembly and output of the story file/module.                     */
/* ------------------------------------------------------------------------- */

FILE *sf_handle;

static void sf_put(int c)
{
    /*  The checksum is the unsigned sum mod 65536 of the bytes in the
        story file from 0x0040 (first byte after header) to the end.

        The link data does not contribute to the checksum of a module.       */

    checksum_low_byte += c;
    if (checksum_low_byte>=256)
    {   checksum_low_byte-=256;
        if (++checksum_high_byte==256) checksum_high_byte=0;
    }

    fputc(c, sf_handle);
}

extern void output_file(void)
{   FILE *fin; char new_name[128];
    int32 length, blanks=0, size, i, j;

    /*  Enter the length information into the header.                        */

    length=((int32) Write_Strings_At) + static_strings_extent;
    if (module_switch) length += link_data_size +
                                 zcode_backpatch_size +
                                 zmachine_backpatch_size;

    while ((length%length_scale_factor)!=0) { length++; blanks++; }
    length=length/length_scale_factor;
    zmachine_paged_memory[26]=(length & 0xff00)/0x100;
    zmachine_paged_memory[27]=(length & 0xff);

    /*  To assist interpreters running a paged virtual memory system, Inform
        writes files which are padded with zeros to the next multiple of
        0.5K.  This calculates the number of bytes of padding needed:        */

    while (((length_scale_factor*length)+blanks-1)%512 != 511) blanks++;

    /*  Write a copy of the header into the debugging information file
        (mainly so that it can be used to identify which story file matches
        with which debugging info file)                                      */

    if (debugfile_switch)
    {   write_debug_byte(HEADER_DBR);
        for (i=0; i<64; i++)
            write_debug_byte((int) (zmachine_paged_memory[i]));
    }

    translate_out_filename(new_name, Code_Name);

    sf_handle = fopen(new_name,"wb");
    if (sf_handle == NULL)
        fatalerror_named("Couldn't open output file", new_name);

#ifdef MAC_MPW
    /*  Set the type and creator to Andrew Plotkin's MaxZip, a popular
        Z-code interpreter on the Macintosh  */

    if (!module_switch) fsetfileinfo(new_name, 'mxZR', 'ZCOD');
#endif

    /*  (1)  Output the paged memory.                                        */

    for (i=0;i<64;i++)
        fputc(zmachine_paged_memory[i], sf_handle);
    size = 64;
    checksum_low_byte = 0;
    checksum_high_byte = 0;

    for (i=64; i<Write_Code_At; i++)
    {   sf_put(zmachine_paged_memory[i]); size++;
    }

    /*  (2)  Output the compiled code area.                                  */

    if (temporary_files_switch)
    {   fclose(Temp2_fp);
        fin=fopen(Temp2_Name,"rb");
        if (fin==NULL)
            fatalerror("I/O failure: couldn't reopen temporary file 2");
    }

    j=0;
    if (!module_switch)
    for (i=0; i<zcode_backpatch_size; i=i+3)
    {   int long_flag = TRUE;
        int32 offset
            = 256*read_byte_from_memory_block(&zcode_backpatch_table, i+1)
              + read_byte_from_memory_block(&zcode_backpatch_table, i+2);
        backpatch_error_flag = FALSE;
        backpatch_marker
            = read_byte_from_memory_block(&zcode_backpatch_table, i);
        if (backpatch_marker >= 0x80) long_flag = FALSE;
        backpatch_marker &= 0x7f;
        offset = offset + (backpatch_marker/32)*0x10000;
        backpatch_marker &= 0x1f;

        while (j<offset)
        {   size++;
            sf_put((temporary_files_switch)?fgetc(fin):
                  read_byte_from_memory_block(&zcode_area, j));
            j++;
        }

        if (long_flag)
        {   int32 v = (temporary_files_switch)?fgetc(fin):
                read_byte_from_memory_block(&zcode_area, j);
            v = 256*v + ((temporary_files_switch)?fgetc(fin):
                read_byte_from_memory_block(&zcode_area, j+1));
            v = backpatch_value(v);
            sf_put(v/256); sf_put(v%256);
            size += 2; j += 2;
        }
        else
        {   int32 v = (temporary_files_switch)?fgetc(fin):
                read_byte_from_memory_block(&zcode_area, j);
            v = backpatch_value(v);
            sf_put(v);
            size++; j++;
        }

        if (backpatch_error_flag)
        {   printf("*** %s  zcode offset=%08lx  backpatch offset=%08lx ***\n",
                (long_flag)?"long":"short", (long int) j, (long int) i);
        }
    }

    while (j<zmachine_pc)
    {   size++;
        sf_put((temporary_files_switch)?fgetc(fin):
            read_byte_from_memory_block(&zcode_area, j));
        j++;
    }

    if (temporary_files_switch)
    {   if (ferror(fin))
            fatalerror("I/O failure: couldn't read from temporary file 2");
        fclose(fin);
    }

    /*  (3)  Output any null bytes (required to reach a packed address)
             before the strings area.                                        */

    while (size<Write_Strings_At) { sf_put(0); size++; }

    /*  (4)  Output the static strings area.                                 */

    if (temporary_files_switch)
    {   fclose(Temp1_fp);
        fin=fopen(Temp1_Name,"rb");
        if (fin==NULL)
            fatalerror("I/O failure: couldn't reopen temporary file 1");
        for (i=0; i<static_strings_extent; i++) sf_put(fgetc(fin));
        if (ferror(fin))
            fatalerror("I/O failure: couldn't read from temporary file 1");
        fclose(fin);
        remove(Temp1_Name); remove(Temp2_Name);
    }
    else
        for (i=0; i<static_strings_extent; i++)
            sf_put(read_byte_from_memory_block(&static_strings_area,i));

    /*  (5)  Output the linking data table (in the case of a module).        */

    if (temporary_files_switch)
    {   if (module_switch)
        {   fclose(Temp3_fp);
            fin=fopen(Temp3_Name,"rb");
            if (fin==NULL)
                fatalerror("I/O failure: couldn't reopen temporary file 3");
            for (j=0; j<link_data_size; j++) sf_put(fgetc(fin));
            if (ferror(fin))
                fatalerror("I/O failure: couldn't read from temporary file 3");
            fclose(fin);
            remove(Temp3_Name);
        }
    }
    else
        if (module_switch)
            for (i=0; i<link_data_size; i++)
                sf_put(read_byte_from_memory_block(&link_data_area,i));

    if (module_switch)
    {   for (i=0; i<zcode_backpatch_size; i++)
            sf_put(read_byte_from_memory_block(&zcode_backpatch_table, i));
        for (i=0; i<zmachine_backpatch_size; i++)
            sf_put(read_byte_from_memory_block(&zmachine_backpatch_table, i));
    }

    /*  (6)  Output null bytes to reach a multiple of 0.5K.                  */

    while (blanks>0) { sf_put(0); blanks--; }

    if (ferror(sf_handle))
        fatalerror("I/O failure: couldn't write to story file");

    fseek(sf_handle, 28, SEEK_SET);
    fputc(checksum_high_byte, sf_handle);
    fputc(checksum_low_byte, sf_handle);

    if (ferror(sf_handle))
      fatalerror("I/O failure: couldn't backtrack on story file for checksum");

    fclose(sf_handle);

#ifdef ARCHIMEDES
    {   char settype_command[128];
        sprintf(settype_command, "settype %s %s",
            new_name, riscos_file_type());
        system(settype_command);
    }
#endif
#ifdef MAC_FACE
     if (module_switch)
         InformFiletypes (new_name, INF_MODULE_TYPE);
     else
         InformFiletypes (new_name, INF_ZCODE_TYPE);
#endif
}

/* ------------------------------------------------------------------------- */
/*   Output the text transcript file (only called if there is to be one).    */
/* ------------------------------------------------------------------------- */

FILE *transcript_file_handle; int transcript_open;

extern void write_to_transcript_file(char *text)
{   fputs(text, transcript_file_handle);
    fputc('\n', transcript_file_handle);
}

extern void open_transcript_file(char *what_of)
{   char topline_buffer[256];

    transcript_file_handle = fopen(Transcript_Name,"w");
    if (transcript_file_handle==NULL)
        fatalerror_named("Couldn't open transcript file",
        Transcript_Name);

    transcript_open = TRUE;

    sprintf(topline_buffer, "Transcript of the text of \"%s\"\n\
[From %s]\n", what_of, banner_line);
    write_to_transcript_file(topline_buffer);
}

extern void abort_transcript_file(void)
{   if (transcript_switch && transcript_open)
        fclose(transcript_file_handle);
    transcript_open = FALSE;
}

extern void close_transcript_file(void)
{   char botline_buffer[256];
    char sn_buffer[7];

    write_serial_number(sn_buffer);
    sprintf(botline_buffer, "\n[End of transcript: release %d.%s]\n",
        release_number, sn_buffer);
    write_to_transcript_file(botline_buffer);

    if (ferror(transcript_file_handle))
        fatalerror("I/O failure: couldn't write to transcript file");
    fclose(transcript_file_handle);
    transcript_open = FALSE;

#ifdef ARCHIMEDES
    {   char settype_command[128];
        sprintf(settype_command, "settype %s text",
            Transcript_Name);
        system(settype_command);
    }
#endif
#ifdef MAC_FACE
    InformFiletypes (Transcript_Name, INF_TEXT_TYPE);
#endif
}

/* ------------------------------------------------------------------------- */
/*   Access to the debugging information file.                               */
/* ------------------------------------------------------------------------- */

static FILE *Debug_fp;                 /* Handle of debugging info file      */

extern void open_debug_file(void)
{   Debug_fp=fopen(Debugging_Name,"wb");
    if (Debug_fp==NULL)
       fatalerror_named("Couldn't open debugging information file",
           Debugging_Name);
}

extern void close_debug_file(void)
{   fputc(EOF_DBR, Debug_fp);
    if (ferror(Debug_fp))
        fatalerror("I/O failure: can't write to debugging info file");
    fclose(Debug_fp);
#ifdef MAC_FACE
    InformFiletypes (Debugging_Name, INF_DEBUG_TYPE);
#endif
}

extern void write_debug_byte(int i)
{
    /*  All output to the debugging file is funneled through this routine    */

    fputc(i,Debug_fp);
    if (ferror(Debug_fp))
        fatalerror("I/O failure: can't write to debugging info file");
}

extern void write_debug_string(char *s)
{
    /*  Write a null-terminated string into the debugging file.              */

    int i;
    for (i=0; s[i]!=0; i++)
        write_debug_byte((int) s[i]);
    write_debug_byte(0);
}

extern void write_debug_address(int32 i)
{
    /*  Write a 3-byte address (capable of being a byte address in a story
        file whose size may be as much as 512K) into the debugging file.
        Also used for character positions in source files.                   */

    write_debug_byte((int)((i/256)/256));
    write_debug_byte((int)((i/256)%256));
    write_debug_byte((int)(i%256));
}

/* ------------------------------------------------------------------------- */
/*   There is a standard four-byte format to express code line refs in the   */
/*   debugging file: if X is a dbgl,                                         */
/*                                                                           */
/*       X.b1 = file number (input files to Inform are numbered from 1 in    */
/*              order of being opened)                                       */
/*       X.b2, X.b3 = high byte, low byte of line in this file (numbered     */
/*              from 1)                                                      */
/*       X.cc = character position in current line                           */
/* ------------------------------------------------------------------------- */

extern void write_dbgl(dbgl x)
{   write_debug_byte(x.b1);
    write_debug_byte(x.b2); write_debug_byte(x.b3);
    write_debug_byte(x.cc);
}

extern void begin_debug_file(void)
{   open_debug_file();

    /* DEBF == Debugging File (sorry) */
    write_debug_byte(0xDE); write_debug_byte(0xBF);

    /* Debugging file format version number */
    write_debug_byte(0); write_debug_byte(0);

    /* Identify ourselves */
    write_debug_byte(VNUMBER/256); write_debug_byte(VNUMBER%256);
}

/* ------------------------------------------------------------------------- */
/*  Temporary storage files:                                                 */
/*                                                                           */
/*      Temp file 1 is used to hold the static strings area, as compiled     */
/*                2 to hold compiled routines of Z-code                      */
/*                3 to hold the link data table (but only for modules)       */
/*                                                                           */
/*  (Though annoying, this procedure typically saves about 200K of memory,   */
/*  an important point for Amiga and sub-386 PC users of Inform)             */
/* ------------------------------------------------------------------------- */

extern void open_temporary_files(void)
{   translate_temp_filename(1);
    Temp1_fp=fopen(Temp1_Name,"wb");
    if (Temp1_fp==NULL) fatalerror_named("Couldn't open temporary file 1",
        Temp1_Name);
    translate_temp_filename(2);
    Temp2_fp=fopen(Temp2_Name,"wb");
    if (Temp2_fp==NULL) fatalerror_named("Couldn't open temporary file 2",
        Temp2_Name);

    if (!module_switch) return;
    translate_temp_filename(3);
    Temp3_fp=fopen(Temp3_Name,"wb");
    if (Temp3_fp==NULL) fatalerror_named("Couldn't open temporary file 3",
        Temp3_Name);
}

extern void check_temp_files(void)
{
    if (ferror(Temp1_fp))
        fatalerror("I/O failure: couldn't write to temporary file 1");
    if (ferror(Temp2_fp))
        fatalerror("I/O failure: couldn't write to temporary file 2");
    if (module_switch && ferror(Temp3_fp))
        fatalerror("I/O failure: couldn't write to temporary file 3");
}

extern void remove_temp_files(void)
{   if (Temp1_fp != NULL) fclose(Temp1_fp);
    if (Temp2_fp != NULL) fclose(Temp2_fp);
    remove(Temp1_Name); remove(Temp2_Name);
    if (module_switch)
    {   if (Temp3_fp != NULL) fclose(Temp3_fp);
        remove(Temp3_Name);
    }
}

/* ========================================================================= */
/*   Data structure management routines                                      */
/* ------------------------------------------------------------------------- */

extern void init_files_vars(void)
{   malloced_bytes = 0;
    checksum_low_byte = 0;
    checksum_high_byte = 0;
    transcript_open = FALSE;
}

extern void files_begin_prepass(void)
{   input_file = 0;
}

extern void files_begin_pass(void)
{   total_chars_read=0;
    if (temporary_files_switch)
        open_temporary_files();
}

extern void files_allocate_arrays(void)
{
}

extern void files_free_arrays(void)
{
}

/* ========================================================================= */
