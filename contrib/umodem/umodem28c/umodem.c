/*
 *  UMODEM -- Implements the "CP/M User's Group XMODEM" protocol for
 *            packetized file up/downloading.    
 *
 * Modification History:
 *
 * April 19, 1984 by Robin Miller.
 *	Remove all conditionals to generate code for VMS only.
 *	Don't send the last block twice when sending binary files.
 */

#include stdio

#include ssdef
#include tt2def
#include ttdef
#include "vmodem.h"

#define VERSION         28      /* Version Number */
#define TRUE            1               
#define FALSE           0
#define SOH             001
#define STX             002
#define ETX             003
#define EOT             004
#define ENQ             005
#define ACK             006
#define LF              012     /* Unix LF/NL */
#define CR              015
#define NAK             025
#define SYN             026
#define CAN             030
#define ESC             033
#define CTRLZ           032     /* CP/M EOF for text (usually!) */
#define TIMEOUT         -1
#define ERRORMAX        10      /* maximum errors tolerated */
#define RETRYMAX        10      /* maximum retries to be made */
#define BBUFSIZ         128     /* buffer size -- do not change! */
#define CREATMODE       0644    /* mode for created files */
#define CAN_BOMB                /* Enable bomb out on receipt of CAN */

/*  VMS structures  */
/*
 *      TT_INFO structures are used for passing information about
 *      the terminal.  Used in GTTY and STTY calls.
 */
struct  tt_info ttys, ttysnew, ttystemp;

FILE    *LOGFP, *fopen();
char    buff[BBUFSIZ + 1];

char    XMITTYPE;
int     BIT7, BITMASK, DELFLAG, LOGFLAG;
int     PMSG, RECVFLAG, SENDFLAG, STATDISP;
int     delay;

main(argc, argv)
int argc;
char **argv;
{
        char *logfile;
        int index;
        char flag;

        logfile = "umodem.log";  /* Name of LOG File */

        printf("\nUMODEM Version %d.%d", VERSION/10, VERSION%10);
        printf(" -- UNIX-Based Remote File Transfer Facility\n");

        if (argc < 3 || *argv[1] != '-')
        {  printf("\nUsage:  \n\tumodem ");
                printf("-[rb!rt!sb!st][l][p][y][7]");
                printf(" filename\n");
           printf("\n");
           printf("\trb <-- Receive Binary.\n");
           printf("\trt <-- Receive Text.\n");
           printf("\tsb <-- Send    Binary.\n");
           printf("\tst <-- Send    Text.\n");
           printf("\td  <-- Create umodem.log else append to existing.\n");
           printf("\tl  <-- (ell) Turn OFF LOG File Entries.\n");
           printf("\tp  <-- Turn ON Parameter Display.\n");
           printf("\ty  <-- Display file status (size) information only.\n");
           printf("\t7  <-- Enable 7-bit transfer mask.\n");
           printf("\n");
                exit(SS$_NORMAL);
        }

/*
 *      Initializations
 */
        index           = 1;            /* set index for loop */
        delay           = 3;            /* assume FTP 3 delay */
        RECVFLAG        = FALSE;        /* not receive */
        SENDFLAG        = FALSE;        /* not send either */
        XMITTYPE        = 't';          /* assume text */
        LOGFLAG         = TRUE;         /* assume log messages */
        PMSG            = FALSE;        /* turn off flags */
        STATDISP        = FALSE;        /* assume not a status display */
        BIT7            = FALSE;        /* assume 8-bit communication */
        DELFLAG         = FALSE;        /* do NOT delete log file
                                         *   before starting */

        while ((flag = argv[1][index++]) != '\0')
            switch (flag) {
                case '7' : BIT7 = TRUE;  /* transfer only 7 bits */
                           break;
                case 'd' : DELFLAG = TRUE;  /* delete log file first */
                           break;
                case 'L':
                case 'l' : LOGFLAG = FALSE;  /* turn off log report */
                           break;
                case 'P':
                case 'p' : PMSG = TRUE;  /* print all messages */
                           break;
                case 'R':
                case 'r' : RECVFLAG = TRUE;  /* receive file */
                           XMITTYPE = gettype(argv[1][index++]);  /* get t/b */
                           break;
                case 'S':
                case 's' : SENDFLAG = TRUE;  /* send file */
                           XMITTYPE = gettype(argv[1][index++]);
                           break;
                case 'Y':
                case 'y' : STATDISP = TRUE;  /* display file status */
                           break;
                default  : error("Invalid Flag", FALSE);
                }

        if (BIT7 && (XMITTYPE == 'b'))
        {  printf("\nUMODEM:  Fatal Error -- Both 7-Bit Transfer and ");
           printf("Binary Transfer Selected");
           exit(SS$_NORMAL);
        }

        if (BIT7)  /* set MASK value */
           BITMASK = 0177;  /* 7 significant bits */
        else
           BITMASK = 0377;  /* 8 significant bits */

        if (PMSG)
           { printf("\nSupported File Transfer Protocols:");
             printf("\n\tCP/M UG XMODEM2 (TERM II FTP 3)");
             printf("\n\n");
           }

        if (LOGFLAG)
           { if (!DELFLAG)
                LOGFP = fopen(logfile, "a");  /* append to LOG file */
             else
                LOGFP = fopen(logfile, "w");  /* new LOG file */
             fprintf(LOGFP,"\n\n++++++++\n");
             fprintf(LOGFP,"\nUMODEM Version %d.%d\n", VERSION/10, VERSION%10);
             printf("\nUMODEM:  LOG File '%s' is Open\n", logfile);
           }

        if (STATDISP) yfile(argv[2]);  /* status of a file */

        if (RECVFLAG && SENDFLAG)
                error("Both Send and Receive Functions Specified", FALSE);
        if (!RECVFLAG && !SENDFLAG)
                error("Neither Send nor Receive Functions Specified", FALSE);

/*
 *	Receive or send a file.
 */
        if (RECVFLAG)
                {
                rfile(argv[2]);                 /*  Receive file  */
                }
        else
                sfile(argv[2]);                 /*  Send file  */

}

