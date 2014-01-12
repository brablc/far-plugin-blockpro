/*
 * Copyright (c) Ondrej Brablc, 2002 <far@brablc.com>
 * You can use, modify, distribute this source code  or
 * any other parts of BlockPro plugin only according to
 * BlockPro  License  (see  \Doc\License.txt  for  more
 * information).
 */

#include <plugin.hpp>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys\timeb.h>

#include <renreg.hpp>
#include <scanreg.h>
#include <scanfile.h>
#include <farintf.h>
#include <winmem.h>
#include <filebuf.hpp>
#include <rtparams.hpp>
#include <oemcoder.hpp>

#include "bpplugin.h"
#include "blockpro.h"
#include "blockprolng.h"

#define ENABLED     ((int)(pow(2,Glob.openedFrom)))
#define DISABLED(x) (x & ENABLED?1:0)

#define MAX_LINE_LEN  4096
#define MAX_VIEW_LINE   60
#define START_LINE (ei.BlockType!=BTYPE_NONE?ei.BlockStartLine:ei.CurLine);
#define MENU_TAIL " "
#define EXT_MENU  '.'

char * REG_Main          = "Menu";
char * REG_Debug         = "Debug";
char * REG_AddToPanels   = "ShowInPanels";
char * REG_BackupFiles   = "BackupFiles";
char * REG_Last          = "LastCommand";
char * REG_Lines         = "LinesForPreview";
char * REG_LastLine      = "LastLine";
char * REG_LastFile      = "LastFile";
char * REG_NonPersistent = "NonPersistent";
char * REG_RememberDelay = "RememberDelay";

static char * REG_PARAMS[] =
{
    "DisabledMask",
    "IsSubmenu",
    "Hotkey",
    "Command",
    "ProcessInput",
    "ProcessInputChoice",
    "IncludeSubdir",
    "ProcessSelection",
    "ProcessSelectionChoice",
    "ProcessOutput",
    "ProcessOutputChoice",
    "ShowPreview",
    "CopyToClipboard",
    "KeepAutoSelection",
    "RemoveAnySelection",
    "IgnoreReturnCode",
    "AutoSave",
    "FixedPath"
};

struct Global
{
    bool hasFirstLine;
    bool hasLastLine;
    char firstLine[MAX_LINE_LEN];
    char lastLine[MAX_LINE_LEN];
    char fullKeyName[512];
    char pluginPath[NM];
    long height;
    long curPos;
    int  openedFrom;

    void Reset();
} Glob;

void Global::Reset()
{
    hasFirstLine = false;
    hasLastLine  = false;
    *firstLine   = 0;
    *lastLine    = 0;
    // *fullKeyName = 0;  ! Never reset this !
    height       = 0;
    curPos       = 0;
}

RunTimeParameters RunTimeParam;
char              WordDiv[80];
int               WordDivLen;

struct Options
{
    int addToPluginsMenu;
    int backupFiles;
    int linesToDisplay;
    int nonPersistentBlock;
    int rememberDelay;
    int debug;
    int  lastLine;       // Last selected line
    char lastFile[NM];   // Lats processed file
    char lastOutput[NM]; // Used for jump line menu
} Opt;

struct Parameters
{
    char regKey[512];
    char regRoot[512];

    char name[128];
    int  isDisabled;
    int  isMenu;
    char hotkey[128];
    char command[NM*4];
    int  processInput;
    int  processInputChoice;
    int  includeSubdir;
    int  processSelection;
    int  processSelectionChoice;
    int  processOutput;
    int  processOutputChoice;
    int  preview;
    int  copyToClipboard;
    int  keepAutoSelect;
    int  removeAnySelect;
    int  ignoreReturnCode;
    int  autoSave;
    char fixedPath[NM];

    bool Delete(char * pRoot, char * pName);
    void Get(char * pRoot, char * pName);
    void GetHotKey(const char * pRoot, char * pName);
    void Init(char * psRoot);
    void Set();
    void Toggle(char * pRoot, char * pName);

} Param, TempParam;

/******************************************************************************/

static void OpenJumpMenu();

/******************************************************************************/


void ErrorMsg(const char *fmt,...)
{
    char Msg[256];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(Msg, fmt, argptr);
    va_end(argptr);

    const char * MsgItems[]={GetMsg(MCaption),Msg,GetMsg(MOK)};
    Info.Message(Info.ModuleNumber,FMSG_WARNING,NULL,EXARR(MsgItems),1);
}

bool FileExists( char * fileName, bool allowDir = false)
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
        if (!allowDir && data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) retVal = false;
        FindClose( hFile);
        return retVal;
    }
}

bool WriteBufferToFile( const char * fileName, const char * buffer = NULL, bool eol = false)
{
    WriteFileBuf wfb( fileName);

    if (wfb.Error())
    {
        return false;
    }

    if (buffer)
    {
        wfb.Write( buffer);
        if (eol) wfb.WriteLine(NULL);

    }

    return true;
}

bool CreateEmptyFile( char * fileName)
{
    return WriteBufferToFile( fileName);
}

void WinError(char *fmt,...)
{
    char Msg[256];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(Msg, fmt, argptr);
    va_end(argptr);

    LPVOID lpMsgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL
    );

    char FullMsg[2048];

    sprintf(FullMsg,"%s\n%s\n%s", GetMsg(MSystemError), Msg, lpMsgBuf);
    Info.Message(Info.ModuleNumber,FMSG_MB_OK|FMSG_WARNING|FMSG_LEFTALIGN|FMSG_ALLINONE,
                         NULL,(const char **)FullMsg,0,0);

    LocalFree( lpMsgBuf );
}

char *GetOnlyName(char *FullName)
{
    char *Name=strrchr(FullName,'\\');
    if(Name) ++Name;
    else Name=FullName;
    return Name;
}

char *GetFullName(char *Dest,const char *Dir,char *Name)
{
    lstrcpy(Dest,Dir);
    int len=lstrlen(Dest);
    if(len)
    {
        if(Dest[len-1]=='\\') --len;
        else Dest[len]='\\';
        lstrcpy(Dest+len+1,GetOnlyName(Name));
    }
    else lstrcpy(Dest, Name);

    return Dest;
}

/******************************************************************************/

void Parameters::Init(char * pRoot)
{
    strcpy(regRoot,pRoot);
    *regKey                = 0;
    *name                  = 0;
    isDisabled             = 0;
    isMenu                 = 0;
    *hotkey                = 0;
    *command               = 0;
    processInput           = 1;
    processInputChoice     = 0;
    includeSubdir          = 0;
    processSelection       = 0;
    processSelectionChoice = 0;
    processOutput          = 1;
    processOutputChoice    = 0;
    preview                = 0;
    copyToClipboard        = 0;
    keepAutoSelect         = 0;
    removeAnySelect        = 0;
    ignoreReturnCode       = 0;
    autoSave               = 0;
    *fixedPath             = 0;
}

void Parameters::Get(char * pRoot, char * pName)
{
    Init( pRoot);
    strncpy(name,pName,sizeof(name));
    name   [sizeof(name)   -1]=0;
    FSF.sprintf (regKey,"%s\\%s", regRoot, name);

    int i = 0;
    isDisabled = DISABLED(GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],isDisabled));
    isMenu     = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],isMenu);

    GetRegKey(HKEY_CURRENT_USER,regKey, REG_PARAMS[i++], hotkey,  "", sizeof(hotkey)-1);
    GetRegKey(HKEY_CURRENT_USER,regKey, REG_PARAMS[i++], command, "", sizeof(command)-1);

    hotkey [sizeof(hotkey) -1]=0;
    command[sizeof(command)-1]=0;

    if (!isMenu)
    {
        processInput           = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processInput     );
        processInputChoice     = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processInputChoice     );
        includeSubdir          = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],includeSubdir);
        processSelection       = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processSelection   );

        // Swap 1 with two when reading
        processSelection = (processSelection==0?0:(processSelection==1?2:1));

        processSelectionChoice = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processSelectionChoice   );
        processOutput          = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processOutput    );
        processOutputChoice    = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processOutputChoice    );
        preview                = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],preview   );
        copyToClipboard        = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],copyToClipboard);
        keepAutoSelect         = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],keepAutoSelect);
        removeAnySelect        = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],removeAnySelect);
        ignoreReturnCode       = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],ignoreReturnCode);
        autoSave               = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],autoSave);
        GetRegKey(HKEY_CURRENT_USER,regKey, REG_PARAMS[i++], fixedPath,  "", sizeof(fixedPath)-1);
    }
}

void Parameters::GetHotKey(const char * pRoot, char * pName)
{
    strcpy(regRoot,pRoot);
    FSF.sprintf (regKey,"%s\\%s", regRoot, strcpy(name,pName));

    int i = 0;
    isDisabled = DISABLED(GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++], 0));
    isMenu     = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++], 0);
    GetRegKey(HKEY_CURRENT_USER,regKey, REG_PARAMS[i++], hotkey,  "", sizeof(hotkey)-1);
}

bool Parameters::Delete(char * pRoot, char * pName)
{
    strcpy(regRoot,pRoot);
    FSF.sprintf (regKey,"%s\\%s", regRoot, strcpy(name,pName));

    DeleteRegKey(HKEY_CURRENT_USER, regKey);
    isMenu = GetRegKey(HKEY_CURRENT_USER,regKey, REG_PARAMS[1], 0);
    return (isMenu==1);
}

void Parameters::Set()
{
    if (*regKey) // Delete old regKey
    {
        if (isMenu)
        {
            char newKey[512];
            FSF.sprintf (newKey,"%s\\%s", regRoot, name);

            if (strcmp(regKey, newKey) != 0)
            {
                char oldReg[512];
                char newReg[512];

                FSF.sprintf( oldReg, "%s\\%s", PluginRootKey, regKey);
                FSF.sprintf( newReg, "%s\\%s", PluginRootKey, newKey);

                RenameRegistryItem(oldReg, newReg);
            }
        }
        else
        {
            DeleteRegKey(HKEY_CURRENT_USER, regKey);
        }
    }

    FSF.sprintf (regKey,"%s\\%s", regRoot, name);

    int i = 1; //Skip isDisabled

    if (isMenu)
    {
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i], isMenu);
    }

    i++;

    if (*FSF.RTrim(hotkey))
    {
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i], hotkey);
    }

    i++;

    if (*FSF.RTrim(command))
    {
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i], command);
    }

    i++;

    if (!isMenu)
    {
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processInput     );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processInputChoice     );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],includeSubdir     );

        // Swap 1 with two when saving
        int saveProcessSelection = (processSelection==0?0:(processSelection==1?2:1));

        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],saveProcessSelection   );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processSelectionChoice   );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processOutput    );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],processOutputChoice    );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],preview           );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],copyToClipboard   );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],keepAutoSelect   );
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],removeAnySelect);
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],ignoreReturnCode);
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],autoSave);
        SetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[i++],fixedPath);
    }
}

