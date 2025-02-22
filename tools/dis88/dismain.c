static char *sccsid =  "@(#) dismain.c, Ver. 2.1 created 00:00:00 87/09/01";

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file  contains  the source  code for the  machine- *
  * independent  portions of a disassembler  program to run *
  * in a Unix (System III) environment.  It expects, as its *
  * input, a file in standard a.out format, optionally con- *
  * taining symbol table information.  If a symbol table is *
  * present, it will be used in the disassembly; otherwise, *
  * all address references will be literal (absolute).      *
  *                                                         *
  * The disassembler  program was originally written for an *
  * Intel 8088 CPU.  However, all details of the actual CPU *
  * architecture are hidden in three machine-specific files *
  * named  distabs.c,  dishand.c,  and disfp.c  (the latter *
  * file is specific to the 8087 numeric co-processor). The *
  * code in this file is generic,  and should require mini- *
  * mal revision if a different CPU is to be targeted. If a *
  * different version of Unix is to be targeted, changes to *
  * this file may be necessary, and if a completely differ- *
  * ent OS is to be targeted, all bets are off.             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dis.h"              /* Disassembler declarations  */

extern char *release;         /* Contains release string    */
static char *IFILE = NULL;    /* Points to input file name  */
static char *OFILE = NULL;    /* Points to output file name */
static char *PRG;             /* Name of invoking program   */
static unsigned long zcount;  /* Consecutive "0" byte count */
int objflg = 0;               /* Flag: output object bytes  */
int force = 0;		      /* Flag: override some checks */

#ifdef VENIX
#define MAXSTR (10*1024)
char strtab[MAXSTR];
#endif

#define unix 1
#define i8086 1
#define ibmpc 1

#if unix && i8086 && ibmpc    /* Set the CPU identifier     */
static int cpuid = 1;
#else
static int cpuid = 0;
#endif

_PROTOTYPE(static void usage, (char *s ));
_PROTOTYPE(static void fatal, (char *s, char *t ));
_PROTOTYPE(static void zdump, (unsigned long beg ));
_PROTOTYPE(static void prolog, (void));
_PROTOTYPE(static void distext, (void));
_PROTOTYPE(static void disdata, (void));
_PROTOTYPE(static void disbss, (void));

_PROTOTYPE(static char *invoker, (char *s));
_PROTOTYPE(static int objdump, (char *c));
_PROTOTYPE(static char *getlab, (int type));
_PROTOTYPE(static void prolog, (void));

 /* * * * * * * MISCELLANEOUS UTILITY FUNCTIONS * * * * * * */

static void
usage(s)
   register char *s;
{
   fprintf(stderr,"Usage: %s [-o] ifile [ofile]\n",s);
   exit(-1);
}

static void
fatal(s,t)
   register char *s, *t;
{
   fprintf(stderr,"%s: %s\n",s,t);
   exit(-1);
}

static void
zdump(beg)
   unsigned long beg;
{
   beg = PC - beg;
   if (beg > 1L)
      printf("\t.zerow\t%ld\n",(beg >> 1));
   if (beg & 1L)
      printf("\t.byte\t0\n");
}