gettype(ichar)
char ichar;
        {
        switch (ichar)
                {
                case 'b':       case 'B':
                case 't':       case 'T':
                        return(ichar);

                default:
                        error("Invalid Send/Receive Parameter - not t or b",
                                                                FALSE);
                }

        return;
        }

/* set tty modes for UMODEM transfers */
setmodes()
{

/*  Device characteristics for VMS  */

        int     *iptr, parameters;

/*
 *      Get current terminal parameters.
 */
        if (gtty(&ttys) != SS$_NORMAL)
                error("SETMODES:  error return from GTTY (1)", FALSE);
        if (gtty(&ttysnew) != SS$_NORMAL)
                error("SETMODES:  error return from GTTY (2)", FALSE);

/*
 *      Set new terminal parameters.
 *      Note that there are three bytes of terminal characteristics,
 *      so we should make sure the fourth byte of the integer is unchanged.
 */
        iptr    = &(ttysnew.dev_characteristics.bcharacteristics);
        parameters      = *iptr;

        parameters      &= ~TT$M_ESCAPE;                /*  ESCAPE   OFF  */
        parameters      &= ~TT$M_HOSTSYNC;              /*  HOSTSYNC OFF  */
        parameters      |=  TT$M_NOECHO;                /*  NOECHO   ON   */
        parameters      |=  TT$M_PASSALL;               /*  PASSALL  ON   */
        parameters      &= ~TT$M_READSYNC;              /*  READSYNC OFF  */
        parameters      &= ~TT$M_TTSYNC;                /*  TTSYNC   OFF  */
        parameters      &= ~TT$M_WRAP;                  /*  WRAP     OFF  */
        if (! BIT7)
                parameters      |= TT$M_EIGHTBIT;       /*  EIGHTBIT ON  */

        *iptr           = parameters;

        if (stty(&ttysnew) != SS$_NORMAL)
                error("SETMODES:  error return from STTY", TRUE);

        if (PMSG)
                { printf("\nUMODEM:  TTY Device Parameters Altered");
                  ttyparams();  /* print tty params */
                }

        return;
}