void Parameters::Toggle(char * psRoot, char * pName)
{
    Param.Get(psRoot, pName);

    int maskIsDisabled = GetRegKey(HKEY_CURRENT_USER,regKey,REG_PARAMS[0],0);

    if (isDisabled) // Enable now
    {
        maskIsDisabled -= ENABLED;
    }
    else            // Disable now
    {
        maskIsDisabled += ENABLED;
    }

    SetRegKey(HKEY_CURRENT_USER, regKey, REG_PARAMS[0], maskIsDisabled);
}

/******************************************************************************/

bool CopyToTempFile( char * pTempFile, bool bFullLines)
{
    struct EditorGetString     egs;
    struct EditorInfo          ei;
    struct EditorSelect        sel;

    DWORD  dWritten;
    static SECURITY_ATTRIBUTES sa;
    const char* lStart;
    int    lWidth;

    memset(&sa, 0, sizeof(sa));
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle       = true;

    Info.EditorControl(ECTL_GETINFO, &ei);

    Glob.curPos  = ei.CurPos;

    HANDLE hFile = CreateFile(pTempFile, GENERIC_WRITE,
                              FILE_SHARE_READ,&sa,CREATE_ALWAYS,
                              FILE_FLAG_SEQUENTIAL_SCAN,NULL);

    if (hFile == INVALID_HANDLE_VALUE )
    {
        WinError("Error opening input file: %s", pTempFile);
        return false;
    }

    Glob.height = 0;

    if (ei.BlockType == BTYPE_NONE)
    {
        egs.StringNumber = ei.CurLine;

        Info.EditorControl(ECTL_GETSTRING, &egs);

        Glob.height++;

        if (bFullLines)
        {
            OEMCoder * oem = NULL;

            if (ei.AnsiMode) oem = new OEMCoder(egs);

            WriteFile( hFile,
                       (oem?oem->ToOEM():egs.StringText),
                       egs.StringLength,
                       &dWritten,
                       NULL);

            if (oem) delete oem;
        }
        else
        {
            if (ei.CurPos > egs.StringLength)
            {
                ei.CurPos = egs.StringLength;
            }

            Glob.hasFirstLine = true;
            Glob.hasLastLine  = true;

            strncpy(Glob.firstLine, egs.StringText, ei.CurPos);
            Glob.firstLine[ei.CurPos] = 0;
            strcpy(Glob.lastLine,   egs.StringText + ei.CurPos);
        }

        WriteFile( hFile,
                   "\x0D\x0A",
                   2,
                   &dWritten,
                   NULL);
    }
    else
    {
        // Scan the file line by line
        for ( int i = ei.BlockStartLine; i < ei.TotalLines; i++ )
        {
            egs.StringNumber = i;
            Info.EditorControl(ECTL_GETSTRING, &egs);

            // Break if line not selected
            if (egs.SelStart == -1 || (egs.SelStart == 0 && egs.SelEnd == 0)) break;

            lWidth = egs.StringLength;

            if (bFullLines)
            {
                lStart = egs.StringText;
            }
            else
            {
                lStart = egs.StringText + egs.SelStart;

                if (ei.BlockType == BTYPE_COLUMN)
                {
                    if (egs.SelStart > egs.StringLength)
                    {
                        lWidth = 0;
                    }
                    else
                    {
                        lWidth = (egs.StringLength<egs.SelEnd?egs.StringLength:egs.SelEnd) - egs.SelStart;
                    }
                }
                else
                {
                    bool handled = false;

                    if (egs.SelStart != 0) // First incomplete line
                    {
                        strncpy( Glob.firstLine, egs.StringText, egs.SelStart);
                        Glob.firstLine[egs.SelStart] = 0;
                        Glob.hasFirstLine  = true;
                        lWidth      -= egs.SelStart;
                        handled      = true;
                    }

                    if (egs.SelEnd   != -1) // Last incomplete line
                    {
                        strncpy( Glob.lastLine, egs.StringText + egs.SelEnd, egs.StringLength - egs.SelEnd );
                        Glob.lastLine[egs.StringLength - egs.SelEnd] = 0;
                        Glob.hasLastLine  = true;
                        lWidth      = egs.SelEnd - egs.SelStart;
                        handled     = true;
                    }

                    if (!handled) // Full line
                    {
                        lWidth = egs.StringLength;
                    }
                }
            }

            OEMCoder * oem = NULL;

            if (ei.AnsiMode) oem = new OEMCoder(lStart,lWidth);

            WriteFile( hFile,
                       (oem?oem->ToOEM():lStart),
                       lWidth,
                       &dWritten,
                       NULL);

            if (oem) delete oem;

            WriteFile( hFile,
                       "\x0D\x0A",
                       2,
                       &dWritten,
                       NULL);

            Glob.height++;
        }
    }

    CloseHandle(hFile);

    return true;
}

bool CopyNameTempFile( char * pTempFile)
{
    struct EditorInfo          ei;
    DWORD  dWritten;
    static SECURITY_ATTRIBUTES sa;
    char   FileName[NM+2];

    memset(&sa, 0, sizeof(sa));
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle       = true;

    HANDLE hFile = CreateFile(pTempFile, GENERIC_WRITE,
                              FILE_SHARE_READ,&sa,CREATE_ALWAYS,
                              FILE_FLAG_SEQUENTIAL_SCAN,NULL);

    if (hFile == INVALID_HANDLE_VALUE )
    {
        WinError("Error opening input file: %s", pTempFile);
        return false;
    }

    Info.EditorControl(ECTL_GETINFO, &ei);

    strcpy(FileName, ei.FileName);
    strcat(FileName, "\x0D\x0A");
    WriteFile( hFile, FileName, strlen(FileName), &dWritten, NULL);
    CloseHandle(hFile);

    return true;
}

void CopyFromTempFile( char * pTempFile, bool bFullLines)
{
    struct EditorGetString   egs;
    struct EditorGetString   egsEOL;
    struct EditorSetString   ess;
    struct EditorSelect      sel;
    struct EditorInfo        ei;
    struct EditorSetPosition esp;
    int    lWidth, lLen;

    Info.EditorControl(ECTL_GETINFO, &ei);

    ReadFileBuf rfb(pTempFile);

    if (rfb.Error())
    {
        WinError("Cannot read results!");
        return;
    }

    // Column block - block driven
    if (ei.BlockType == BTYPE_COLUMN && !bFullLines)
    {
        // Scan the file line by line
        for ( int i = ei.BlockStartLine; i < ei.TotalLines; i++ )
        {
            egs.StringNumber = i;
            Info.EditorControl(ECTL_GETSTRING, &egs);

            // Break if line not selected
            if (egs.SelStart == -1)
            {
                break;
            }

            // Width for column blocks
            lWidth = egs.SelEnd - egs.SelStart;

            char * lString = winNew(char, (egs.SelEnd>egs.StringLength?egs.SelEnd:egs.StringLength) + 1);
            strcpy( lString, egs.StringText);
            lString[egs.StringLength] = 0;

            rfb.ReadLine();
            if (ei.AnsiMode) OEMCoder::convertFromOEM(rfb.LastLine(),rfb.Length());

            // If too short then fill with spaces
            if (strlen( lString) < (unsigned)egs.SelEnd)
            {
                memset( lString + strlen( lString), 0x20, egs.SelEnd - strlen( lString));
                lString[ egs.SelEnd ] = 0;
            }

            memset( lString + egs.SelStart, 0x20, lWidth);

            // Set the max length
            memcpy( lString + egs.SelStart,
                    rfb.LastLine(),
                    min(lWidth,(signed)rfb.Length()));

            ess.StringNumber = egs.StringNumber;
            ess.StringEOL    = const_cast<char*>(egs.StringEOL);
            ess.StringLength = strlen(lString);
            ess.StringText   = lString;
            Info.EditorControl(ECTL_SETSTRING, &ess);

            winDel( lString);
        }
    }
    else // Stream - result driven or full lines
    {
        int  dStart     = START_LINE;
        int  dLine      = 0;
        int  dTotal     = ei.TotalLines;
        char sEOL[3]    = "\r\n";

        // Get string for EOL
        egsEOL.StringNumber   = dStart;
        Info.EditorControl(ECTL_GETSTRING, &egsEOL);
        if (strlen(egsEOL.StringEOL)!=0)
        {
            strcpy(sEOL,egsEOL.StringEOL);
        }

        esp.TopScreenLine  = ei.TopScreenLine;
        esp.LeftPos        = 0;
        esp.CurTabPos      = ei.CurTabPos;
        esp.Overtype       = ei.Overtype;
        esp.CurLine        = dStart;

        sel.BlockStartLine = dStart;
        sel.BlockStartPos  = strlen(Glob.firstLine);

        bool ret = rfb.ReadLine();
        if (ei.AnsiMode) OEMCoder::convertFromOEM(rfb.LastLine(),rfb.Length());

        dLine = dStart;

        if (ret)
        {
            do
            {
                char * sBuffer;
                int    lBuffer = rfb.Length();

                // Glob.lastLine is needed only for the last lien
                sBuffer = winNew( char, lBuffer +
                                        strlen(Glob.firstLine) +
                                        strlen(Glob.lastLine)  + 1);

                if (Glob.hasFirstLine && dLine == dStart)
                {
                    lBuffer += strlen(Glob.firstLine);
                    strcpy(sBuffer, Glob.firstLine);
                }
                else
                {
                    *sBuffer = 0;
                }

                strncat(sBuffer, rfb.LastLine(), rfb.Length());
                sBuffer[lBuffer] = 0;
                ret = rfb.ReadLine();
                if (ei.AnsiMode) OEMCoder::convertFromOEM(rfb.LastLine(),rfb.Length());

                if (!ret && Glob.hasLastLine)
                {
                    lBuffer += strlen(Glob.lastLine);
                    strcat(sBuffer, Glob.lastLine);
                }

                ess.StringNumber  = dLine;
                ess.StringEOL     = sEOL;
                ess.StringText    = sBuffer;
                ess.StringLength  = lBuffer;

                // Too many lines or we are on the last line
                if ((dLine - dStart)>=Glob.height )
                  //|| dLine >= (dTotal-1)) //!! Might be correct but caused error when changing last line without CR
                {
                    esp.TopScreenLine  = ei.TopScreenLine;
                    esp.CurTabPos      = esp.Overtype = -1;
                    esp.CurLine        = dLine;
                    esp.CurPos         = 0;
                    esp.LeftPos        = 0;
                    Info.EditorControl(ECTL_SETPOSITION,  &esp);
                    Info.EditorControl(ECTL_INSERTSTRING, NULL);
                    Info.EditorControl(ECTL_SETSTRING,    &ess);
                    dTotal += 1;
                }
                else
                {
                    egs.StringNumber  = dLine;
                    Info.EditorControl(ECTL_GETSTRING, &egs);

                    if (egs.StringLength != ess.StringLength ||
                        strncmp(ess.StringText,egs.StringText,egs.StringLength) != 0)
                    {
                        Info.EditorControl(ECTL_SETSTRING,&ess);
                    }
                }

                winDel(sBuffer);
                dLine++;

            } while (ret);
        }

        // The output is smaller - delete the lines
        if (Glob.height>(dLine - dStart))
        {
            esp.TopScreenLine  = ei.TopScreenLine;
            esp.CurTabPos      = esp.Overtype = -1;
            esp.CurLine        = dLine;
            esp.CurPos         = 0;
            esp.LeftPos        = 0;
            Info.EditorControl(ECTL_SETPOSITION,  &esp);

            for (int i = 0; i < Glob.height - (dLine - dStart); i++)
            {
                Info.EditorControl(ECTL_DELETESTRING, NULL);
            }
        }

        sel.BlockType   = BTYPE_STREAM;
        sel.BlockHeight = dLine - dStart;

        if (Glob.hasLastLine)
        {
            int X1 = strlen(Glob.firstLine);

            egs.StringNumber = ess.StringNumber;
            Info.EditorControl(ECTL_GETSTRING, &egs);

            int X2 = egs.StringLength - strlen(Glob.lastLine);

            sel.BlockWidth  = X2 - X1;
            esp.CurPos      = X2;
            esp.CurLine     = dLine -1;
        }
        else
        {
            sel.BlockWidth  = -1;
            esp.CurPos      = Glob.curPos;
            esp.CurLine     = dLine;
        }

        if (ei.BlockType != BTYPE_COLUMN)
        {
            Info.EditorControl(ECTL_SELECT,  &sel);
        }

        if ( esp.CurLine > (ei.TopScreenLine + ei.WindowSizeY))
        {
            esp.TopScreenLine  = esp.CurLine - ei.WindowSizeY + 1;
        }

        esp.LeftPos        = -1;
        esp.CurTabPos      = -1;
        esp.Overtype       = ei.Overtype;

        Info.EditorControl(ECTL_SETPOSITION,&esp);
    }
}