static char *
invoker(s)
   register char *s;
{
   register int k;

   k = strlen(s);

   while (k--)
      if (s[k] == '/')
         {
         s += k;
         ++s;
         break;
         }

   return (s);
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This rather tricky routine supports the disdata() func- *
  * tion.  Its job is to output the code for a sequence  of *
  * data bytes whenever the object buffer is full,  or when *
  * a symbolic label is to be output. However, it must also *
  * keep track of  consecutive  zero words so that  lengthy *
  * stretches of null data can be  compressed by the use of *
  * an  appropriate  assembler  pseudo-op.  It does this by *
  * setting and testing a file-wide  flag which counts suc- *
  * cessive full buffers of null data. The function returns *
  * a logical  TRUE value if it outputs  anything,  logical *
  * FALSE otherwise.  (This enables disdata()  to determine *
  * whether to output a new  synthetic  label when there is *
  * no symbol table.)                                       *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
objdump(c)

   register char *c;

{/* * * * * * * * * * START OF  objdump() * * * * * * * * * */

   register int k,j;
   int retval = 0;

   if (objptr == OBJMAX)
      {
      for (k = 0; k < OBJMAX; ++k)
         if (objbuf[k])
            break;
      if (k == OBJMAX)
         {
         zcount += k;
         objptr = 0;
         if (c == NULL)
            return (retval);
         }
      }

   if (zcount)
      {
      printf("\t.zerow\t%ld\n",(zcount >> 1));
      ++retval;
      zcount = 0L;
      }

   if (objptr)
      {
      printf("\t.byte\t");
      ++retval;
      }
   else
      return (retval);

   for (k = 0; k < objptr; ++k)
      {
#ifdef VENIX
      printf("%d",objbuf[k]);
#else
      printf("$%02.2x",objbuf[k]);
#endif
      if (k < (objptr - 1))
         putchar(',');
      }

   for (k = objptr; k < OBJMAX; ++k)
      printf("    ");

   printf("    | \"");

   for (k = 0; k < objptr; ++k)
      {
      if (objbuf[k] > ' ' && objbuf[k] <= '~' )
         putchar(objbuf[k]);
      else switch(objbuf[k])
         {
	 case '\t': printf("\\t"); break;
	 case '\n': printf("\\n"); break;
	 case '\f': printf("\\f"); break;
	 case '\r': printf("\\r"); break;
	 default:   putchar('.'); break;
         }
      }
   printf("\"\n");

   objptr = 0;

   return (retval);

}/* * * * * * * * * *  END OF  objdump()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  routine,  called  at the  beginning  of the input *
  * cycle for each object byte,  and before any interpreta- *
  * tion is  attempted,  searches  the symbol table for any *
  * symbolic  name with a value  corresponding  to the cur- *
  * rent PC and a type  corresponding  to the segment  type *
  * (i.e.,  text, data, or bss) specified by the function's *
  * argument. If any such name is found, a pointer to it is *
  * returned; otherwise, a NULL pointer is returned.        *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static char *
getlab(type)
register int type;
{/* * * * * * * * * *  START OF getlab()  * * * * * * * * * */

   register int k;
   static char b[48], c[32];

   if (symptr < 0) {
      if ((type == N_TEXT)
       || ((type == N_DATA) && ( ! objptr ) && ( ! zcount )))
         {
         if (type == N_TEXT)
            sprintf(b,"T%05.5lx:",PC);
         else
            sprintf(b,"D%05.5lx:",PC);
         return (b);
         }
      else
         return (NULL);
   }

   for (k = 0; k <= symptr; ++k)
      if ((symtab[k].n_value == PC)
       && (N_TYP(symtab[k]) == type))
         {
         sprintf(b,"%s:\n",getnam(k));
         if (objflg && (type != N_TEXT))
            sprintf(c,"| %05.5lx\n",PC);
         strcat(b,c);
         return (b);
         }

   return (NULL);

}/* * * * * * * * * * * END OF getlab() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This routine  performs a preliminary scan of the symbol *
  * table,  before disassembly begins, and outputs declara- *
  * tions of globals and constants.                         *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
prolog()

{/* * * * * * * * * *  START OF prolog()  * * * * * * * * * */

   register int j, flag;

   fflush(stdout);

   if (symptr < 0)
      return;

   for (j = flag = 0; j <= symptr; ++j)
      if (N_IS_EXT(symtab[j])) {
         if (!N_IS_COMM(symtab[j]))
            {
            char *c = getnam(j);
            printf("\t.globl\t%s",c);
            if (++flag == 1)
               {
               putchar('\t');
               if (strlen(c) < 8)
                  putchar('\t');
               printf("| Internal global\n");
               }
            else
               putchar('\n');
            }
         else
            if (symtab[j].n_value)
               {
               char *c = getnam(j);
               printf("\t.comm\t%s,0x%08.8x",c,
                symtab[j].n_value);
               if (++flag == 1)
                  printf("\t| Internal global\n");
               else
                  putchar('\n');
               }
      }

   if (flag)
      putchar('\n');
   fflush(stdout);

   for (j = flag = 0; j <= relptr; ++j)
#ifdef S_BSS
   /*
    * Venix doesn't tag segments like this, so it's unclear what to do.
    * For now, skip it the test.
    * XXX
    */
      if (R_IDX(relo[j]) < S_BSS)
#else
      if (relo[j].r_extern)