/* restore normal tty modes */
restoremodes(errcall)
int errcall;
{
/*  Device characteristic restoration for VMS  */

        if (stty(&ttys) != SS$_NORMAL)          /*  Restore original modes  */
                {
                if (!errcall)
                        error("Error restoring original terminal params.",
                                                                        FALSE);
                else
                        {
                        printf("UMODEM/RESTOREMODES:  ");
                        printf("Error restoring original terminal params.\n");
                        }
                }

        if (PMSG)
                { printf("\nUMODEM:  TTY Device Parameters Restored");
                  ttyparams();  /* print tty params */
                }

        return;
}

/* print error message and exit; if mode == TRUE, restore normal tty modes */
error(msg, mode)
char *msg;
int mode;
{
        if (mode)
                restoremodes(TRUE);  /* put back normal tty modes */
        printf("UMODEM:  %s\n", msg);
        if (LOGFLAG)
        {   fprintf(LOGFP, "UMODEM Fatal Error:  %s\n", msg);
            fclose(LOGFP);
        }
        exit(SS$_NORMAL);
}

/**  print status (size) of a file  **/
yfile(name)
char *name;
{
        printf("UMODEM File Status Display for %s\n", name);
        if (LOGFLAG) fprintf(LOGFP,"UMODEM File Status Display for %s\n",
          name);

        if (open(name,0) < 0)
        {  printf("File %s does not exist\n", name);
           if (LOGFLAG) fprintf(LOGFP,"File %s does not exist\n", name);
           exit(SS$_NORMAL);
        }

        prfilestat(name);  /* print status */
        printf("\n");
        if (LOGFLAG)
        {  fprintf(LOGFP,"\n");
           fclose(LOGFP);
        }
        exit(SS$_NORMAL);
}