int ReadFileToBuffer( char * psFile, char * psBuffer, int piLen)
{
    int length = strlen(psBuffer);
    int lines  = Opt.linesToDisplay;
    struct EditorInfo ei;

    ReadFileBuf rfb(psFile);

    if (rfb.Error())
    {
        WinError("Cannot open file!");
        return 0;
    }

    if (Glob.openedFrom == OPEN_EDITOR)
    {
        Info.EditorControl(ECTL_GETINFO, &ei);
    }


    while (lines && length < piLen && rfb.ReadLine())
    {
        int freeSpace = min( (unsigned long)(piLen - length), rfb.Length());

        strncat(psBuffer, rfb.LastLine(), freeSpace );
        length += freeSpace;
        psBuffer[length] = 0;
        strcat(psBuffer, "\n");
        length += 2;

        lines--;
    }

    return length;
}

int ShowResult(char * pFile)
{
     char Msg[2048];
     FSF.sprintf(Msg, "%s\n", GetMsg(MCaption));
     ReadFileToBuffer( pFile, Msg, sizeof(Msg)-1);
     return Info.Message(Info.ModuleNumber, FMSG_MB_OKCANCEL|FMSG_LEFTALIGN|FMSG_ALLINONE,
                         NULL,(const char **)Msg,0,0);
}

void ShowError(char * pFile)
{
    char Msg[2048];
    FSF.sprintf(Msg, "%s\n", GetMsg(MRunError));
    ReadFileToBuffer( pFile, Msg, sizeof(Msg)-1);
    Info.Message(Info.ModuleNumber,
                 FMSG_WARNING|FMSG_LEFTALIGN|FMSG_MB_OK|FMSG_ALLINONE,
                 NULL,(const char **)Msg,0,0);

}

void CopyToClipboard(char * pFile, struct EditorInfo & ei)
{
    ReadFileBuf rfb( pFile );

    if (rfb.Error())
    {
        WinError("Cannot open file!");
        return;
    }

    if (rfb.ReadLine())
    {
        FSF.CopyToClipboard(rfb.LastLine());
    }
}

bool ReplacePlaceHolder( char * pCmd, char * pHolder, char * pValue)
{
    char * ptr;
    bool  found = false;

    while ((ptr = strstr( pCmd, pHolder)))
    {
        found = true;
        char * tmp = winNew(char,strlen(ptr+strlen(pHolder))+1);
        strcpy( tmp, ptr+strlen(pHolder));
        ptr[0] = 0;
        strcat( pCmd, pValue);
        strcat( pCmd, tmp);
        winDel(tmp);
    }

    return found;
}

#define RPH_FNC( fmt ) if (strstr(pCmd, fmt )) ReplacePlaceHolder( pCmd, fmt, fnc.get(fmt,output))
void ExpandFilename( char * pCmd, int size, const char * pFullName)
{
    struct EditorInfo ei;
    const char * fileName = pFullName;
    char * output   = winNew( char, size);

    if (!fileName)
    {
        Info.EditorControl(ECTL_GETINFO, &ei);
        fileName = ei.FileName;
    }

    FileNameConvertor fnc( fileName);

    RPH_FNC("!\\!.!");
    RPH_FNC("!~");
    RPH_FNC("!.!");
    RPH_FNC("!-!");
    RPH_FNC("!:");
    RPH_FNC("!\\");
    RPH_FNC("!/");
    RPH_FNC("!");

    winDel( output);
}
#undef RPH_FNC

void ExpandMacros(char * pCmd, bool bReplace)
{
    struct EditorInfo        ei;
    struct EditorGetString   egs;
    char   start[10]   = "0",
           width[10]   = "0",
           tabsize[10] = "";

    if (bReplace)
    {
        Info.EditorControl(ECTL_GETINFO, &ei);
        switch (ei.BlockType)
        {
            case BTYPE_NONE:
                sprintf( start, "%d", ei.CurPos);
                break;

            default:
                egs.StringNumber = ei.BlockStartLine;
                Info.EditorControl(ECTL_GETSTRING, &egs);
                if (ei.BlockType == BTYPE_STREAM && (egs.SelStart==-1 || egs.SelEnd==-1)) break;
                sprintf( start, "%d", egs.SelStart );
                sprintf( width, "%d", egs.SelEnd - egs.SelStart);
        }

        sprintf( tabsize, "%d", ei.TabSize );
        ReplacePlaceHolder( pCmd,"!?TABSIZE?!", tabsize);
    }

    ReplacePlaceHolder( pCmd,"!?BPHOME?!",  Glob.pluginPath);
    ReplacePlaceHolder( pCmd,"!?START?!",   start);
    ReplacePlaceHolder( pCmd,"!?WIDTH?!",   width);
}

void EditorProcessVKey(WORD vkey, int state)
{
    INPUT_RECORD tr;
    tr.EventType = KEY_EVENT;
    tr.Event.KeyEvent.bKeyDown = true;
    tr.Event.KeyEvent.wRepeatCount = 1;
    tr.Event.KeyEvent.wVirtualKeyCode = vkey;
    tr.Event.KeyEvent.dwControlKeyState = state;
    Info.EditorControl(ECTL_PROCESSINPUT, &tr);
}

bool ProcessInput( char * inputFile, bool & autoSelect, struct EditorInfo & ei)
{
    if (Param.processInput) // If processing input
    {
        switch (Param.processInputChoice)
        {
            case 0:
            case 1:
                if (Param.processSelection > 0)
                {
                    bool keepBlock = false;
                    struct EditorGetString egs;
                    egs.StringNumber = ei.CurLine;

                    if (ei.BlockType != BTYPE_NONE && Opt.nonPersistentBlock)
                    {
                        Info.EditorControl(ECTL_GETSTRING, &egs);

                        // Does not handle full lines
                        if (ei.CurPos == 0 && egs.SelStart == -1 && ei.CurLine>0)
                        {
                            struct EditorGetString egs2;
                            egs2.StringNumber = ei.CurLine - 1;
                            Info.EditorControl(ECTL_GETSTRING, &egs2);

                            if (egs2.SelEnd == -1)
                            {
                                keepBlock = true;
                            }
                        }
                    }

                    // Force autoselect by unselecting before
                    if (Param.processSelection == 1)
                    {
                        // But only if we are not inside the block
                        if (!keepBlock &&
                            !( egs.SelStart != -1 &&
                               egs.SelStart <= ei.CurPos &&
                              (egs.SelEnd   == -1 || ei.CurPos <= egs.SelEnd)))
                        {
                            EditorProcessVKey('U', LEFT_CTRL_PRESSED);
                            Info.EditorControl(ECTL_GETINFO, &ei);
                        }
                    }

                    if (ei.BlockType == BTYPE_NONE)
                    {
                        switch (Param.processSelectionChoice)
                        {
                            case 0: // word
                                if (ei.CurPos>0)
                                {
                                    struct EditorGetString egs;
                                    egs.StringNumber = ei.CurLine;
                                    Info.EditorControl(ECTL_GETSTRING, &egs);

                                    if (ei.CurPos<=egs.StringLength &&
                                        strchr(WordDiv,egs.StringText[ei.CurPos])  ==NULL &&
                                        strchr(WordDiv,egs.StringText[ei.CurPos-1])==NULL)
                                    {
                                        EditorProcessVKey(VK_LEFT,  LEFT_CTRL_PRESSED);
                                    }
                                }
                                // Select the word
                                EditorProcessVKey(VK_RIGHT, LEFT_CTRL_PRESSED|SHIFT_PRESSED);
                                break;

                            case 1: // line
                                EditorProcessVKey(VK_HOME,  0);
                                EditorProcessVKey(VK_END,   SHIFT_PRESSED);
                                break;

                            case 2: // file
                                EditorProcessVKey('A',      LEFT_CTRL_PRESSED);
                                break;
                        }

                        autoSelect = true;
                    }
                }

                if (!CopyToTempFile(inputFile,(Param.processInputChoice == 1)))
                {
                    return false;
                }
                break;

            case 2:
                if (!WriteBufferToFile(inputFile, ei.FileName, true))
                {
                    WinError("Cannot write own name to %s!", inputFile);
                    return false;
                }
                break;
        }
    }
    else
    {
        if (!CreateEmptyFile(inputFile))
        {
            WinError("Cannot create empty file %s!", inputFile);
            return false;
        }
    }

    return true;
}