#endif
         {
	 char *c = getnam(R_IDX(relo[j]));
         ++flag;
         printf("\t.globl\t%s",c);
         putchar('\t');
         if (strlen(c) < 8)
            putchar('\t');
         printf("| Undef: %05.5x\n",relo[j].r_vaddr);
         }

   if (flag)
      putchar('\n');
   fflush(stdout);

   for (j = flag = 0; j <= symptr; ++j)
      if (N_TYP(symtab[j]) == N_ABS)
         {
         char *c = getnam(j);
         printf("%s=0x%08.8x",c,symtab[j].n_value);
         if (++flag == 1)
            {
            printf("\t\t");
            if (strlen(c) < 5)
               putchar('\t');
            printf("| Literal\n");
            }
         else
            putchar('\n');
         }

   if (flag)
      putchar('\n');
   fflush(stdout);

}/* * * * * * * * * * * END OF prolog() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This function is  responsible  for  disassembly  of the *
  * object file's text segment.                             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
distext()

{/* * * * * * * * * * START OF  distext() * * * * * * * * * */

   char *c;
   register int j;
   register void (*f)();

#ifdef VENIX	/* venix doesn't have hdrlen */
   for (j = 0; j < (int)sizeof(HDR); ++j)
      getchar();
#else
   for (j = 0; j < (int)(HDR.a_hdrlen); ++j)
      getchar();
#endif

   printf("| %s, %s\n\n",PRG,release);

   printf("| @(");

   printf("#)\tDisassembly of %s",IFILE);


   if (symptr < 0)
      printf(" (no symbols)\n\n");
   else
      printf("\n\n");

#ifdef VENIX
   if (HDR.a_magic == OMAGIC)
	   printf("| OMAGIC format\n");
   if (HDR.a_magic == NMAGIC)
	   printf("| NMAGIC format\n");
   if (HDR.a_stack)
	   printf("| Stack size for -z binary: 0x%x paragraphs\n", HDR.a_stack);
   printf("| %d bytes text\n", HDR.a_text);
   printf("| %d bytes data\n", HDR.a_data);
   printf("| %d bytes bss\n", HDR.a_bss);
   printf("| %d bytes symbols\n", HDR.a_syms);
   printf("| %d bytes text reloc bytes\n", HDR.a_trsize);
   printf("| %d bytes data reloc bytes\n", HDR.a_drsize);
   printf("| %#x entry\n", HDR.a_entry);
#else
   if (HDR.a_flags & A_EXEC)
      printf("| File is executable\n\n");

   if (HDR.a_flags & A_SEP)
      printf("| File has split I/D space\n\n");
#endif

   prolog();

   printf("\t.text\t\t\t| loc = %05.5lx, size = %05.5x\n\n",
    PC,HDR.a_text);
   fflush(stdout);

   segflg = 0;

   for (PC = 0L; PC < HDR.a_text; ++PC)
      {
      j = getchar();
      oops:;
      if( j == EOF ) break;
      j &= 0xFF;
      if ((j == 0) && ((PC + 1L) == HDR.a_text))
         {
         ++PC;
         break;
         }
      if ((c = getlab(N_TEXT)) != NULL)
         printf("%s",c);
      else
	 printf("%#lx",PC);
      if( j>=0 && j<256 )
      {
         f = optab[j].func;
         (*f)(j);
      }
      fflush(stdout);
      /*
       * hack here to cope with odd routies ending in
       * 0x00 which disassembles to crap. So skip it.
       * c3 is ret, and 0x34 in binaries is the end
       * of int $f1 to exit... It might be better to
       * check if the byte is simply 0.
       */
      if ((HDR.a_stack && PC == 0x34) || (j == 0xc3 && (PC & 1) == 0)) {
	      j = getchar();
	      PC++;
	      if (getlab(N_TEXT))
		      goto oops;
	      printf("\t.byte $%02x\n", j);
      }
      }

}/* * * * * * * * * *  END OF  distext()  * * * * * * * * * */

