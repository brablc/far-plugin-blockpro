/*
 * Copyright (c) Ondrej Brablc, 2002 <far@brablc.com>
 * You can use, modify, distribute this source code  or
 * any other parts of BlockPro plugin only according to
 * BlockPro  License  (see  \Doc\License.txt  for  more
 * information).
 */

#include <plugin.hpp>
#include <filebuf.hpp>

#include <stdio.h>
#include <farintf.h>
#include <winmem.h>

#include "bpplugin.h"

extern "C"
{
    bool WINAPI _export OpenHelp(struct PluginStartupInfo *PSInfo,
                                 struct BlockProData *Data);
};

class ClassFileBuf: public ReadFileBuf
{
    public:
        ClassFileBuf(char * fileName, DWORD mode = FILE_FLAG_SEQUENTIAL_SCAN);
        bool ReadClass(char * pPath, char * pClass);
        bool GetClass(char * pPath, char * pClass);
};

ClassFileBuf::ClassFileBuf(char * fileName, DWORD mode):
              ReadFileBuf( fileName, mode)
{

}

#define CLASS_BEG "<A HREF=\""
#define CLASS_MID "\" TARGET=\"classFrame\">"
#define CLASS_END "<\/A>"
bool ClassFileBuf::GetClass(char * pPath, char * pClass)
{
    char * beg, * end;

    if (!(beg = strstr( LastLine(), CLASS_BEG))) return false;
    beg += strlen(CLASS_BEG);

    if (!(end = strstr( beg, CLASS_MID))) return false;
    strncpy( pPath, beg, end-beg);
    pPath[end-beg] = 0;
    beg = end + strlen( CLASS_MID);

    if (!(end = strstr( beg, CLASS_END))) return false;
    strncpy( pClass, beg, end-beg);
    pClass[end-beg] = 0;

    if (strstr( pClass, "<I>") == pClass)
    {
        strcpy(pClass, pClass+3);
        pClass[strlen(pClass)-4] = 0;
    }
    return true;
}
#undef CLASS_BEG
#undef CLASS_MID
#undef CLASS_END

bool ClassFileBuf::ReadClass(char * pPath, char * pClass)
{
    while (ReadLine())
    {
        if (GetClass(pPath, pClass)) return true;
    }
    return false;
}

/******************************************************************************/

bool StrBegin(char * str, char * substr)
{
    if (strlen(substr)>strlen(str)) return false;

    while (*substr)
    {
        if (FSF.LUpper(*str++) != FSF.LUpper(*substr++)) return false;
    }
    return true;
}

/******************************************************************************/

int BuildMenu(ClassFileBuf * cfb, FarMenuItem ** menu, char * keyWord)
{
    char path[512];
    char className[80];

    struct TMenuArray
    {
        FarMenuItem *Items;
        size_t Count, Size, Delta;
    } a = { NULL, 0, 0, 10 };

    FarMenuItem *newItems;

    if (!cfb->GetClass( path, className))
    {
        // InfoMsg(true, "NextRead", "OK", "Class: %s", className);
        cfb->ReadClass(path, className);
    }

    do
    {
        if (StrBegin(className,keyWord))
        {
            if ( a.Count == a.Size )
            {
                newItems = winNew(FarMenuItem, a.Size+a.Delta);
                if ( a.Count != 0 )
                {
                    memcpy(newItems, a.Items, a.Count*sizeof(FarMenuItem));
                    winDel(a.Items);
                }
                a.Items = newItems;
                a.Size += a.Delta;
            }

            a.Items[a.Count].Selected = a.Items[a.Count].Checked = a.Items[a.Count].Separator = false;

            FSF.sprintf(a.Items[a.Count].Text, "%-20s [%s]", className, path);
            a.Count++; // Increase counter
        }
        else if (strcmp(className, keyWord)>0)
        {
            break;
        }
    }
    while (cfb->ReadClass(path, className) );

    *menu = a.Items;
    return a.Count;
}

/******************************************************************************/

bool OpenBrowser( WriteFileBuf * wfb, char * helpFile, char * path)
{
    char  fullCmd[1024];
    DWORD dataSize = sizeof( fullCmd);
    DWORD type;
    HKEY hKey;
    int exitCode;

    if ((exitCode = RegOpenKeyEx(HKEY_CLASSES_ROOT,"http\\shell\\open\\command",0,KEY_QUERY_VALUE,&hKey)) != ERROR_SUCCESS )
    {
        char err[100];
        FSF.sprintf(err, "Cannot open registry: %d!", exitCode);
        wfb->WriteLine(err);
        return false;
    }

    exitCode = RegQueryValueEx(hKey,"",NULL,&type,(BYTE*)fullCmd,&dataSize);
    RegCloseKey(hKey);

    if ( hKey == NULL || exitCode != ERROR_SUCCESS )
    {
        char err[100];
        FSF.sprintf(err, "Cannot read registry: %d!", exitCode);
        wfb->WriteLine(err);
        return false;
    }

    strcat(fullCmd, " \"");
    char * ptrDir = strrchr(helpFile, '\\');
    if (ptrDir) ptrDir[1] = 0;
    strcat(fullCmd, helpFile); // Now contains only directory
    strcat(fullCmd, path);
    strcat(fullCmd, "\"");

    static STARTUPINFO si;
    memset(&si,0,sizeof(si));
    si.cb      = sizeof(si);

    static PROCESS_INFORMATION pi;
    memset(&pi,0,sizeof(pi));


    if (!CreateProcess(NULL,fullCmd,NULL,NULL,true,0,NULL,NULL,&si,&pi))
    {
        wfb->WriteLine("Cannot launch internet browser!");
        return false;
    }
    return true;
}