void UnselectBlock( bool bAutoSelect, struct EditorInfo ei)
{
    struct EditorSetPosition esp;

    if (Param.removeAnySelect || (bAutoSelect && !Param.keepAutoSelect))
    {
        EditorProcessVKey('U', LEFT_CTRL_PRESSED);
        if (bAutoSelect)
        {
            esp.TopScreenLine  = ei.TopScreenLine;
            esp.CurTabPos      = esp.Overtype = -1;
            esp.CurLine        = ei.CurLine;
            esp.CurPos         = ei.CurPos;
            esp.LeftPos        = ei.LeftPos;
            Info.EditorControl(ECTL_SETPOSITION,  &esp);
        }
    }
}

void ProcessOutput(char * inputFile, char * outputFile, struct EditorInfo & ei)
{
    if (Param.preview && ShowResult(outputFile) != 0)
        return;

    if (Param.copyToClipboard)
        CopyToClipboard(outputFile, ei);

    if (!Param.processOutput)
        return;

    if (Param.processOutput==2)
    {
        bool lbAgain          = false;
        int retCode           = -1;
        int breakKeys[]       = { VK_RETURN, 0 };

        struct FarMenuItem menu[] =
        {
        /*00*/ { 0, 0, 0, 0 },
        /*01*/ { 0, 0, 0, 0 },
        /*02*/ { 0, 0, 0, 0 },
        /*03*/ { 0, 0, 0, 0 }
        };

        int count = sizeof(menu)/sizeof(menu[0]);

        strcpy(menu[0].Text,GetMsg(MOutputToEditor));
        strcpy(menu[1].Text,GetMsg(MOutputToNewEditor));
        strcpy(menu[2].Text,GetMsg(MOutputToNewViewer));
        strcpy(menu[3].Text,GetMsg(MOutputToJumpMenu));

        menu[Param.processOutputChoice].Selected = 1;

        int dSelected = Info.Menu(Info.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE,
                            GetMsg(MOutputToPrompt), NULL,
                            "Output", breakKeys, &retCode, menu, count);

        if (retCode == 0 || (retCode == -1 && dSelected>-1))
        {
            Param.processOutputChoice = dSelected;
        }
        else
        {
            return;
        }
    }

    if (Param.processOutputChoice==0 &&   // Current editor
        Glob.openedFrom != OPEN_EDITOR && // Not opened in editor
        (!Param.processInput || Param.processInputChoice==2)) // Entire file without
    {
        Param.processOutputChoice = 1;  // Handle into new editor
    }

    switch (Param.processOutputChoice)
    {
        case 0:
            if (Glob.openedFrom != OPEN_EDITOR)
            {
                if (Opt.backupFiles)
                {
                    char lBackup[NM];
                    strcpy(lBackup, inputFile);
                    char * ptr = strrchr( lBackup, '.');
                    if (ptr)
                    {
                        ptr[0]=0;
                    }
                    strcat( lBackup, ".bak");
                    CopyFileEx(inputFile, lBackup, NULL, NULL, FALSE, 0);
                }
                CopyFileEx( outputFile, inputFile, NULL, NULL, FALSE, 0);
                *inputFile = 0;
            }
            else
            {
                CopyFromTempFile(outputFile, (Param.processInputChoice == 1));
            }
            break;

        case 1:
            {
                char output[NM];
                char expanded[NM];

                if (*Param.fixedPath)
                {
                    FSF.ExpandEnvironmentStr(Param.fixedPath, expanded, sizeof(expanded));
                    strcpy(output, expanded);
                }
                else
                {
                    bool again;

                    do
                    {
                        again = false;

                        if (!Info.InputBox(GetMsg(MEditor),
                                          GetMsg(MEditorSaveTo),
                                          "NewEdit",NULL,
                                          output,NM,
                                          NULL,
                                          FIB_EXPANDENV|FIB_NOUSELASTHISTORY))
                        {
                            return;
                        }

                        FSF.ExpandEnvironmentStr(output, expanded, sizeof(expanded));
                        strcpy(output, expanded);

                        char msg[1024];
                        sprintf( msg, "%s\n%s", GetMsg(MOverWrite), output);

                        if (FileExists( output) &&
                            Info.Message(Info.ModuleNumber,
                                             FMSG_MB_OKCANCEL|FMSG_LEFTALIGN|FMSG_ALLINONE|FMSG_WARNING,
                                             NULL,(const char**)msg,0,0)!=0)
                        {
                            again = true;
                        }
                    }
                    while (again);
                }

                if (MoveFileEx( outputFile,output,MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED))
                {
                    Info.Editor(output,NULL,0,0,-1,-1,EF_NONMODAL,0,1);
                }
                else
                {
                    WinError("Cannot move the file to new location!");
                    return;
                }
            }
            break;

        case 2:
            Info.Viewer(outputFile,NULL,0,0,-1,-1,VF_NONMODAL|VF_DELETEONCLOSE);
            *outputFile = 0;
            break;

        case 3:
            {
                MoveFileEx( outputFile, Opt.lastOutput,MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED);

                Opt.lastLine = 0;

                if (Glob.openedFrom == OPEN_EDITOR)
                {
                    strcpy( Opt.lastFile, ei.FileName); // For jump line menu
                }

                SetRegKey(HKEY_CURRENT_USER,"",REG_LastFile,Opt.lastFile);
                OpenJumpMenu();

                if (Glob.openedFrom == OPEN_EDITOR)
                {
                    Info.EditorControl(ECTL_GETINFO, &ei);  // Update line number
                }
            }
            break;
    }
}

bool RunCommand(char * psRoot, char * pName, char * pFullName=NULL)
{
    char  lTmpInput[NM],
          lTmpOutput[NM],
          lPHInput[NM],
          lPHOutput[NM],
          lFullCmd[NM*10];

    struct EditorInfo ei;

    bool  bInput      = false;
    bool  bOutput     = false;
    bool  bProcessOK  = false;
    bool  bAutoSelect = false;

    DWORD lExitCode;

    HANDLE hScreen, hFile, hStdInput = NULL, hStdOutput = NULL;

    Param.Get( psRoot, pName);

    if (Glob.openedFrom == OPEN_EDITOR && Param.autoSave)
    {
        if (!Info.EditorControl(ECTL_SAVEFILE, NULL))
        {
            ErrorMsg("Error auto saving the file!");
            return false;
        }
    }

    strcpy( lFullCmd, Param.command);

    bool bpPlugin = (strstr(lFullCmd, BPPLUGINS_PREFIX) == lFullCmd);

    // Create temporary files for placeholders
    if (strchr( lFullCmd, '<'))
    {
        bInput = true;
        if (!FSF.MkTemp(lPHInput, "BIP"))
        {
            WinError("Cannot create input temp file!");
            return false;
        }
        ReplacePlaceHolder( lFullCmd, "<", lPHInput);
    }
    if (strchr( lFullCmd, '>'))
    {
        bOutput = true;
        if (!FSF.MkTemp(lPHOutput, "BOP"))
        {
            WinError("Cannot create input temp file!");
            return false;
        }
        ReplacePlaceHolder( lFullCmd, ">", lPHOutput);
    }

    bool exclamation = false;
    char * subst     = "\xFF­\xFF";

    if (strstr(lFullCmd, "!!"))
    {
        ReplacePlaceHolder( lFullCmd, "!!", subst);
        exclamation = true;
    }

    ExpandMacros( lFullCmd, Glob.openedFrom == OPEN_EDITOR && Param.processInputChoice!=2);
    if (!RunTimeParam.Translate(pName,lFullCmd,sizeof(lFullCmd))) return false;

    if (Glob.openedFrom == OPEN_EDITOR)
    {
        Info.EditorControl(ECTL_GETINFO, &ei);
        ExpandFilename( lFullCmd, sizeof(lFullCmd), ei.FileName);
    }
    else
    {
        if (pFullName)
        {
            ExpandFilename( lFullCmd, sizeof(lFullCmd), pFullName);
        }
    }

    if (exclamation)
    {
        ReplacePlaceHolder( lFullCmd, subst, "!");
    }

    if (!FSF.MkTemp(lTmpOutput,"BPO"))
    {
        WinError("Cannot create output temp file!");
        return false;
    }

    if (pFullName)
    {
        strcpy( Opt.lastFile, pFullName); // For jump line menu
        strcpy( lTmpInput, pFullName);
    }
    else
    {
        if (!FSF.MkTemp(lTmpInput, "BPI"))
        {
            WinError("Cannot create input temp file!");
            return false;
        }
    }

    Glob.Reset();

    if (!pFullName && !ProcessInput(lTmpInput, bAutoSelect, ei)) return false;

    if (bInput)
    {
        CopyFileEx( lTmpInput, lPHInput, NULL, NULL, FALSE, 0);
        CreateEmptyFile( lTmpInput);
    }

    char  sConsoleTitleOld[256];
    DWORD dTitleLen = GetConsoleTitle(sConsoleTitleOld, 256);
    char  sConsoleTitleNew[256];
    strncpy( sConsoleTitleNew, lFullCmd, 256);
    sConsoleTitleNew[255] = 0;
    SetConsoleTitle(sConsoleTitleNew);
    hScreen = Info.SaveScreen(0,0,-1,-1);

    char    lTmpCmd[NM*10];
    if (bpPlugin)
    {
        *lTmpCmd = 0;
    }
    else
    {
        strcpy( lTmpCmd,"%COMSPEC% /c ");
    }
    strcat( lTmpCmd, lFullCmd);
    FSF.ExpandEnvironmentStr( lTmpCmd, lFullCmd,sizeof(lFullCmd));

    if (bpPlugin)
    {
        // bpplugins:WindowsAPI#OpenHelp Commands

        char * t_col  = strchr( lFullCmd, ':');
        char * t_hash = strchr( t_col,    '#');

        if (!t_hash)
        {
            ErrorMsg("Improper form of BlockPro plugin call!");
            return FALSE;
        }

        char pluginName[NM];
        char pluginFunc[128];
        char pluginCmd[NM*10];

        strcpy(pluginFunc,   t_hash+1);
        *t_hash = 0;
        strcpy(pluginName, t_col+1);
        *t_col  = 0;

        char * t_spc  = strchr( pluginFunc, ' ');
        if (t_spc)
        {
            strcpy(pluginCmd, t_spc+1);
            *t_spc = 0; // Terminate function
        }
        else
        {
            *pluginCmd = 0;
        }

        char fullPath[NM];
        sprintf( fullPath, "%s\\%s\\%s.bpd", Glob.pluginPath, BPPLUGINS_DIR, pluginName);

        HINSTANCE dll = LoadLibrary(fullPath);
        if (dll)
        {
            BlockProFunction fnc = (BlockProFunction)GetProcAddress(dll,pluginFunc);
            BlockProData BPD;

            BPD.inputFile  = lTmpInput;
            BPD.outputFile = lTmpOutput;
            BPD.commands   = pluginCmd;

            if (fnc)
            {
                if (fnc(&Info,&BPD))
                {
                    bProcessOK=true;
                }
            }
            else
            {
                WinError("Cannot find function %s!", pluginFunc);
            }
        }
        else
        {
            WinError("Cannot load dynamic library %s!", fullPath);
        }

        FreeLibrary(dll);
    }
    else
    {
        InfoMsg(false, GetMsg(MCaption), GetMsg(MOK), GetMsg(MRunning));

        static SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof(sa));
        sa.nLength=sizeof(sa);
        sa.lpSecurityDescriptor=NULL;
        sa.bInheritHandle=true;

        static STARTUPINFO si;
        memset(&si,0,sizeof(si));
        si.cb      = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;

        hStdInput  = CreateFile(lTmpInput, GENERIC_READ,
                                FILE_SHARE_READ,&sa,OPEN_EXISTING,
                                FILE_FLAG_SEQUENTIAL_SCAN,NULL);

        if (hStdInput == INVALID_HANDLE_VALUE)
        {
            WinError("Error opening input file: %s", lTmpInput);
            if (!pFullName) DeleteFile( lTmpInput);
            return false;
        }

        hStdOutput = CreateFile(lTmpOutput,GENERIC_WRITE,
                                FILE_SHARE_READ,&sa,CREATE_ALWAYS,
                                FILE_FLAG_SEQUENTIAL_SCAN,NULL);

        if (hStdOutput == INVALID_HANDLE_VALUE)
        {
            WinError("Error creating output file: %s", lTmpOutput);
            if (!pFullName) DeleteFile( lTmpInput);
            DeleteFile( lTmpOutput);
            return false;
        }

        si.hStdInput = hStdInput;
        si.hStdOutput= hStdOutput;
        si.hStdError = hStdOutput;

        static PROCESS_INFORMATION pi;
        memset(&pi,0,sizeof(pi));

        if (Opt.debug) InfoMsg(true, "Command", "OK", "%s", lFullCmd);

        if (CreateProcess(NULL,lFullCmd,NULL,NULL,true,0,NULL,NULL,&si,&pi))
        {
            WaitForSingleObject(pi.hProcess,INFINITE);
            GetExitCodeProcess(pi.hProcess,&lExitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            if (lExitCode==0 || Param.ignoreReturnCode==1)
            {
                bProcessOK=true;
            }
        }
        else
        {
            WinError("Cannot create process!");
        }

        CloseHandle(hStdInput);
        CloseHandle(hStdOutput);
    }

    Info.RestoreScreen(hScreen);
    if (dTitleLen)  SetConsoleTitle(sConsoleTitleOld);

    if (bInput)
    {
        DeleteFile( lPHInput);
    }

    if (bOutput)
    {
        CopyFileEx( lPHOutput, lTmpOutput, NULL, NULL, FALSE, 0);
        DeleteFile( lPHOutput);
    }

    if (bProcessOK)
    {
        // Updates ei after line change
        ProcessOutput(lTmpInput,lTmpOutput, ei);
    }
    else
    {
        if (Glob.openedFrom == OPEN_EDITOR) UnselectBlock(bAutoSelect, ei);
        ShowError(lTmpOutput);
    }

    if (!Opt.debug)
    {
        if (*lTmpInput && !pFullName)  DeleteFile(lTmpInput);
        if (*lTmpOutput)               DeleteFile(lTmpOutput);
    }

    if (Glob.openedFrom == OPEN_EDITOR) UnselectBlock(bAutoSelect, ei);

    return bProcessOK;
}

