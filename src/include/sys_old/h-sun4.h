/*
 * h-sun4.h
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>
#ifndef O_ACCMODE
# define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif /* !O_ACCMODE */
#define SEEK_SET 0
#define SEEK_CUR 1

extern PROTO(int, abort,(void));
extern PROTO(int, close,(int));
extern PROTO(int, fcntl,(int des, int cmd, int e));
extern PROTO(int, fprintf,(FILE *,char *,...));
extern PROTO(int, printf,(char *,...));
extern PROTO(int, qsort,(void *data,int els, int siz, int func()));
extern PROTO(void, exit,(int));
extern PROTO(int, fseek,(FILE*, int, int));
extern PROTO(int, fclose,(FILE*));
extern PROTO(void, bcopy,(char*,char*,int));
extern PROTO(int, bcmp,(char *, char *, int));
extern PROTO(void, bzero,(char *, int));
extern PROTO(PTR,memset,(PTR, int,unsigned int));
#ifndef __GNUC__
PROTO(PTR, memcpy,(PTR,CONST PTR,unsigned int));
#else
# if 0
PROTO(char *, memcpy,(char *,CONST char *,unsigned int));
# endif /* 0 */
#endif /* !__GNUC__ */

extern PROTO(int,getuid,());
extern PROTO(int,getgid,());
extern char * strchr();
extern PROTO(void, perror,(CONST char *));
extern char *getenv();
extern char *memchr();
extern char *strrchr();
extern int chmod();
extern int fread();
extern int fstat();
extern int fwrite();
extern int sscanf();
extern int stat();
extern int strtol();
#ifndef DONTDECLARE_MALLOC
extern PROTO(PTR,malloc,(unsigned));
extern PROTO(PTR ,realloc, (PTR, unsigned));
#endif /* !DONTDECLARE_MALLOC */

extern PROTO(int, free,(PTR));


extern char *strrchr();
extern char *ctime();
extern int _flsbuf();
extern int fclose();
extern int time();
extern int utimes();
extern int vfprintf();
extern long atol();
extern char *getenv();
extern int fputc();
extern int unlink();


/* EXACT TYPES */
typedef char int8e_type;
typedef unsigned char uint8e_type;
typedef short int16e_type;
typedef unsigned short uint16e_type;
typedef int int32e_type;
typedef unsigned int uint32e_type;


#ifdef __GNUC__
typedef unsigned long long uint64e_type;

#else
typedef struct {
  uint32e_type low, high;
} uint64e_type;

#endif /* __GNUC__ */
/* CORRECT SIZE OR GREATER */
typedef char int8_type;
typedef unsigned char uint8_type;
typedef short int16_type;
typedef unsigned short uint16_type;
typedef int int32_type;
typedef unsigned int uint32_type;

#ifdef __GNUC__
typedef unsigned long long uint64_type;
typedef long long int64_type;
#else
typedef struct {
  uint32e_type low, high;
} uint64_type;

typedef struct {
  uint32e_type low, high;
} int64_type;

#endif /* __GNUC__ */


#define BYTES_IN_PRINTF_INT 4
#ifndef __GNUC__
# define uint64_typeLOW(x) (uint32_type)(((x).low))
# define uint64_typeHIGH(x) (uint32_type)(((x).high))
#else
# define uint64_typeLOW(x) (uint32_type)(((x) & 0xffffffff))
# define uint64_typeHIGH(x) (uint32_type)(((x) >> 32) & 0xffffffff)
#endif /* !__GNUC__ */

/* EOF */
