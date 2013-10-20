/* NSISutils contains various utility / useful functions 
 * that I've found useful for NSIS Plugins
 * Each section is preceeded by its copyright and license
 * information.  Nothing more restrictive than NSIS
 * license is used.
 *
 * Please note: all material here other than those by KJD
 * have been taken from the respective author/copyright holder's
 * work, do not contact them regarding these changes.
 * Corrections/additions/ or information to obtain original
 * versions, please contact me: jeremyd@computer.org
 *
 * Portions copyright:
 *   Tim Kosse
 * Other portions (not copyrighted):
 *   KJD
 * 
 */

#ifndef NSIS_UTILS_H
#define NSIS_UTILS_H


#include "miniclib.h"   /* mini stdc replacement & includes windows.h */


#ifdef __cplusplus
extern "C" {
#endif


/* includes definitions and functions from sample  #include "../exdll/exdll.h" */


/* actual stack item */
typedef struct _stack_t {
  struct _stack_t *next;
  char text[1]; // this should be the length of string_size
} stack_t;

/* global variables */
extern HWND g_hwndParent;
extern HWND g_hwndList;

extern unsigned int g_stringsize;
extern stack_t **g_stacktop;
extern char *g_variables;


/* For page showing plug-ins */
#define WM_NOTIFY_OUTER_NEXT (WM_USER+0x8)
#define WM_NOTIFY_CUSTOM_READY (WM_USER+0xd)
#define NOTIFY_BYE_BYE 'x'

/* defines variables for use with get/set uservariable */
enum
{
INST_0,         // $0
INST_1,         // $1
INST_2,         // $2
INST_3,         // $3
INST_4,         // $4
INST_5,         // $5
INST_6,         // $6
INST_7,         // $7
INST_8,         // $8
INST_9,         // $9
INST_R0,        // $R0
INST_R1,        // $R1
INST_R2,        // $R2
INST_R3,        // $R3
INST_R4,        // $R4
INST_R5,        // $R5
INST_R6,        // $R6
INST_R7,        // $R7
INST_R8,        // $R8
INST_R9,        // $R9
INST_CMDLINE,   // $CMDLINE
INST_INSTDIR,   // $INSTDIR
INST_OUTDIR,    // $OUTDIR
INST_EXEDIR,    // $EXEDIR
INST_LANG,      // $LANGUAGE
__INST_LAST
};


/*
 * each exported function should call this (unless you can guarentee its already been done)
 * pluginInit(hwndParent, string_size, variables, stacktop);
 */
void pluginInit(HWND hwndParent, int string_size, char *variables, stack_t **stacktop);

/* 
 * retrieves next item from stack
 * you should not pop more arguments than you are expecting,
 * as past that you are poping arguments pushed on the
 * stack for some other purposes (i.e. to save some value for restoration)
 */
int popstring(char *str); // 0 on success, 1 on empty stack

/* 
 * pushes a value onto the stack for the NSIS script to retrieve
 * or for you to pop off later in your plugin
 */
void pushstring(char *str);

/*
 * cheat a little with argument stack
 * copies current stack item to str, where current top of
 * stack is determined by stacktop (use NULL to get top of
 * stack as popstring sees)
 * returns a pointer to the next item on the argument stack
 */
stack_t * peekstring(char *str, stack_t *stacktop);

/*
 * gets a variable's value (a string), $R0-$R9, $0-$9, ...
 */
char *getuservariable(int varnum);

/*
 * sets a variable's value (a string), $R0-$R9, $0-$9, ...
 */
void setuservariable(int varnum, char *var);

/*
 * Displays a message in NSIS details window (LogMessage)
 * roughly the same as NSIS' DetailPrint
 */
void DetailPrint(const char *pStr);

/*
 * more printf like variant of below LogMessage from Tim (upon which it requires)
 */
void _cdecl PrintMessage(const char *msg, ...);

/*
 * Sets the status text
 */
int SetStatus(const char *pStr);

/*
 * fill in a char* list[] and its count with arguments up to
 * next argument (-<arg>) or end of stack.
 * returns 0 on success, nonzero on error
 */
int getArgList(int *argCnt, char **argList[], char *cmdline);


#ifdef __cplusplus
}
#endif

#endif /* NSIS_UTILS_H */