void ProcessFiles(char * psRoot, char * pName)
{
    struct PanelInfo PInfo;
    struct PanelInfo PAnotherInfo;

    char FullName[NM+2];
    char lTmpInput[NM];

    WriteFileBuf * wfb = NULL;
    Param.Get( psRoot, pName);

    if (!Param.processInput)
    {
        RunCommand( psRoot, pName);
        Info.Control(INVALID_HANDLE_VALUE,FCTL_UPDATEPANEL,(void*)1);
        Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,NULL);
        return;
    }

    if (Param.processInputChoice == 2)
    {
        if (!FSF.MkTemp(lTmpInput, "BPF"))
        {
            WinError("Cannot create file for filename list!");
            return;
        }

        static SECURITY_ATTRIBUTES sa;

        memset(&sa, 0, sizeof(sa));
        sa.nLength              = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle       = true;

        wfb = new WriteFileBuf( lTmpInput);

        if (wfb->Error())
        {
            WinError("Error opening input file: %s", lTmpInput);
            return;
        }
    }

    int count = 0;
    Info.Control(INVALID_HANDLE_VALUE,FCTL_GETPANELINFO,&PInfo);

    for (int i=0;i<PInfo.SelectedItemsNumber;i++)
    {
        GetFullName(FullName,PInfo.CurDir,
                    PInfo.SelectedItems[i].FindData.cFileName);

        if (!FileExists( FullName, (Param.includeSubdir==1)))
        {
            continue;
        }

        if (Param.processInputChoice == 2)
        {
            count++;

            wfb->WriteLine(FullName);
        }
        else
        {
            if (!FileExists( FullName))
            {
                continue;
            }

            count++;

            if (!RunCommand( psRoot, pName, FullName))
            {
                break;
            }
        }

    }

    if (Param.processInputChoice == 2)
    {
        Info.Control(INVALID_HANDLE_VALUE,FCTL_GETANOTHERPANELINFO,&PAnotherInfo);

        for (int i=0;i<PAnotherInfo.SelectedItemsNumber;i++)
        {
            GetFullName(FullName,PAnotherInfo.CurDir,
                        PAnotherInfo.SelectedItems[i].FindData.cFileName);

            if (!(PAnotherInfo.SelectedItems[i].Flags&PPIF_SELECTED)) continue;
            if (!FileExists( FullName, true)) continue;

            count++;
            wfb->WriteLine(FullName);
        }
    }

    if (count==0)
    {
        ErrorMsg(GetMsg(MNoFiles));
    }

    if (Param.processInputChoice == 2)
    {
        delete wfb;
        bool deselect = false;

        if (count>0)
        {
            RunCommand( psRoot, pName, lTmpInput);
        }

        DeleteFile( lTmpInput);
    }

    if (count>0)
    {
        if (Param.removeAnySelect)
        {
            Info.Control(INVALID_HANDLE_VALUE,FCTL_UPDATEPANEL,NULL);
            if (Param.processInputChoice==2)
                Info.Control(INVALID_HANDLE_VALUE,FCTL_UPDATEANOTHERPANEL,NULL);
        }
        else
        {
            Info.Control(INVALID_HANDLE_VALUE,FCTL_UPDATEPANEL,(void*)1);
            if (Param.processInputChoice==2)
                Info.Control(INVALID_HANDLE_VALUE,FCTL_UPDATEANOTHERPANEL,(void*)1);
        }

        Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,NULL);
        if (Param.processInputChoice==2)
            Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWANOTHERPANEL,NULL);
    }
}

/******************************************************************************
 *** Menu functions ***********************************************************
 ******************************************************************************/

static void ConfigSubMenu( char * psRoot, char * pName)
{
    struct InitDialogItem InitItems[] =
    {
    /*00*/ { DI_DOUBLEBOX, 3,1,44,  6,        0,               0, (char*)MCaption },
    /*01*/ { DI_TEXT,      5,2,14,  0,        0,               0, (char*)MHotkey  },
    /*02*/ { DI_FIXEDIT,  15,2,15,  0,        0,               0, Param.hotkey    },
    /*03*/ { DI_TEXT,      5,3,14,  0,        0,               0, (char*)MDescr   },
    /*04*/ { DI_EDIT,     15,3,42,  0,        0,               0, Param.name      },
    /*05*/ { DI_TEXT,      5,4, 0,255,        0,                  NULL            },
    /*06*/ { DI_BUTTON,    0,5, 0,  0,        0, DIF_CENTERGROUP, (char*)MOK      },
    /*07*/ { DI_BUTTON,    0,5, 0,  0,        0, DIF_CENTERGROUP, (char*)MCancel  }
    };

    int dOk       = sizeof(InitItems)/sizeof(InitItems[0]) - 2;
    int dExitCode;

    do
    {
        dExitCode = ExecDialog(EXARR(InitItems),2,dOk, "Menu");

        if (dExitCode == dOk)
        {
            if (Param.hotkey[0] == 0x20)
            {
                ErrorMsg(GetMsg(MWrongHotkey));
                continue;
            }

            Param.Set();
            break;
        }

    } while (dExitCode == dOk);
}

static void ConfigExtMenu( char * psRoot, char * pName)
{
    struct InitDialogItem InitItems[] =
    {
    /*00*/ { DI_DOUBLEBOX, 3,1,44,  6,        0,               0, (char*)MCaption },
    /*01*/ { DI_TEXT,      5,2,14,  0,        0,               0, (char*)MExtensions },
    /*02*/ { DI_EDIT,     15,2,42,  0,        0,               0, Param.hotkey    },
    /*03*/ { DI_TEXT,      5,3,14,  0,        0,               0, (char*)MDescr   },
    /*04*/ { DI_EDIT,     15,3,42,  0,        0,               0, Param.name      },
    /*05*/ { DI_TEXT,      5,4, 0,255,        0,                  NULL            },
    /*06*/ { DI_BUTTON,    0,5, 0,  0,        0, DIF_CENTERGROUP, (char*)MOK      },
    /*07*/ { DI_BUTTON,    0,5, 0,  0,        0, DIF_CENTERGROUP, (char*)MCancel  }
    };

    int dOk       = sizeof(InitItems)/sizeof(InitItems[0]) - 2;
    int dExitCode;

    do
    {
        dExitCode = ExecDialog(EXARR(InitItems),2,dOk, "Menu");

        if (dExitCode == dOk)
        {
            if (Param.hotkey[0] == 0x20)
            {
                ErrorMsg(GetMsg(MWrongHotkey));
                continue;
            }

            FSF.LLowerBuf(Param.hotkey,strlen(Param.hotkey));
            Param.Set();
            break;
        }

    } while (dExitCode == dOk);
}