int
Fetch()
{
   int p;
   ++PC;
//   if( symptr>=0 && getlab(N_TEXT) != NULL ) { --PC; return -1; }
   
/* #define FETCH(p)  ++PC; p = getchar() & 0xff; objbuf[objptr++] = p */
   p = getchar();
   objbuf[objptr++] = p;
   return p;
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  function  handles the object file's data segment. *
  * There is no good way to disassemble a data segment, be- *
  * cause it is  impossible  to tell,  from the object code *
  * alone,  what each data byte refers to.  If it refers to *
  * an external symbol,  the reference can be resolved from *
  * the relocation table, if there is one.  However,  if it *
  * refers to a static symbol,  it cannot be  distinguished *
  * from numeric, character, or other pointer data. In some *
  * cases,  one might make a semi-educated  guess as to the *
  * nature of the data,  but such  guesses  are  inherently *
  * haphazard,  and they are  bound to be wrong a good por- *
  * tion of the time.  Consequently,  the data  segment  is *
  * disassembled  as a byte  stream,  which will satisfy no *
  * one but which, at least, will never mislead anyone.     *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
disdata()

{/* * * * * * * * * * START OF  disdata() * * * * * * * * * */

   register char *c;
   register int j;
   unsigned long end;

   putchar('\n');
   if( HDR.a_data == 0 ) return;

#ifndef VENIX
   if (HDR.a_flags & A_SEP)
      {
      PC = 0L;
      end = HDR.a_data;
      }
   else
      end = HDR.a_text + HDR.a_data;
#else
	   /* XXX True for z-type binaries? */
      PC += HDR.a_stack;
      end = HDR.a_text + HDR.a_stack + HDR.a_data;
#endif

   printf("\t.data\t\t\t| loc = %05.5lx, size = %05.5x\n\n",
    PC,HDR.a_data);

   segflg = 0;

   for (objptr = 0, zcount = 0L; PC < end; ++PC)
      {
      if ((c = getlab(N_DATA)) != NULL)
         {
         objdump(c);
         printf("%s",c);
         }
      if (objptr >= OBJMAX)
         if (objdump(NULL) && (symptr < 0))
            printf("D%05.5lx:",PC);
      j = getchar() & 0xff;
      objbuf[objptr++] = j;
      }

   objdump("");

}/* * * * * * * * * *  END OF  disdata()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  function  handles the object  file's bss segment. *
  * Disassembly of the bss segment is easy,  because every- *
  * thing in it is zero by definition.                      *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void disbss()

{/* * * * * * * * * *  START OF disbss()  * * * * * * * * * */

   register int j;
   register char *c;
   unsigned long beg, end;

   putchar('\n');

   if( HDR.a_bss == 0 ) return;

#ifndef VENIX
   if (HDR.a_flags & A_SEP)
      end = HDR.a_data + HDR.a_bss;
   else
#endif
      end = HDR.a_text + HDR.a_stack + HDR.a_data + HDR.a_bss;

   printf("\t.bss\t\t\t| loc = %05.5lx, size = %05.5x\n\n",
    PC,HDR.a_bss);

   segflg = 0;

   for (beg = PC; PC < end; ++PC)
      if ((c = getlab(N_BSS)) != NULL)
         {
         if (PC > beg)
            {
            zdump(beg);
            beg = PC;
            }
         printf("%s",c);
         }

   if (PC > beg)
      zdump(beg);

}/* * * * * * * * * * * END OF disbss() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the program  entry  point.  The command line is *
  * searched for an input file name, which must be present. *
  * An optional output file name is also permitted; if none *
  * is found, standard output is the default.  One command- *
  * line option is available:  "-o",  which causes the pro- *
  * gram to include  object code in comments along with its *
  * mnemonic output.                                        *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int
main(argc,argv)

   int argc;                  /* Command-line args from OS  */
   register char **argv;