/**  receive a file  **/
rfile(name)
char *name;
{
        char mode;
        int fd, j, firstchar, sectnum, sectcurr, tmode;
        int sectcomp, errors, errorflag, recfin;
        register int bufctr, checksum;
        register int c;
        int errorchar, fatalerror, startstx, inchecksum, endetx, endenq;
        long recvsectcnt;
        int             lowlim, nlflag;
        char            inbuf[BBUFSIZ + 7];

        mode = XMITTYPE;  /* set t/b mode */
        if ((fd = creat(name, CREATMODE)) < 0)
                error("Can't create file for receive", FALSE);
        printf("\r\nUMODEM:  File Name: %s", name);
        if (LOGFLAG)
        {    fprintf(LOGFP, "\n----\nUMODEM Receive Function\n");
             fprintf(LOGFP, "File Name: %s\n", name);
             fprintf(LOGFP,
                  "TERM II File Transfer Protocol 3 (CP/M UG) Selected\n");
             if (BIT7)
                fprintf(LOGFP, "7-Bit Transmission Enabled\n");
             else
                fprintf(LOGFP, "8-Bit Transmission Enabled\n");
        }
        printf("\r\nUMODEM:  ");
        if (BIT7)
                printf("7-Bit");
        else
                printf("8-Bit");
        printf(" Transmission Enabled");
        printf("\r\nUMODEM:  Ready to RECEIVE File\r\n");

        setmodes();  /* setup tty modes for xfer */
        recfin = FALSE;
        sectnum = errors = 0;
        fatalerror = FALSE;  /* NO fatal errors */
        recvsectcnt = 0;  /* number of received sectors */

        if (mode == 't' || mode == 'T')
                tmode   = TRUE;
        else
                tmode   = FALSE;

        sendbyte(NAK);  /* FTP 3 Sync */

        nlflag  = FALSE;
        do
        {   errorflag = FALSE;
            do {
                  firstchar = readbyte(6);
            } while ((firstchar != SOH) && (firstchar != EOT) && (firstchar 
                     != TIMEOUT));
            if (firstchar == TIMEOUT)
            {  if (LOGFLAG)
                fprintf(LOGFP, "Timeout on Sector %d\n", sectnum);
               errorflag = TRUE;
            }

            if (firstchar == SOH)
               {
/*
 *      Under VMS, read the whole block at once.
 */
               raw_read(BBUFSIZ + 3, inbuf, delay * (BBUFSIZ + 3));
               sectcurr = inbuf[0] & BITMASK;
               sectcomp = inbuf[1] & BITMASK;

               if ((sectcurr + sectcomp) == BITMASK)
               {  if (sectcurr == ((sectnum + 1) % 256) & BITMASK)
                  {  checksum = 0;
                     lowlim  = 2;

                     bufctr     = 0;
                     for (j = lowlim; j < lowlim + BBUFSIZ; j++)
                        {

                        buff[bufctr] = c = inbuf[j] & BITMASK;
                        checksum = (checksum+c)&BITMASK;
                        if (!tmode)  /* binary mode */
                        {  bufctr++;
                           continue;
                        }

/*
 *      Translate CP/M's CR/LF into UNIX's LF, but don't do it by
 *      simply ignoring a CR from CP/M--it may indicate an
 *      overstruck line!
 */
                        if (nlflag)
                                {
/*
 *      Last packet ended with a CR.  If the first character of this
 *      packet ISN'T a LF, then be sure to send the CR.
 */
                                nlflag  = FALSE;
                                if (c != LF)
                                        {
                                        buff[bufctr++]  = CR;
                                        buff[bufctr]    = c;
                                        }
                                }
                        if (j == lowlim + BBUFSIZ - 1 && c == CR)
                                {
/*
 *      Last character in the packet is a CR.  Don't send it, but
 *      turn on NLFLAG to make sure we check the first character in
 *      the next packet to see if it's a LF.
 */
                                nlflag  = TRUE;
                                continue;
                                }
                        if (c == LF && bufctr && buff[bufctr - 1] == CR)
                                {
/*
 *      We have a CR/LF pair.  Discard the CR.
 */
                                buff[bufctr - 1]        = LF;
                                continue;
                                }
                        if (c == CTRLZ)  /* skip CP/M EOF char */
                        {  recfin = TRUE;  /* flag EOF */
                           continue;
                        }
                        if (!recfin)
                           bufctr++;
                     }

/*  Checksum  */     inchecksum      = inbuf[lowlim + BBUFSIZ] & BITMASK;
                        
                     if (checksum == inchecksum)  /* good checksum */
                     {  errors = 0;
                        recvsectcnt++;
                        sectnum = sectcurr;  /* update sector counter */
                        if (write(fd, buff, bufctr) < 0)
                           error("File Write Error", TRUE);
                        else
                           sendbyte(ACK);
                     }
                     else
                     {  if (LOGFLAG)
                                fprintf(LOGFP, "Checksum Error on Sector %d\n",
                                sectnum);
                        errorflag = TRUE;
                     }
                  }
                  else
                  { if (sectcurr == (sectnum % 256) & BITMASK)
                    {  while(readbyte(3) != TIMEOUT);
                       sendbyte(ACK);
                    }
                    else
                    {  if (LOGFLAG)
                        { fprintf(LOGFP, "Phase Error--received sector is %d ",
                                        sectcurr);
                          fprintf(LOGFP, "while expected sector is %d (%d)\n",
                                        ((sectnum + 1) % 256) & BITMASK,
                                        sectnum);
                        }
                        errorflag = TRUE;
                        fatalerror = TRUE;
                        sendbyte(CAN);
                    }
                  }
           }
           else
           {  if (LOGFLAG)
                fprintf(LOGFP, "Header Sector Number Error on Sector %d\n",
                   sectnum);
               errorflag = TRUE;
           }
        }
        if (errorflag == TRUE)
        {  errors++;
           while (readbyte(3) != TIMEOUT);
           sendbyte(NAK);
        }
  }
  while ((firstchar != EOT) && (errors != ERRORMAX) && !fatalerror);
  if ((firstchar == EOT) && (errors < ERRORMAX))
  {  sendbyte(ACK);
     close(fd);
     restoremodes(FALSE);  /* restore normal tty modes */
     sleep(3);  /* give other side time to return to terminal mode */
     if (LOGFLAG)
     {  fprintf(LOGFP, "\nReceive Complete\n");
        fprintf(LOGFP,"Number of Received CP/M Records is %ld\n", recvsectcnt);
        fclose(LOGFP);
     }
     printf("\n");
     exit(SS$_NORMAL);
  }
  else
  {  if (errors == ERRORMAX)
             error("Too many errors", TRUE);
     if (fatalerror)
             error("Fatal error", TRUE);
  }
}

