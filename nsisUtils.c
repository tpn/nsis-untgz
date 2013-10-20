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
 *   NullSoft, Tim Kosse
 * Other portions (not copyrighted):
 *   KJD
 * 
 */

#include "nsisUtils.h"  /* prototypes */
#include <commctrl.h>   /* for listview stuff */


/* The following functions are from ExDLL.h
   copyright NullSoft 2002,2003
*/

static unsigned int g_stringsize;
static stack_t **g_stacktop;
static char *g_variables;


#define EXDLL_INIT()           {  \
        g_stringsize=string_size; \
        g_stacktop=stacktop;      \
        g_variables=variables; }

/* utility functions (not required but often useful) */
int popstring(char *str)
{
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th=(*g_stacktop);
  lstrcpy(str,th->text);
  *g_stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

void pushstring(char *str)
{
  stack_t *th;
  if (!g_stacktop) return;
  th=(stack_t*)GlobalAlloc(GPTR,sizeof(stack_t)+g_stringsize);
  lstrcpyn(th->text,str,g_stringsize);
  th->next=*g_stacktop;
  *g_stacktop=th;
}

char *getuservariable(int varnum)
{
  if (varnum < 0 || varnum >= __INST_LAST) return NULL;
  return g_variables+varnum*g_stringsize;
}

void setuservariable(int varnum, char *var)
{
	if (var != NULL && varnum >= 0 && varnum < __INST_LAST) 
		lstrcpy(g_variables + varnum*g_stringsize, var);
}



/* The following functions from UnTGZ NSIS plugin:
   KJD No copyright (other than that from NSIS & zlib & ExtractDLL)
*/

// each exported function should call this (unless you can guarentee its already been done)
//pluginInit(hwndParent, string_size, variables, stacktop);
void pluginInit(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
  g_hwndParent=hwndParent;
  g_hwndList = FindWindowEx(FindWindowEx(g_hwndParent,NULL,"#32770",NULL),NULL,"SysListView32",NULL);
  EXDLL_INIT();
}


/*
 * more printf like variant of below LogMessage from Tim (upon which it requires)
 */
void _cdecl PrintMessage(const char *msg, ...)
{
  char buf[1024];
  va_list argptr;
  va_start(argptr, msg);
  wvsprintf (buf, msg, argptr);
  va_end(argptr);
  DetailPrint(buf);
}

/*
// cheat a little with argument stack
// copies current stack item to str, where current top of
// stack is determined by stacktop (use NULL to get top of
// stack as popstring sees)
// returns a pointer to the next item on the argument stack
 */
stack_t * peekstring(char *str, stack_t *stacktop)
{
  if (stacktop == NULL) stacktop = *g_stacktop;
  if (!stacktop) return NULL;
  lstrcpy(str,stacktop->text);
  return stacktop->next;
}

/*
// fill in a char* list[] and its count with arguments up to
// next argument (-<arg>) or end of stack.
// returns 0 on success, nonzero on error
 */
int getArgList(int *argCnt, char **argList[], char *cmdline)
{
  int cnt;
  char **list;
  char buf[1024];

  register int i;

  stack_t * stkptr = *g_stacktop;


  /* some sanity checking */
  if (argList == NULL) 
  {
    PrintMessage(__FILE__ "::getArgList() called with argList == NULL");
    return -1;
  }
  if (argCnt == NULL) 
  {
    PrintMessage(__FILE__ "::getArgList() called with argCnt == NULL");
    return -2;
  }


  cnt = *argCnt = 0;
  list = NULL;
  *argList = NULL;

  *buf = '\0';
  // loop through until end of stack or next option found
  while ( stkptr && (*buf != '-') )
  {
    stkptr = peekstring(buf, stkptr);
    cnt++;
  }
  if (*buf == '-') cnt--;

  if (cnt > 0)
  {
    if ((list = (char **)malloc(cnt * sizeof(char *))) == NULL) 
	{
		PrintMessage("WARNING: " __FILE__ "::getArgList() Unable to allocate memory required!");
		return 1;
	}
    for (i = 0; i < cnt; i++)
    {
      if (popstring(buf)) 
      {
        /* free all the memory we have allocated */
		int j;
		for (j = 0; j < i; j++)
			if (list[j] != NULL) free(list[j]);
		free(list);
        PrintMessage("WARNING: NSIS stack in undefined state!");
        return 2;
      }
	  strcat(cmdline, "'");
      strcat(cmdline, buf);
      strcat(cmdline, "' ");

      list[i] = (char *)malloc(strlen(buf)+1);
	  if (list[i] == NULL)
      {
        /* free all the memory we have allocated */
		int j;
		for (j = 0; j < i; j++)
          if (list[j] != NULL) free(list[j]);
		free(list);
        /* finish popping the stack so its in a known state */
		for ( ; i < cnt; i++)
          popstring(buf);
		PrintMessage("WARNING: " __FILE__ "::getArgList() Unable to allocate memory required!");
        return 3;
      }
      strcpy(list[i], buf);
    }
  }
  else if (cnt < 0)
  {
    PrintMessage(__FILE__ "::getArgList() Internal Error: cnt < 0");
    return 333;
  }

  *argList = list;
  *argCnt = cnt;
  return 0;
}
/* End KJD's Functions */



/* 
  The following portions from [based upon] code in ExtractDLL NSIS plugin:
			   Copyright 2002 
                     by Tim Kosse
                     tim.kosse@gmx.de

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

*/
int SetStatus(const char *pStr)
{
	HWND hwndCtrl=GetDlgItem(g_hwndList, 1006);
	if (hwndCtrl)
	{
		SetWindowText(hwndCtrl, pStr);
	}
	PrintMessage(pStr);
	return 0;
}

/*
 * Displays a message in NSIS details window, roughly the same as NSIS' DetailPrint
 */
void DetailPrint(const char *pStr)  /* LogMessage() */
{
	if (!g_hwndList) return;
	if (!lstrlen(pStr)) return;

	{
		LVITEM item={0};
		int nItemCount=SendMessage(g_hwndList, LVM_GETITEMCOUNT, 0, 0);
		item.mask=LVIF_TEXT;
		item.pszText=(char *)pStr;
		item.cchTextMax=0;  /* =6 */
		item.iItem=nItemCount;
		/* SendMessage(g_hwndList, LVM_INSERTITEM, 0, (LPARAM)&item); */
		ListView_InsertItem(g_hwndList, &item);
		ListView_EnsureVisible(g_hwndList, item.iItem, 0);
	}
}
/* End Tim's Functions */