{/* * * * * * * * * * * START OF main() * * * * * * * * * * */

   char a[1024];
   register int fd;
   long taboff, tabnum;
   long reloff, relnum;

   PRG = invoker(*argv);

   while (*++argv != NULL)    /* Process command-line args  */
      if (**argv == '-')
         switch (*++*argv)
            {
            case 'o' :
               if (*++*argv)
                  usage(PRG);
               else
                  ++objflg;
               break;
            case 'f' :
	       force++;
               break;
            default :
               usage(PRG);
            }
      else
         if (IFILE == NULL)
            IFILE = *argv;
         else if (OFILE == NULL)
            OFILE = *argv;
         else
            usage(PRG);

   if (IFILE == NULL)
      usage(PRG);
   else
      if ((fd = open(IFILE,0)) < 0)
         {
         sprintf(a,"can't access input file %s",IFILE);
         fatal(PRG,a);
         }

   if (OFILE != NULL)
      if (freopen(OFILE,"w",stdout) == NULL)
         {
         sprintf(a,"can't open output file %s",OFILE);
         fatal(PRG,a);
         }

   if ( ! cpuid )
      fprintf(stderr,"%s: warning: host/cpu clash\n",PRG);

   read(fd, (char *) &HDR,sizeof(struct exec));

   if (BADMAG(HDR))
      {
      if (!force)
	 {
	 sprintf(a,"input file %s not in object format",IFILE);
	 fatal(PRG,a);
	 }
#ifdef VENIX
      bzero(&HDR, sizeof(struct exec));
#else
      memset(&HDR, '\0', sizeof(struct exec));
#endif
      HDR.a_text = 0x10000L;
      }

#ifdef A_I8086
   if (HDR.a_cpu != A_I8086 && !force)
      {
      sprintf(a,"%s is not an 8086/8088 object file",IFILE);
      fatal(PRG,a);
      }
#endif

#ifdef A_MINDHR
   if (HDR.a_hdrlen <= A_MINHDR)
   {
      HDR.a_trsize = HDR.a_drsize = 0L;
      HDR.a_tbase = HDR.a_dbase = 0L;
 /*   HDR.a_lnums = HDR.a_toffs = 0L; */
   }
#endif

#ifdef VENIX
   reloff = N_TXTOFF(HDR) + HDR.a_text + HDR.a_data;
   relnum =
      (HDR.a_trsize + HDR.a_drsize) / sizeof(struct relocation_info);
   printf("trsize %d drsize %d each %zd\n", HDR.a_trsize, HDR.a_drsize,
       sizeof(struct relocation_info));
#else
   reloff = HDR.a_text        /* Compute reloc data offset  */
          + HDR.a_data
          + (long)(HDR.a_hdrlen);
   relnum =
      (HDR.a_trsize + HDR.a_drsize) / sizeof(struct reloc);
#endif


#ifdef VENIX
   /* Do string offset -> string table stuff here ? */
   taboff = N_SYMOFF(HDR);
   tabnum = HDR.a_syms / sizeof(struct symtb);
#else
   taboff = reloff            /* Compute name table offset  */
          + HDR.a_trsize
          + HDR.a_drsize;

   tabnum = HDR.a_syms / sizeof(struct nlist);
#endif

   if (relnum > MAXSYM)
      fatal(PRG,"reloc table overflow");

   if (tabnum > MAXSYM)
      fatal(PRG,"symbol table overflow");

   if (relnum) {                          /* Get reloc data */
      printf("Reloff is %ld\n", reloff);
      if (lseek(fd,reloff,0) != reloff)
         fatal(PRG,"lseek error");
      else
         {
         for (relptr = 0; relptr < relnum; ++relptr) {
#ifdef VENIX
            read(fd, (char *) &relo[relptr],sizeof(struct relocation_info));
	    printf("Relo %d: addr %#x symnum %d pcrel %d len %d extern %d\n",
		relptr,
		relo[relptr].r_address,
		relo[relptr].r_symbolnum,
		relo[relptr].r_pcrel,
		1 << relo[relptr].r_length,
		relo[relptr].r_extern);
#else
            read(fd, (char *) &relo[relptr],sizeof(struct reloc));
#endif
         }
         relptr--;
         }
   }

   if (tabnum) {                            /* Read in symtab */
      printf("Symtab offset is %ld\n", taboff);
      if (lseek(fd,taboff,0) != taboff)
         fatal(PRG,"lseek error");
      else
         {
         for (symptr = 0; symptr < tabnum; ++symptr)
#ifdef VENIX
            read(fd, (char *) &symtab[symptr],sizeof(struct symtb));
#else
            read(fd, (char *) &symtab[symptr],sizeof(struct nlist));
#endif
         symptr--;
         }
   }
   
#ifdef VENIX
   /*
    * Fill in the string table.
    */
   if (lseek(fd, N_STROFF(HDR), 0) != N_STROFF(HDR))
      fatal(PRG,"lseek error");
   printf("Reading %zd bytes into symbol table\n",
       read(fd, (char *)strtab, MAXSTR));
   for (symptr = 0; symptr < tabnum; ++symptr)
      {
      printf("Symbol %d: offset %d vaddr 0x%x type %x ", symptr,
	   symtab[symptr].ns_un.ns_strx, symtab[symptr].ns_value,
		symtab[symptr].ns_type);
//    symtab[symptr].ns_un.ns_name = strtab + symtab[symptr].ns_un.ns_strx;
      printf("%s\n", strtab + symtab[symptr].ns_un.ns_strx);
      }
#endif

   close(fd);

   if (freopen(IFILE,"r",stdin) == NULL)
      {
      sprintf(a,"can't reopen input file %s",IFILE);
      fatal(PRG,a);
      }

   distext();

   disdata();

   disbss();

   exit(0);

}/* * * * * * * * * * *  END OF main()  * * * * * * * * * * */