/**  send a file  **/
sfile(name)
char *name;
{
        char mode;
        int fd, charval, attempts;
        int nlflag, sendfin, tmode;
        register int bufctr, checksum, sectnum;
        char c;
        int sendresp;  /* response char to sent block */

        mode = XMITTYPE;  /* set t/b mode */
        if ((fd = open(name, 0)) < 0)
        {  if (LOGFLAG) fprintf(LOGFP, "Can't Open File\n");
           error("Can't open file for send", FALSE);
        }
        printf("\r\nUMODEM:  File Name: %s", name);
        if (LOGFLAG)
        {   fprintf(LOGFP, "\n----\nUMODEM Send Function\n");
            fprintf(LOGFP, "File Name: %s\n", name);
        }
        prfilestat(name);  /* print file size statistics */
        if (LOGFLAG)
        {  fprintf(LOGFP,
                   "TERM II File Transfer Protocol 3 (CP/M UG) Selected\n");
           if (BIT7)
                fprintf(LOGFP, "7-Bit Transmission Enabled\n");
           else
                fprintf(LOGFP, "8-Bit Transmission Enabled\n");
        }
        printf("\r\nUMODEM:  ");
        if (BIT7)
                printf("7-Bit");
        else
                printf("8-Bit");
        printf(" Transmission Enabled");
        printf("\r\nUMODEM:  Ready to SEND File\r\n");

        setmodes();  /* setup tty modes for xfer */     
        if (mode == 't' || mode == 'T')
                tmode   = TRUE;
        else
                tmode   = FALSE;

        sendfin = nlflag = FALSE;
        attempts = 0;

        while (readbyte(30) != NAK)  /* FTP 3 Synchronize with Receiver */
           if (++attempts > RETRYMAX) error("Remote System Not Responding",
                TRUE);

        sectnum = 1;  /* first sector number */
        attempts = 0;

        do 
        {   for (bufctr=0; bufctr < BBUFSIZ;)
            {   if (nlflag)
                {  buff[bufctr++] = LF;  /* leftover newline */
                   nlflag = FALSE;
                }
                if ((charval = read(fd, &c, 1)) < 0)
                   error("File Read Error", TRUE);
                if (charval == 0)  /* EOF for read */   
                {  sendfin = TRUE;  /* this is the last sector */
                   if (tmode)
                      buff[bufctr++] = CTRLZ;  /* Control-Z for CP/M EOF */
                   else
                      { if (bufctr == 0)
                            break;             /* Nothing more to send. */
                        else
                            bufctr++;          /* Partial binary file. */
                      }
                   continue;
                }
                if (tmode && c == LF)  /* text mode & Unix newline? */
                {  if (c == LF)  /* Unix newline? */
                   {  buff[bufctr++] = CR;  /* insert carriage return */
                      if (bufctr < BBUFSIZ)
                         buff[bufctr++] = LF;  /* insert Unix newline */
                      else
                         nlflag = TRUE;  /* insert newline on next sector */
                   }
                   continue;
                }       
                buff[bufctr++] = c;  /* copy the char without change */
            }
            attempts = 0;
            do
            {   if (bufctr == 0) break;  /* don't resend last block twice */
                sendbyte(SOH);           /* send start of packet header */
                sendbyte(sectnum);       /* send current sector number */
                sendbyte(-sectnum-1);    /* and its complement */

                checksum = 0;  /* init checksum */
                for (bufctr=0; bufctr < BBUFSIZ; bufctr++)
                {  sendbyte(buff[bufctr]);  /* send the byte */
                   checksum = (checksum+buff[bufctr])&BITMASK;
                }
/*              while (readbyte(3) != TIMEOUT);   flush chars from line */
                sendbyte(checksum);  /* send the checksum */
                attempts++;
                sendresp = readbyte(10);  /* get response */
                if ((sendresp != ACK) && LOGFLAG)
                   { fprintf(LOGFP, "Non-ACK Received on Sector %d\n",
                      sectnum);
                     if (sendresp == TIMEOUT)
                        fprintf(LOGFP, "This non-ACK was a TIMEOUT\n");
                     else if (sendresp == NAK)
                        fprintf(LOGFP, "This non-ACK was a NAK\n");
                   }
                if (sendresp == CAN)
                        {
                        if (LOGFLAG)
                                fprintf(LOGFP, "This non-ACK was a CAN\n");

#ifdef CAN_BOMB
                        if (LOGFLAG)
                                fprintf(LOGFP,
                                        "Exiting:  got a CAN from receiver.\n");
                        close(fd);
                        restoremodes(TRUE);
                        sleep(5);       /*  Allow other side to recover  */
                        printf("Exiting:  got a CAN from receiver.\n\n");
                        exit(SS$_NORMAL);
#endif
                        }
            }   while((sendresp != ACK) && (attempts != RETRYMAX));
            sectnum++;  /* increment to next sector number */
    }  while (!sendfin && (attempts != RETRYMAX));

    if (attempts == RETRYMAX)
        error("Remote System Not Responding", TRUE);

    attempts = 0;
    sendbyte(EOT);  /* send 1st EOT */
        while ((readbyte(15) != ACK) && (attempts++ < RETRYMAX))
           sendbyte(EOT);
        if (attempts >= RETRYMAX)
           error("Remote System Not Responding on Completion", TRUE);

    close(fd);
    restoremodes(FALSE);  
    sleep(5);  /* give other side time to return to terminal mode */
    if (LOGFLAG)
    {  fprintf(LOGFP, "\nSend Complete\n");
       fclose(LOGFP);
    }
    printf("\n");
    exit(SS$_NORMAL);
}

