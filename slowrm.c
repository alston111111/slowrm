/*
* Copy Right Alston@HOME, Oct 28th 2012
* 
* Goal: To Remove a file SMOOTHLY.
* That is to remove a file with less IO load,
* But may take more time than ''rm'' does.
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libgen.h>
#include <string.h>

#ifndef DEFAULT_USLEEP_INTERVAL
#define DEFAULT_USLEEP_INTERVAL (100 * 1000)
#endif
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE (4096 * 256)
#endif
#ifndef MAX_FILE_NUM
#define MAX_FILE_NUM (-1)
#endif

#ifndef VERSION
#define VERSION "0.0.1"
#endif

struct rm_arguments{
    size_t block_size;
    int usleep_interval;
    char ** filenames;
    int num_files;

};

static void help_info();

static int help_flag = 0;
static int force_flag = 0;

static const char * program_name;
static const char * version = VERSION;

/*
* to remove a file with less io load by truncating the file to a smaller size at a time until the file is empy.
*/
int fslowtrunc(int fd, off_t len, size_t block_size, int interval)
{
  off_t flen;
  struct stat buf;
  if(0 > fstat(fd, &buf)){
    printf("sth wrong when getting the stat of fd: %d\n", fd);
    return errno;
  }
  if(!S_ISREG(buf.st_mode)){
    printf("Fd: %dThis is not a regular file, cannot truncate it!\n", fd);
    return -1;
  }
  flen = buf.st_size - buf.st_size % block_size;
  while(flen >= block_size){
    flen -= block_size;
    ftruncate(fd, flen);
    usleep(interval);
  }
  ftruncate(fd, 0);
  return errno;
}

/*
* Try to parse opts:
* Two main options:
* -S #SECONDS: a non-negative float number
* -b #BYTES: a positive integer, better as a power of 2 or devidible by 1024
*/
void * do_with_opts(int argc, char * argv[])
{
    int c, i;
    int max_file_num = MAX_FILE_NUM;

    struct rm_arguments * arg;

    if( NULL == (arg = (struct rm_arguments *)malloc(sizeof(struct rm_arguments))) ){
        abort();
    }
    arg->block_size = DEFAULT_BLOCK_SIZE;
    arg->usleep_interval = DEFAULT_USLEEP_INTERVAL;
//    arg->filenames = (char**)malloc(sizeof(char*) * max_file_num);

    while(1) {
        static struct option long_options[] = { 
            {"block-size", required_argument, 0, 'b'},
            {"sleep", required_argument, 0, 's'},
            {"help", no_argument, &help_flag, 1},
            {"force", no_argument, &force_flag, 1},
            {0, 0, 0, 0}
        };  

        int opt_index = 0;
        c = getopt_long(argc, argv, "s:b:fh", long_options, &opt_index);
        if( -1 == c)
            break;

        switch(c)
        {   
        case 'b':
            arg->block_size = (size_t)atoi(optarg);
            break;
        case 'f':
            force_flag = 1;
            break;
        case 's':
            arg->usleep_interval = (int)(1000 * 1000 * atof(optarg));
            break;
        case 'h':
            help_info(program_name);
            break;
        case 0:
            if(help_flag)
                help_info(program_name);
            break;
        default:
            exit(1);
        }
    }

    arg->num_files = argc - optind;
    if( NULL == (arg->filenames = (char**)malloc(sizeof(char*) * arg->num_files)) ){
        fprintf(stderr, "Something wrong with allocation of memory! file: %s, line: %d\n",  __FILE__, __LINE__);
        exit(1);
    }
    for(i=0; optind < argc; i++, optind++){
        arg->filenames[i] = argv[optind];
    }
    arg->num_files = i;
    return arg;
}

void help_info(const char * program_name)
{
    static const char help_str[] = "\
    This is slowrm Version %s\n\
    %s is a tool for smoothly removing large files, which you can use just like using the 'rm' command.\n\
    This command will prompt by default and users should input 'y' to remove the file.\n\
    But so far as this version released, it does not support removing files recursively.\n\n\
    Usage:\n\
        %s [OPTIONS] FILE1 [,FILE2...]\n\n\
    OPTIONS:\n\n\
        -b #BYTES, --block-size\n\
                    Truncates files block by block, each block with size #BYTES.\n\
                    By default, #BYTES is %d\n\n\
        -f, --force\n\
                    Removes the file forcely, that is to run without a prompt.\n\n\
        -s #SECONDS, --sleep-interval\n\
                    Sleeps SECONDS seconds after truncating the file by one block.\n\
                    By default, #SECONDS is %f\n\n\
        -h, --help  Shows help infomation\n\n\
        --verbose   explain what is being done\n\n\
    EXAMPLES:\n\
        slowrm -s 0.2 -b 4096 file1 file2\n\
        slowrm -s 0.2 -b 4096 fileabc*\n\
        slowrm -s 0.2 -b `expr 4096 \\* 256` fileabc*\n\n\
";
    printf( help_str, version, program_name, program_name, DEFAULT_BLOCK_SIZE, (double)DEFAULT_USLEEP_INTERVAL/(1000*1000));
}

void my_err(int errno, const char * extra_msg)
{
  fprintf(stderr, "%s: %s: %s", program_name, extra_msg, strerror(errno));
}

int main(int argc, char ** argv)
{
  char * filename;
  int fd;
  int i = 1;
  struct rm_arguments * arg;
  struct stat stat_buf;
  program_name = basename(argv[0]);
  char rm_yes, c;

  if(argc == 1){
    help_info(program_name);
  }
  arg = do_with_opts(argc, argv);
  if( NULL == arg){
    error(1, 1, "%s: No filenames are provided!", program_name);
    exit(1);
  }
  i = 0;
  while(i < arg->num_files){
    filename = arg->filenames[i++];
    if(0 > lstat(filename, &stat_buf)){
      fprintf(stderr, "%s: cannot lstat '%s': %s\n", program_name, filename, strerror(errno));
      continue;
    }
    if(!S_ISREG(stat_buf.st_mode)){
      fprintf(stderr, "%s: '%s' is not a regular file: %s\n", program_name, filename, (errno?strerror(errno):""));
      continue;
    }

    if(!force_flag){
      printf("%s: remove regular file '%s'?(y:yes)", program_name, filename);
      rm_yes = getchar();
      while((c=getchar()) != 0 && c != '\n');
      if(rm_yes != 'y'){
        continue;
      }
    }
    if(stat_buf.st_nlink > 1){
      unlink(filename);
      continue;
    }

    if(0 > (fd = open(filename, O_WRONLY))){
      fprintf(stderr, "%s: error with opening file: '%s': %s\n", program_name, filename, strerror(errno));
      continue;
    }
    fslowtrunc(fd, 0, arg->block_size, arg->usleep_interval);
    close(fd);
    unlink(filename);
  }
  return errno;
}
