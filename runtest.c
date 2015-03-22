/* file: runtest.c

  modified by: Chad Duncan
  last modified: 2/22/2015

  This version: prints termination messages,
                redirects stdout and stderr to temporary files
                  and copies the first 40 to the report,
                compares output to baseline, 
                and enforces a file size of 100K and CPU time 
                  limit of 1 second.
  
   WARNING: The name of this program (unfortunately) 
   is the same as a Linux utility.
   So, to execute it always use "./runtest" (not just "runtest").
  
   To compile this program use the Makefile, or at the command line

     gcc -Wall -Wextra -pedantic -o runtest runtest.c

*/

/* needed for strsignal() in string.h with SunOS */
#define __EXTENSIONS__
/* needed for strsignal() in string.h with Linux */
#define _GNU_SOURCE
/* needed for fork(), execve(), getcwd(), unlink(), close(), dup2(), alarm() */
#include <unistd.h>
#include <stdio.h>
/* needed for open(), O_RDWR, O_CREAT, S_IWUSR, S_IRUSR */
#include <fcntl.h>  
#include <stdlib.h>
#include <string.h>
/* needed for errno and ENOENT */
#include <errno.h>  
/* needed for dirname() and basename() */
#include <libgen.h>
/* needed for sigaction() and kill() */
#include <signal.h>
/* needed for uname() */
#include <sys/utsname.h>
/* needed for waitpid() */
#include <sys/wait.h>

#define CHECK(CALL) if (CALL == -1) {perror (#CALL); exit (-1);}
#define CHECK_ERRNO(CALL,ERRNO) if ((CALL != 0) && errno !=ERRNO) {perror (#CALL); exit (-1);}

extern char ** environ;  /* see "man environ" for explanation */

/* time to wait for the child, in seconds */
#define SLEEP_TIME 2

#define TMPFILE "tmpfile"

/* copy first 40 lines of output*/
void cpLines(_IO_FILE * stream, char* tmp1)
{
  FILE *openTmp = fopen(tmp1, "r");
  char str[1001];
  int i;
  for(i=0; i<40; ++i)
  {
    if(fgets(str, 1000, openTmp) == NULL)
      return;
    
    fputs(str, stream);
  }
  fclose(openTmp);
}

/* compare output */
int compare(char* base, int tmp1)
{
  /* open files to be compared */
  FILE *baseline = fopen(base, "r");
  FILE *openTmp = fdopen(tmp1, "r");
  char str[1001];
  char str2[1001];
  
  /* loop until we reach the end of either file */
  while(fgets(str, 1000, baseline) != NULL && fgets(str2, 1000, openTmp) != NULL)
  {
    /* quit early in case of difference */
    if(strcmp(str, str2) != 0)
    {
      fclose(openTmp);
      fclose(baseline);
      return 0;
    }
  }
  fclose(openTmp);
  fclose(baseline);
  if(strcmp(str, str2) != 0)
    return 0;
  return 1;
}

/* handle alarm signal */
void sigHandler()
{
  return;
}