#define SET_CHOICE( group, choice, items, counter) \
    for (int i=0; i<group; i++) \
        if (items[ counter++].Selected) choice = i;

static void ConfigCommand(char * psRoot, char * pName)
{
    if (pName)
    {
        Param.Get(psRoot, pName);
    }
    else /* New item or menu? */
    {
        Param.Init(psRoot);

        struct InitDialogItem InitItems[] =
        {
        /*00*/ { DI_DOUBLEBOX, 3,1,52, 3,        0,               0, (char*)MCaption },
        /*01*/ { DI_BUTTON,    0,2,0,  0,        1, DIF_CENTERGROUP, (char*)MNewItem },
        /*02*/ { DI_BUTTON,    0,2,0,  0,        0, DIF_CENTERGROUP, (char*)MNewMenu },
        /*03*/ { DI_BUTTON,    0,2,0,  0,        0, DIF_CENTERGROUP, (char*)MNewExtension }
        };

        switch (ExecDialog(EXARR(InitItems),1,1, "Choose"))
        {
            case 1:  Param.isMenu = 0; break;
            case 2:  Param.isMenu = 1; break;
            case 3:  Param.isMenu = 2; break;
            default: return;
        }
    }

    switch (Param.isMenu)
    {
        case 1:  ConfigSubMenu(psRoot,pName); return;
        case 2:  ConfigExtMenu(psRoot,pName); return;
    }

    memcpy( &TempParam, &Param, sizeof(Param));

    char *HS = "BlockPro\\Command";

    struct InitDialogItem InitItems[] =
    {
    /* 0*/ { DI_DOUBLEBOX,   3, 1,72, 19,        0,               0, (char*)MCaption },
    /* 1*/ { DI_TEXT,        5, 2,12,  0,        0,               0, (char*)MHotkey  },
    /* 2*/ { DI_FIXEDIT,    13, 2,13,  0,        0,               0, TempParam.hotkey    },
    /* 3*/ { DI_TEXT,        5, 3,12,  0,        0,               0, (char*)MDescr   },
    /* 4*/ { DI_EDIT,       13, 3,45,  0,        0,               0, TempParam.name      },
    /* 5*/ { DI_TEXT,        5, 4,12,  0,        0,               0, (char*)MScript  },
    /* 6*/ { DI_EDIT,       13, 4,70,  0,(DWORD)HS,     DIF_HISTORY, TempParam.command   },
    /* 7*/ { DI_TEXT,        5, 5, 0,255,        0,                  NULL            },
    /* 8*/ { DI_CHECKBOX ,   5, 6,36,  0,        0,               0, (char *)MInputProcess},
    /* 9*/ { DI_RADIOBUTTON, 5, 7,36,  0,        0,       DIF_GROUP, (char *)MInputSelected},
    /*10*/ { DI_RADIOBUTTON, 5, 8,36,  0,        0,               0, (char *)MInputSelectedLines},
    /*11*/ { DI_RADIOBUTTON, 5, 9,23,  0,        0,               0, (char *)MInputFilenameList},
    /*12*/ { DI_CHECKBOX ,  24, 9,36,  0,        0,               0, (char *)MInputSubdir},
    /*13*/ { DI_CHECKBOX ,  37, 6,70,  0,        0,      DIF_3STATE, (char *)MInputSelection      },
    /*14*/ { DI_RADIOBUTTON,37, 7,70,  0,        0,       DIF_GROUP, (char *)MInputSelectionWord  },
    /*15*/ { DI_RADIOBUTTON,37, 8,70,  0,        0,               0, (char *)MInputSelectionLine  },
    /*16*/ { DI_RADIOBUTTON,37, 9,70,  0,        0,               0, (char *)MInputSelectionEntire},
    /*17*/ { DI_CHECKBOX,    5,11,36,  0,        0,      DIF_3STATE, (char *)MOutputProcess    },
    /*18*/ { DI_RADIOBUTTON, 5,12,36,  0,        0,       DIF_GROUP, (char *)MOutputToEditor   },
    /*19*/ { DI_RADIOBUTTON, 5,13,20,  0,        0,               0, (char *)MOutputToNewEditor},
    /*20*/ { DI_RADIOBUTTON, 5,14,36,  0,        0,               0, (char *)MOutputToNewViewer},
    /*21*/ { DI_RADIOBUTTON, 5,15,36,  0,        0,               0, (char *)MOutputToJumpMenu},
    /*22*/ { DI_BUTTON,     21,13,36,  0,        0,               0, (char *)MOutputToFixedName },
    /*23*/ { DI_CHECKBOX,   37,11,70,  0,        0,               0, (char *)MOutputToDialog},
    /*24*/ { DI_CHECKBOX,   37,12,70,  0,        0,               0, (char *)MOutputToClipboard},
    /*25*/ { DI_CHECKBOX,   37,13,70,  0,        0,               0, (char *)MKeepAutoSelect},
    /*26*/ { DI_CHECKBOX,   37,14,70,  0,        0,               0, (char *)MRemoveAnySelect},
    /*27*/ { DI_CHECKBOX,   37,15,70,  0,        0,               0, (char *)MIgnoreReturnCode},
    /*28*/ { DI_CHECKBOX,   37,16,70,  0,        0,               0, (char *)MAutoSave},
    /*29*/ { DI_TEXT,        5,17, 0,255,        0,                  NULL            },
    /*30*/ { DI_BUTTON,      0,18, 0,  0,        0, DIF_CENTERGROUP, (char*)MOK      },
    /*31*/ { DI_BUTTON,      0,18, 0,  0,        0, DIF_CENTERGROUP, (char*)MCancel  }
    };

    /*
    [x] Process input from:   [ ] Auto select:
    () Only selected text    () Pick up &word under cursor
    ( ) Lines with selection  ( ) Pick up whole current line
    ( ) Filename list         ( ) Process full file

    [x] Process output to:    [ ] Preview output
    () Current editor        [ ] Copy first line to Clipboard
    ( ) New editor [ Fixed]   [ ] Keep auto select
    ( ) New viewer            [ ] Remove any selection
    ( ) Line jump menu        [ ] Ignore return code
                              [ ] Autosave

    New design:

    [x] Process input from:   [ Only selected text ]
    [x] Process output to:    [ Current editor ]
    [ ] Auto select:          [ Pick up word under cursor ]

    [ ] Autosave              [ ] Preview output
    [ ] Keep auto select      [ ] Copy first line to Clipboard
    [ ] Remove any selection  [ ] Ignore return code

    */

    int size      = sizeof(InitItems)/sizeof(InitItems[0]);
    int tCheckBox = 8;
    int dFixed    = 22;
    int dOk       = size - 2;
    int dExitCode;

    do
    {
        tCheckBox = 8;

        InitItems[tCheckBox++].Selected=TempParam.processInput;
        InitItems[tCheckBox++].Selected=(TempParam.processInputChoice==0);
        InitItems[tCheckBox++].Selected=(TempParam.processInputChoice==1);
        InitItems[tCheckBox++].Selected=(TempParam.processInputChoice==2);
        InitItems[tCheckBox++].Selected=TempParam.includeSubdir;
        InitItems[tCheckBox++].Selected=TempParam.processSelection;
        InitItems[tCheckBox++].Selected=(TempParam.processSelectionChoice==0);
        InitItems[tCheckBox++].Selected=(TempParam.processSelectionChoice==1);
        InitItems[tCheckBox++].Selected=(TempParam.processSelectionChoice==2);
        InitItems[tCheckBox++].Selected=TempParam.processOutput;
        InitItems[tCheckBox++].Selected=(TempParam.processOutputChoice==0);
        InitItems[tCheckBox++].Selected=(TempParam.processOutputChoice==1);
        InitItems[tCheckBox++].Selected=(TempParam.processOutputChoice==2);
        InitItems[tCheckBox++].Selected=(TempParam.processOutputChoice==3);
        tCheckBox++;  // Skip fixedPath
        InitItems[tCheckBox++].Selected=TempParam.preview;
        InitItems[tCheckBox++].Selected=TempParam.copyToClipboard;
        InitItems[tCheckBox++].Selected=TempParam.keepAutoSelect;
        InitItems[tCheckBox++].Selected=TempParam.removeAnySelect;
        InitItems[tCheckBox++].Selected=TempParam.ignoreReturnCode;
        InitItems[tCheckBox++].Selected=TempParam.autoSave;

        struct FarDialogItem *Item = winNew(struct FarDialogItem, size);
        InitDialogItems(InitItems, Item, size);
        Item[2].Focus           = 1;
        Item[dOk].DefaultButton = 1;
        dExitCode = Info.Dialog(Info.ModuleNumber,-1,-1,Item[0].X2+4,Item[0].Y2+2,"Command",Item,size);

        if (dExitCode == dOk || dExitCode == dFixed)
        {
            SaveDialogItems(Item, InitItems, size);
        }
        winDel(Item);

        if (dExitCode == dOk || dExitCode == dFixed)
        {
            FSF.Trim(TempParam.hotkey);

            if (*TempParam.hotkey==0)
            {
                ErrorMsg(GetMsg(MWrongHotkey));
                continue;
            }

            FSF.Trim(TempParam.name);

            if (*TempParam.name==0)
            {
                ErrorMsg(GetMsg(MNoDesc));
                continue;
            }

            tCheckBox = 8;
            TempParam.processInput     = InitItems[tCheckBox++].Selected;
            SET_CHOICE( 3, TempParam.processInputChoice,
                                         InitItems, tCheckBox);
            TempParam.includeSubdir    = InitItems[tCheckBox++].Selected;
            TempParam.processSelection = InitItems[tCheckBox++].Selected;
            SET_CHOICE( 3, TempParam.processSelectionChoice,
                                         InitItems, tCheckBox);
            TempParam.processOutput    = InitItems[tCheckBox++].Selected;
            SET_CHOICE( 4, TempParam.processOutputChoice,
                                         InitItems, tCheckBox);
            tCheckBox++; // Skip fixedPath
            TempParam.preview          = InitItems[tCheckBox++].Selected;
            TempParam.copyToClipboard  = InitItems[tCheckBox++].Selected;
            TempParam.keepAutoSelect   = InitItems[tCheckBox++].Selected;
            TempParam.removeAnySelect  = InitItems[tCheckBox++].Selected;
            TempParam.ignoreReturnCode = InitItems[tCheckBox++].Selected;
            TempParam.autoSave         = InitItems[tCheckBox++].Selected;

            if (dExitCode == dFixed)
            {
                Info.InputBox(GetMsg(MEditor),
                            GetMsg(MEditorSaveTo),
                            "NewEdit",
                            TempParam.fixedPath,
                            TempParam.fixedPath,
                            NM,
                            NULL,
                            FIB_NOUSELASTHISTORY|FIB_ENABLEEMPTY);
                continue;
            }

            if (dExitCode == dOk)
            {
                memcpy( &Param, &TempParam, sizeof(Param));
                Param.Set();
                break;
            }
        }
    } while (dExitCode == dOk || dExitCode == dFixed);
}

