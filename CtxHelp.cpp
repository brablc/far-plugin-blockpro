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
#include <htmlhelp.h>

#include "bpplugin.h"

extern "C"
{
    bool WINAPI _export OpenHelp(struct PluginStartupInfo *PSInfo,
                                 struct BlockProData *Data);
};

bool FileExists( char * fileName)
{
    WIN32_FIND_DATA data;

    HANDLE hFile = FindFirstFile(fileName,&data);

    if (hFile == INVALID_HANDLE_VALUE )
    {
        return false;
    }
    else
    {
        bool retVal = true;
        if (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) retVal = false;
        FindClose( hFile);
        return retVal;
    }
}

int GetRegKey(HKEY hRoot,char *Key,char *ValueName,char *ValueData,char *Default,DWORD DataSize)
{
    HKEY hKey;
    if ( RegOpenKeyEx(hRoot,Key,0,KEY_QUERY_VALUE,&hKey) != ERROR_SUCCESS )
        return false;

    DWORD Type;

    int ExitCode = RegQueryValueEx(hKey,ValueName,0,&Type,(BYTE*)ValueData,&DataSize);

    RegCloseKey(hKey);

    if ( hKey == NULL || ExitCode != ERROR_SUCCESS )
    {
        strcpy(ValueData,Default);
        return false;
    }
    return true;
}

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

    bool result;

    if (strstr( helpFile, ".HLP"))
    {
        result = WinHelp( GetForegroundWindow(),  // handle of window requesting Help
                          helpFile,               // address of directory-path string
                          HELP_PARTIALKEY,        // type of Help
                          (DWORD)keyWord          // additional data
                        );
    }
    else
    {
        if (!FileExists(helpFile))
        {
            char shortName[NM];
            strcpy(shortName, helpFile);

            if (GetRegKey(HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\Microsoft\\Windows\\Help",
                        shortName,
                        helpFile,
                        "",
                        sizeof(helpFile)))
            {
                strcat( helpFile, "\\");
            }

            strcat( helpFile, shortName);
        }

        ReadFileBuf tf(helpFile);
        if (tf.Error())
        {
            char buff[NM+20];
            FSF.sprintf(buff, "Path to help file %s is invalid!", helpFile);
            wfb.WriteLine(buff);
            return false;
        }

        HH_AKLINK link;
        link.cbStruct     = sizeof(HH_AKLINK) ;
        link.fReserved    = FALSE ;
        link.pszKeywords  = keyWord;
        link.pszUrl       = NULL ;
        link.pszMsgText   = NULL ;
        link.pszMsgTitle  = NULL ;
        link.pszWindow    = NULL ;
        link.fIndexOnFail = TRUE ;

        result = HtmlHelp( NULL, // GetForegroundWindow(), // handle of window requesting Help
                           helpFile,              // address of directory-path string
                           HH_KEYWORD_LOOKUP,     // type of Help
                           (DWORD)&link           // additional data
                         );
    }

    if (!result)
    {
        wfb.WriteLine("Error calling API function!");
    }

    return result;
}