int main(int argc, char *argv[])
{

  pid_t child;
  int tmp1;
  int tmp2;
  char baseOut[100] = "\0";
  char baseErr[100] = "\0";
  struct rlimit rl;
  struct utsname os;
  char *cwd = getcwd(NULL, 1000);
  cwd = basename(dirname(cwd));
  uname(&os);
  
  /* do a sanity check on command-line arguments */

  if (argc < 2) {
    fprintf (stderr, "usafe: requires at least one command-line argument.\n");
    exit (-1);
  }
  
  /* print descriptive header for this test  */

  fprintf (stdout, "===testing ./sim");
  { int i;
    for (i = 2; i < argc; i++) {
      fprintf (stdout, " %s", argv[i]);
    }
  }
  
  fprintf (stdout, " in %s", cwd);
  fprintf (stdout, " on %s", os.sysname);
  fprintf (stdout, " ===========================\n");
  fflush (stdout);

  /* remove junk files created by this program, if left over from
     a previous execution */
  
  if(strcmp(cwd, "sol0"))
  {
    CHECK_ERRNO (unlink (TMPFILE), ENOENT);
    CHECK_ERRNO (unlink (TMPFILE "2"), ENOENT);
  }
  else
  {
    if(*argv[1] == 1)
    {
      CHECK_ERRNO (unlink ("../../base-Linux-stderr-1"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-Linux-stdout-1"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-SunOS-stderr-1"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-SunOS-stdout-1"), ENOENT);
    }
    else
    {
      CHECK_ERRNO (unlink ("../../base-Linux-stderr-2"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-Linux-stdout-2"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-SunOS-stderr-2"), ENOENT);
      CHECK_ERRNO (unlink ("../../base-SunOS-stdout-2"), ENOENT);
    }
  }
  
  /* create a child process */
  CHECK ((child = fork()));
  
  /* create temporary files to receive output */
  tmp1 = open(TMPFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (tmp1 == -1) {
    perror ("tmpfile open failed\n"); exit (-1);
  }     
  tmp2 = open (TMPFILE "2", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (tmp2 == -1) {
    perror ("tmpfile2 open failed\n"); exit (-1);
  } 
  
  if (child == 0) {

    /* executed only by the child process */

    /* set up argument list for the child process */
    
    int argcount = 1;
    char **args = malloc (argcount + 1);
    args[0] = "./sim";
    args[1] = NULL;

    /* redirect stdout and stderr  */
    
    if (dup2 (tmp1, 1) < 0 || dup2 (tmp2, 2) < 0) {
      perror ("dup2 failed"); exit(-1);
    }
    close (tmp1);
    close (tmp2);
    
    /* set resource limits on CPU time and file size */
    
    /* cpu limit of 1 second */
    getrlimit (RLIMIT_CPU, &rl);
    rl.rlim_cur = 1;
    setrlimit (RLIMIT_CPU, &rl);
    
    /* file size limit of 100K */
    getrlimit (RLIMIT_FSIZE, &rl);
    rl.rlim_cur = 100000;
    setrlimit (RLIMIT_FSIZE, &rl);
    
    /* execute tested program with provided arguments
       pass through the environment variable of the current process  */
    
    CHECK (execve (args[0], args, environ));

    /* there is no normal return from this call, since execution
       continues in the new program */

  } else {

    /* executed only by the parent process */
    
    /* wait for child process to terminate, with timeout */

    /* sleep for a time, to give the child a chance to run */
    CHECK ((int) sleep (SLEEP_TIME));
    
    /* poll the status of the child */
    { int status;
      int result;
      result = waitpid (child, &status, WUNTRACED);
      if (result == -1 && errno != EINTR) {
        perror ("waitpid failed"); exit (-1);
      } else if (result == 0) {
        /* child is not yet terminated */
        /* terminate the child */
        CHECK (kill (child, SIGKILL));
        /* wait for the child to be terminated */
        CHECK (waitpid (child, &status, 0));
      }
      /* print out description of process termination status */
      if(WIFEXITED(status))
      {
        fputs("process exited normally\n", stderr);
      }
      else if(WIFSIGNALED(status))
      {
        if(WTERMSIG(status) == SIGSEGV) 
          fputs("**** process terminated by signal Segmentation fault\n", stderr);
        if(WTERMSIG(status) == SIGXFSZ)
          fputs("**** process terminated by signal File size limit exceeded\n", stderr);
        if(WTERMSIG(status) == SIGXCPU)
          fputs("**** process terminated by signal CPU time limit exceeded\n", stderr);
        if(WTERMSIG(status) == SIGVTALRM)
          fputs("**** process terminated by signal Virtual alarm clock\n", stderr);
      }
      else if(WIFSTOPPED(status))
      {
        fputs("Sim did not terminate properly.\n", stderr);
      }
    }
       
    /* find location of baseline output */
    
    strcat(baseOut, "../../base-");
    strcat(baseOut, os.sysname);
    strcat(baseOut, "-stdout-");
    strcat(baseOut, argv[1]);
    strcat(baseErr, "../../base-");
    strcat(baseErr, os.sysname);
    strcat(baseErr, "-stderr-");
    strcat(baseErr, argv[1]);
    
    /* compare the stderr against the corresponding baseline
       output file from the sol0 directory, and similarly for
       stdout; print one-line messages indicating the result of
       the comparison */
       
    if(!strcmp(cwd, "sol0"))
    {
      rename(TMPFILE "2", baseErr);
      rename(TMPFILE, baseOut);
    }
    else
    {
      if(!compare(baseErr, tmp2))
        fputs("**** stderr does not match baseline!\n", stderr);
      else
        fputs("     stderr matches baseline\n", stderr);
      
      if(!compare(baseOut, tmp1))
        fputs("**** stdout does not match baseline!\n", stderr);
      else
        fputs("     stdout matches baseline\n", stderr);
    }
    
    fprintf (stdout, "___stderr___________________________________________________\n");

    /* copy the first 40 lines of the stderr output from sim to the current stdout */
    
    if(strcmp(cwd, "sol0"))
      cpLines(stdout, TMPFILE "2");
    else
      cpLines(stdout, baseErr);
      
    fprintf (stdout, "___stdout___________________________________________________\n");

    /* do the same for the stdout output from  sim */
    
    if(strcmp(cwd, "sol0"))
      cpLines(stdout, TMPFILE);
    else
      cpLines(stdout, baseOut);

    /* remove any temporary files created */

    if(strcmp(cwd, "sol0"))
    {
      CHECK_ERRNO (unlink (TMPFILE), ENOENT);
      CHECK_ERRNO (unlink (TMPFILE "2"), ENOENT);
    }
  }

  
  return 0;
}