#undef SET_CHOICE

static bool RemoveMask(char * psStr)
{
    if (*psStr == 0)
    {
        return false;
    }

    char lTmp[128];
    if (psStr[0] == '&')
    {
        strcpy( psStr, psStr+3);
    }
    else
    {
        strcpy( psStr, psStr+2);
    }

    char * ptr = strstr(psStr, MENU_TAIL);
    if (ptr)
    {
        *ptr=0;
        FSF.RTrim( psStr);
        return true;
    }

    return false;
}

static bool AddMask(HKEY, char* psRoot, char *key, FarMenuItem *menu, int *n, void *data)
{
    Param.GetHotKey(psRoot, key);
    if (Param.isMenu == 2)
    {
        FSF.sprintf(menu->Text, "%c %s", EXT_MENU, key);
    }
    else
    {
        FSF.sprintf(menu->Text, "%s%.1s %s", (Param.isDisabled?"":"&"), Param.hotkey, key);
    }
    if (Param.isMenu) strcat( menu->Text, MENU_TAIL);
    return (data != NULL && *n == -1) ? (strcmp(key, (char*)data)) == 0 : false;
}

static bool ExtensionMatches(const char * psRoot, char * key, char * ext)
{
    Param.GetHotKey(psRoot, key);
    FSF.LLowerBuf(ext,strlen(ext));
    return (strstr(Param.hotkey,ext)!=NULL);
}

int FarMenuItemText(const void * a, const void * b)
{
    FarMenuItem* k1 = (FarMenuItem*)a;
    FarMenuItem* k2 = (FarMenuItem*)b;

    char * m1 = strstr(k1->Text,MENU_TAIL);
    char * m2 = strstr(k2->Text,MENU_TAIL);

    if ( m1 && !m2) return -1;
    if (!m1 &&  m2) return  1;

    if ( m1 && m2)
    {
        if ( *k1->Text == EXT_MENU && *k2->Text != EXT_MENU) return -1;
        if ( *k1->Text != EXT_MENU && *k2->Text == EXT_MENU) return  1;
    }

    return strcmp( k1->Text+ (k1->Text[0]=='&'?3:2),
                   k2->Text+ (k2->Text[0]=='&'?3:2));
}

static void OpenJumpMenu()
{
    if (!FileExists(Opt.lastOutput))
    {
        ErrorMsg( GetMsg(MNoLastOutput));
        return;
    }

    int breakKeys[]   = { VK_F3,
                          VK_F5,
                          VK_RETURN, 0 };

    FarMenuItem *menu = NULL;
    bool lbAgain      = false;
    int retCode, n    = -1;

    int count = MenuFromFile(Opt.lastOutput, &menu, &n, Opt.lastLine);

    if (count==0)
    {
        return;
    }

    do
    {
        lbAgain = true;
        int oldLastLine = Opt.lastLine;
        Opt.lastLine = Info.Menu(Info.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE,
                           Opt.lastFile, GetMsg(MJumpMenuActions),
                           "LineJump", breakKeys, &retCode, menu, count);

        char * lastLine = NULL;

        if (Opt.lastLine>-1)
        {
            lastLine = winNew( char, strlen(menu[Opt.lastLine].Text)+1);
            strcpy( lastLine, menu[Opt.lastLine].Text);
            SetRegKey(HKEY_CURRENT_USER,"",REG_LastLine,Opt.lastLine);
        }

        switch ( retCode)
        {
            case 0:
            case 1:
                {
                    char copyBuff[MAX_VIEW_LINE+1];
                    char * fullMessage = winNew(char,strlen(lastLine)+strlen(lastLine)/MAX_VIEW_LINE*2+20);
                    strcpy(fullMessage,GetMsg(MViewLine));
                    strcat(fullMessage,"\n");
                    int l = strlen(fullMessage);

                    for (unsigned int i=0;i<strlen(lastLine);i+=60)
                    {
                        l += min((unsigned int)MAX_VIEW_LINE,strlen(lastLine+i));
                        strncat(fullMessage,lastLine+i,MAX_VIEW_LINE);
                        fullMessage[l] = 0;
                        strcat(fullMessage,"\n");
                        l += 1;
                    }

                    if (retCode == 0)
                    {
                        Info.Message(Info.ModuleNumber,FMSG_MB_OK|FMSG_LEFTALIGN|FMSG_ALLINONE,
                                     NULL,(const char **)fullMessage,0,0);
                    }
                    else
                    {
                        lbAgain = false;
                        FSF.CopyToClipboard(lastLine);
                    }

                    winDel( fullMessage);
                    menu[oldLastLine].Selected  = false;
                    menu[Opt.lastLine].Selected = true;
                }
                break;

            case 2:
                // Goto file/line
                {
                    bool newEditor = true;

                    int offset = 0;

                    if (lastLine[0]>='A' &&
                        lastLine[0]<='Z' &&
                        lastLine[1]==':')
                    {
                        offset += 2;
                    }

                    while (lastLine[offset] !=0 && lastLine[offset] != ' ' && lastLine[offset] != ':') offset++;

                    int line = atoi( lastLine+offset+1);

                    char foundName[NM];
                    strncpy(foundName,lastLine,NM);
                    foundName[offset]=0;

                    while (offset>0 && strchr(": ",foundName[offset-1]))
                    {
                        offset--;
                        foundName[offset-1]=0;
                    }

                    if (FileExists(foundName))
                    {
                        strcpy( Opt.lastFile, foundName);
                    }
                    else
                    {
                        // If file not found than we have line number
                        line = atoi( foundName);
                    }

                    if (Glob.openedFrom == OPEN_EDITOR)
                    {
                        struct EditorInfo ei;
                        Info.EditorControl(ECTL_GETINFO, &ei);

                        const char * a = strrchr(Opt.lastFile,'\\');
                        const char * b = strrchr(ei.FileName,'\\');

                        if (!a) a=Opt.lastFile;
                        if (!b) b=ei.FileName;

                        // Compare only fileName
                        if (strcmpi(a,b)==0)
                        {
                            struct EditorSetPosition esp;
                            esp.TopScreenLine  = line-1; //ei.TopScreenLine;
                            esp.CurTabPos      = esp.Overtype = -1;
                            esp.CurLine        = line-1;
                            esp.CurPos         = 0;
                            esp.LeftPos        = ei.LeftPos;
                            Info.EditorControl(ECTL_SETPOSITION,  &esp);
                            newEditor = false;
                        }
                    }

                    if (newEditor)
                    {
                        Info.Editor(Opt.lastFile,NULL,0,0,-1,-1,EF_NONMODAL,line,1);
                    }
                }

            default:
                lbAgain = false;
                break;
        }

        if (lastLine) winDel(lastLine);

    } while (lbAgain);

    winDel(menu);
}