/*  print file size status information  */
prfilestat(name)
char *name;
{
        long            bytes, Kbytes, records;

        bytes           = filestat(name);       /*  Gets file length  */
        if (bytes < 0)
                {
                printf("PRFILESTAT:  error return from FILESTAT\n");
                return;
                }
        Kbytes          = (bytes / 1024) + 1;
        records         = (bytes /  128) + 1;

        printf("\nUMODEM:  Estimated File Size %ldK, %ld Records, %ld Bytes",
                                Kbytes, records, bytes);
        printf("\n         Estimated transfer time at 300 baud:  ");
        printf("%ld min, %ld sec.",
                                bytes / (30*60), ((bytes % (30*60)) / 30) + 1);

        if (LOGFLAG)
                fprintf(LOGFP,
                        "Estimated File Size %ldK, %ld Records, %ld Bytes\n",
                                Kbytes, records, bytes);
        return;
}

/* get a byte from data stream -- timeout if "seconds" elapses */
/*      NOTE, however, that this function returns an INT, not a BYTE!!!  */
readbyte(seconds)
unsigned        seconds;
        {
        int     c;

        c       = raw_read(1, &c, seconds);
        if (c == SS$_TIMEOUT)
                return(TIMEOUT);
        return(c & BITMASK);  /* return the char */
        }

/* send a byte to data stream */
sendbyte(data)
char    data;
        {
        char    dataout;

        dataout = data & BITMASK;               /*  Mask for 7 or 8 bits  */
        raw_write(dataout);
        return;
        }

/* print data on TTY setting */
ttyparams()
{

/*  For VMS, report that no information is available and return  */

        printf("\nUMODEM/TTYPARAMS:  ");
        printf("TT device parameter display not implemented under VMS.\n");
}