/******************************************************************************/

bool WINAPI _export OpenHelp(struct PluginStartupInfo *PSInfo,
                             struct BlockProData *Data)
{
    Info     = *PSInfo;
    FSF      = *PSInfo->FSF;
    Info.FSF = &FSF;

    char helpFile[NM];
    char keyWord[80];

    ReadFileBuf  rfb(Data->inputFile);
    WriteFileBuf wfb(Data->outputFile);

    if (rfb.Error() || wfb.Error())
    {
        return false;
    }

    if (!rfb.ReadLine() || rfb.Length() == 0)
    {
        wfb.WriteLine("No word selected!");
        return false;
    }

    strncpy( keyWord, rfb.LastLine(), sizeof(keyWord));
    keyWord[sizeof(keyWord)-1] = 0;
    FSF.LTrim(keyWord);

    char * ptr = keyWord;
    while (*ptr)
    {
        if (*ptr++ < 0x20)
        {
            wfb.WriteLine("Invalid characters!");
            return false;
        }
    }

    strcpy(helpFile, Data->commands);
    FSF.LStrupr(helpFile);

    if (helpFile[0] == '\"')
    {
        strcpy( helpFile, helpFile+1);
        helpFile[strlen(helpFile)-1] = 0;
    }

    if (strlen(helpFile)==0)
    {
        wfb.WriteLine("Path to help file is missing!");
        return false;
    }

    ClassFileBuf hlp(helpFile, FILE_FLAG_RANDOM_ACCESS);

    if (hlp.Error())
    {
        wfb.WriteLine("Help file not found!");
        return false;
    }

    char path[512];
    char className[80];

    long size = hlp.Size();
    long lo   = 0;
    long hi   = size-1;
    long cur  = size/2;
    long last = 0;

    int cmp = -1;

    while (lo<cur && cur<hi && last != cur && cmp!=0)
    {
        last = cur;

        if (!hlp.Seek(cur))
        {
            wfb.WriteLine("Cannot seek!");
            return false;
        }

        if (!hlp.ReadClass(path, className))
        {
            wfb.WriteLine("Cannot find the class name!");
            return false;
        }

        if (StrBegin(className, keyWord)) break;

        cmp = stricmp(className, keyWord);

        if (cmp>0) // Our keyword is before this
        {
            //InfoMsg(true, "Before", "OK", "Class: %s", className);
            hi = cur;
        }
        else if (cmp<0) // Our keyword is after this
        {
            //InfoMsg(true, "After", "OK", "Class: %s", className);
            lo = cur;
        }

        cur  = (lo+hi)/2;
    }

    cur = hlp.Tell();

    while ( (StrBegin(className,keyWord) &&
             stricmp(className,keyWord)!=0) ||
            stricmp(className,keyWord)>0)
    {
        // InfoMsg(true, "Jump back", "OK", "Class: %s", className);
        cur -= 2*256;
        if (cur<0) break;
        hlp.Seek(cur);
        if (!hlp.ReadClass(path, className)) break;
    }

    int retCode, n    = -1;
    int breakKeys[2]  = { VK_RETURN, 0 };
    int breakKeysCount= sizeof(breakKeys)/sizeof(int) - 1;
    FarMenuItem *menu = NULL;

    int found = BuildMenu(&hlp, &menu, keyWord);

    switch (found)
    {
        case 0:
                wfb.WriteLine("Cannot find matching class!");
                winDel(menu);
                return false;

        case 1:
                n = 0;
                break;

        default:
                // Select item from menu
                n = Info.Menu(Info.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE,
                            "Select class", "Esc,Enter",
                            "Select class", breakKeys, &retCode, menu, found);

                if (retCode != 0 || n<0)
                {
                    winDel( menu);
                    return true;
                }
    }

    char * lToken = strstr( menu[n].Text, "[");
    char * rToken = strstr( menu[n].Text, "]");

    if (!lToken || !rToken)
    {
        wfb.WriteLine("Internal error, path not found!");
        winDel(menu);
        return false;
    }

    *rToken = 0;
    strcpy( path, lToken +1);
    winDel(menu);

    return OpenBrowser(&wfb, helpFile, path);
}