static void ShowMenu(char * psRoot)
{
    char lFullKeyName[512];
    char lLast       [128];
    bool isSubmenu    = false;
    bool isExtmenu    = false;
    bool lbAgain      = false;
    bool lbLast       = true;
    bool lbIgnoreExt  = false;

    int retCode, n    = -1;
    int breakKeys[]   = { VK_BACK,
                          VK_INSERT,
                          VK_DELETE,
                          VK_F3,
                          VK_F4,
                          VK_F9,
                          VK_SPACE,
                          VK_TAB,
                          VK_RETURN, 0 };

    int breakKeysCount= sizeof(breakKeys)/sizeof(int) - 1;
    FarMenuItem *menu = NULL;

    do
    {
        lbAgain = true;
        GetRegKey(HKEY_CURRENT_USER, psRoot, REG_Last, lLast, "", 255);

        int count  = MenuFromRegKey(HKEY_CURRENT_USER, psRoot,AddMask, &menu, &n, lLast);
        int maxLen = 0;

        qsort(menu,count,sizeof(FarMenuItem),FarMenuItemText);

        if (n >= 0 && n < count)
        {
            menu[n].Selected = true;
        }

        int nonExtension = -1;
        int selected     = -1;

        // Count the length of the menu items and check selected item
        for (int i=0; i<count; i++)
        {
            int l = strlen(menu[i].Text);
            if (*menu[i].Text=='&') l--;
            if (l>maxLen) maxLen = l;
            if (*menu[i].Text!=EXT_MENU && nonExtension < 0)
            {
                nonExtension = i;
            }
            if (menu[i].Selected)
            {
                selected = i;
            }
        }

        if (selected<0 && nonExtension>=0)
        {
            menu[nonExtension].Selected = true;
        }

        // Align the menu signs
        for (int i=0; i<count; i++)
        {
            char lLabel[128];
            char * ptr = strstr(menu[i].Text, MENU_TAIL);
            if (ptr)
            {
                *ptr=0;
                FSF.sprintf(lLabel,"%-*.*s%s",
                            maxLen-(menu[i].Text[0]=='&'?0:1),
                            maxLen-(menu[i].Text[0]=='&'?0:1),
                            menu[i].Text, MENU_TAIL);
                strcpy( menu[i].Text, lLabel);
            }
        }

        bool lbExtensionFound = false;

        // Extension submenu found
        if (*menu[0].Text == EXT_MENU && !lbIgnoreExt)
        {
            const char * fileName = NULL;

            switch (Glob.openedFrom)
            {
                case OPEN_EDITOR:
                    {
                        struct EditorInfo ei;
                        Info.EditorControl(ECTL_GETINFO, &ei);
                        fileName = ei.FileName;
                    }
                    break;

                default:
                    {
                        struct PanelInfo PInfo;
                        Info.Control(INVALID_HANDLE_VALUE,FCTL_GETPANELINFO,&PInfo);
                        if (PInfo.SelectedItemsNumber>0)
                        {
                            fileName = PInfo.SelectedItems[0].FindData.cFileName;
                        }
                    }
                    break;
            }

            if (fileName != NULL)
            {
                char * extension = const_cast<char*>(strrchr(fileName, '.'));

                if (extension != NULL)
                {
                    extension++;
                    char menuText[128];

                    for (int i = 0; i < count && *menu[i].Text == EXT_MENU; i++)
                    {
                        strcpy(menuText,menu[i].Text);
                        RemoveMask( menuText);

                        if (ExtensionMatches(psRoot,menuText,extension))
                        {
                            n       = i;
                            retCode = breakKeysCount - 1;
                            lbExtensionFound = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!lbExtensionFound)
        {
            struct timeb t_old, t_new;
            ftime(&t_old);

            n = Info.Menu(Info.ModuleNumber, -1, -1, 0, FMENU_WRAPMODE,
                          GetMsg(MCaption), GetMsg(MActions),
                          "MainMenu", breakKeys, &retCode, menu, count);

            // Temporary solution until SPACE and TAB are called properly, write in DOC's then
            if (Opt.rememberDelay>0)
            {
                ftime(&t_new);

                double diff = (t_new.time    - t_old.time)+
                              (t_new.millitm - t_old.millitm)/1000.0;
                lbLast = (diff > (Opt.rememberDelay/1000));
            }
            else
            {
                lbLast = false;
            }
        }

        if (retCode == 0)
        {
            lbIgnoreExt = true;
            n           = -1; // Backspace pressed
        }
        else
        {
            lbIgnoreExt = false;
        }

        if (retCode == -1 && n>-1 && count>0) retCode =  breakKeysCount - 1; // Highlight is like enter

        bool isDisabled = (n > -1 && *menu[n].Text != '&' && *menu[n].Text != EXT_MENU);

        // We have insert or valid item
        if ( retCode <= 1 || (n > -1 && *menu[n].Text != 0))
        {
            if (n != -1)
            {
                isExtmenu = (*menu[n].Text == EXT_MENU);
                isSubmenu = RemoveMask(menu[n].Text);
            }
            else
            {
                isExtmenu = false;
                isSubmenu = false;
            }

            switch ( retCode )
            {
                case -1: // Escape
                    lbAgain = false;
                    break;

                case  0: // Backspace
                    {
                        char * ptr = strrchr(psRoot, '\\');
                        if (ptr)
                        {
                            *ptr = 0;
                            n = -1;
                        }
                        else
                        {
                            lbAgain = false;
                        }
                    }
                    break;

                case 1: // Insert
                    ConfigCommand(psRoot,NULL);
                    lbIgnoreExt = true;
                    break;

                case 2: // Delete
                    if (isDisabled)
                    {
                        ErrorMsg(GetMsg(MDisabledItem));
                        break;
                    }
                    if (Param.Delete(psRoot, menu[n].Text))
                    {
                        ErrorMsg(GetMsg(MItemsExist));
                    }
                    lbIgnoreExt = true;
                    break;

                case 3: // View last menu F3
                    OpenJumpMenu();
                    lbAgain = false;
                    break;

                case 4: // Edit F4
                    ConfigCommand(psRoot,menu[n].Text);
                    lbIgnoreExt = true;
                    break;

                case 5: // Disable/enable F9
                    Param.Toggle(psRoot,menu[n].Text);
                    lbIgnoreExt = true;
                    break;

                case 6:
                    lbLast = false;
                    break;

                case 7:
                    lbLast = true;
                    break;

                case 8: // Execute menu or item
                    if (isDisabled)
                    {
                        ErrorMsg(GetMsg(MDisabledItem));
                        break;
                    }

                    // Do not save last value if it is extension menu
                    if (lbLast && !isExtmenu)
                    {
                        SetRegKey(HKEY_CURRENT_USER, psRoot, REG_Last, menu[n].Text);
                    }

                    if (isSubmenu)
                    {
                        if (strlen( psRoot) + strlen(menu[n].Text) + 2 < sizeof(Glob.fullKeyName))
                        {
                            strcat( psRoot, "\\");
                            strcat( psRoot, menu[n].Text);
                            n = -1;
                        }
                        else
                        {
                            ErrorMsg( GetMsg(MTooDeep));
                        }
                    }
                    else
                    {
                        RunTimeParam.ClearCache();

                        switch (Glob.openedFrom)
                        {
                            case OPEN_PLUGINSMENU:
                                ProcessFiles(psRoot,menu[n].Text);
                                break;

                            case OPEN_EDITOR:
                                RunCommand(psRoot,menu[n].Text);
                                break;

                            default:
                                ErrorMsg( "How did you get here?");
                        }
                        lbAgain = false;
                    }
                    break;

                default:
                    ErrorMsg("Unknown return code %d", retCode);
                    break;
            }
        }

        if (menu)
        {
            winDel(menu);
        }
        menu = NULL;

    } while (lbAgain);
}

HANDLE WINAPI _export OpenPlugin(int piOpenFrom, int)
{
    strcpy( Glob.fullKeyName, REG_Main);
    Glob.openedFrom = piOpenFrom;
    ShowMenu(Glob.fullKeyName);
    return INVALID_HANDLE_VALUE;
}

int WINAPI _export Configure(int ItemNumber)
{
    char lsLines[4];
    char lsDelay[4];

    struct InitDialogItem InitItems[]=
    {
    /*00*/{DI_DOUBLEBOX,  3,1,48,  9,0,0, (char*)MCaption},
    /*01*/{DI_CHECKBOX,   5,2, 0,  0,0,0, (char*)MConfigAddToPluginMenu},
    /*02*/{DI_CHECKBOX,   5,3, 0,  0,0,0, (char*)MConfigCreateBackups},
    /*03*/{DI_CHECKBOX,   5,4, 0,  0,0,0, (char*)MNonPersistentBlocks},
    /*04*/{DI_FIXEDIT,    5,5, 6,  0,0,0, lsLines  },
    /*05*/{DI_TEXT,       9,5,43,  0,0,0, (char*)MConfigLinesToDisplay },
    /*06*/{DI_FIXEDIT,    5,6, 7,  0,0,0, lsDelay  },
    /*07*/{DI_TEXT,       9,6,43,  0,0,0, (char*)MConfigRememberDelay },
    /*08*/{DI_TEXT,       5,7, 0,255,0,NULL},
    /*09*/{DI_BUTTON,     0,8, 0,  0,0,DIF_CENTERGROUP,(char *)MOK},
    /*10*/{DI_BUTTON,     0,8, 0,  0,0,DIF_CENTERGROUP,(char *)MCancel}
    };

    InitItems[1].Selected=Opt.addToPluginsMenu;
    InitItems[2].Selected=Opt.backupFiles;
    InitItems[3].Selected=Opt.nonPersistentBlock;

    sprintf( lsLines, "%02d", Opt.linesToDisplay);
    sprintf( lsDelay, "%03d", Opt.rememberDelay);

    int dOk       = sizeof(InitItems)/sizeof(InitItems[0]) - 2;
    int dExitCode = ExecDialog(EXARR(InitItems),5,dOk, "Config");

    if (dExitCode!=dOk)
    {
      return(false);
    }

    Opt.addToPluginsMenu   = InitItems[1].Selected;
    Opt.backupFiles        = InitItems[2].Selected;
    Opt.nonPersistentBlock = InitItems[3].Selected;
    Opt.linesToDisplay     = atoi( lsLines);
    Opt.rememberDelay      = atoi( lsDelay);

    SetRegKey(HKEY_CURRENT_USER,"",REG_AddToPanels,  Opt.addToPluginsMenu);
    SetRegKey(HKEY_CURRENT_USER,"",REG_BackupFiles,  Opt.backupFiles);
    SetRegKey(HKEY_CURRENT_USER,"",REG_Lines,        Opt.linesToDisplay);
    SetRegKey(HKEY_CURRENT_USER,"",REG_NonPersistent,Opt.nonPersistentBlock);
    SetRegKey(HKEY_CURRENT_USER,"",REG_RememberDelay,Opt.rememberDelay);

    return(false);
}

void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *Info)
{
    ::Info     = *Info;
    ::FSF      = *Info->FSF;
    ::Info.FSF = &::FSF;

    FSF.sprintf(PluginRootKey,"%s\\BlockProcessor",Info->RootKey);
    WordDivLen = ::Info.AdvControl(::Info.ModuleNumber, ACTL_GETSYSWORDDIV, WordDiv);
    WordDiv[WordDivLen] = 0;
    strcat(WordDiv," \t");

    Opt.addToPluginsMenu   = GetRegKey(HKEY_CURRENT_USER,"",REG_AddToPanels,1);
    Opt.backupFiles        = GetRegKey(HKEY_CURRENT_USER,"",REG_BackupFiles,1);
    Opt.nonPersistentBlock = GetRegKey(HKEY_CURRENT_USER,"",REG_NonPersistent,1);
    Opt.rememberDelay      = GetRegKey(HKEY_CURRENT_USER,"",REG_RememberDelay,5);
    Opt.linesToDisplay     = GetRegKey(HKEY_CURRENT_USER,"",REG_Lines,20);
    Opt.debug              = GetRegKey(HKEY_CURRENT_USER,"",REG_Debug,0);

    // Not published
    Opt.lastLine           = GetRegKey(HKEY_CURRENT_USER,"",REG_LastLine,0);
    GetRegKey(HKEY_CURRENT_USER,"",REG_LastFile,Opt.lastFile, "", NM-1);

    char * bpHome = getenv( "BPHOME");

    FSF.ExpandEnvironmentStr( "%TEMP%\\BPLastOutput.tmp", Opt.lastOutput, NM-1);

    if (bpHome)
    {
        strcpy( Glob.pluginPath, bpHome);
    }
    else
    {
        strcpy( Glob.pluginPath, ::Info.ModuleName);
        char * ptr;
        if ((ptr = strrchr( Glob.pluginPath, '\\'))) *ptr = 0;
    }
}

void WINAPI _export GetPluginInfo(struct PluginInfo *Info)
{
    static char *PluginMenuStrings[1];
    static char *PluginCfgStrings[1];

    Info->StructSize = sizeof(*Info);

    Info->Flags = PF_EDITOR;

    if (!Opt.addToPluginsMenu)
    {
      Info->Flags |= PF_DISABLEPANELS;
    }

    PluginMenuStrings[0]            = const_cast<char*>(GetMsg(MPluginCpt));
    PluginCfgStrings[0]             = const_cast<char*>(GetMsg(MCaption));

    Info->DiskMenuStringsNumber     = 0;
    Info->PluginMenuStringsNumber   = 1;
    Info->PluginMenuStrings         = PluginMenuStrings;
    Info->PluginConfigStringsNumber = 1;
    Info->PluginConfigStrings       = PluginCfgStrings;
}
