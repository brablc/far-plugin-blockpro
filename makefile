# Intended for use with free compiler from Borland
# http://www.borland.com/bcppbuilder/freecompiler/

MYLIB     = ..\..\Lib
MYINCLUDE = ..\..\Include

# ---------------------------------------------------------------------------
BCB         = $(MAKEDIR)\..
DCC32       = dcc32
LINKER      = ilink32
BCC32       = bcc32
LIBPATH     = $(BCB)\lib;$(MYLIB)
PATHCPP     = .;$(MYLIB)
USERDEFINES = _DEBUG
SYSDEFINES  = NO_STRICT;_NO_VCL;_RTLDLL
INCLUDEPATH = $(BCB)\include;$(MYINCLUDE)
WARNINGS    = -w-par
CFLAG1      = -w-rvl -w-pia -w-aus -d -x- -c -O2 -w3 -RT- -N- -a2
LFLAGS      = -Tpd -Gn

# ---------------------------------------------------------------------------
COMMON_OBJ   = $(MYLIB)\FarIntf.obj $(MYLIB)\filebuf.obj

BLOCKPRO_PRJ = BlockPro.dll
BLOCKPRO_OBJ = BlockPro.obj $(COMMON_OBJ) $(MYLIB)\rtparams.obj $(MYLIB)\scanreg.obj $(MYLIB)\scanfile.obj $(MYLIB)\renreg.obj $(MYLIB)\oemcoder.obj

CTXHELP_PRJ  = BPPlugins\CtxHelp.bpd
CTXHELP_OBJ  = CtxHelp.obj $(COMMON_OBJ)

JAVAHELP_PRJ = BPPlugins\JavaHelp.bpd
JAVAHELP_OBJ = JavaHelp.obj $(COMMON_OBJ)

.autodepend

ALL: $(BLOCKPRO_PRJ) BlockPro.lng $(CTXHELP_PRJ) $(JAVAHELP_PRJ)

$(BLOCKPRO_PRJ): $(BLOCKPRO_OBJ)
    $(BCB)\BIN\$(LINKER) @&&!
    $(LFLAGS) -L$(LIBPATH) +
    c0d32.obj $(BLOCKPRO_OBJ), +
    $(BLOCKPRO_PRJ),, +
    import32.lib cw32.lib htmlhelp.lib, +
    $(DEFFILE),
!

$(CTXHELP_PRJ): $(CTXHELP_OBJ)
    $(BCB)\BIN\$(LINKER) @&&!
    $(LFLAGS) -L$(LIBPATH) +
    c0d32.obj $(CTXHELP_OBJ), +
    $(CTXHELP_PRJ),, +
    import32.lib cw32.lib htmlhelp.lib, +
    $(DEFFILE),
!

$(JAVAHELP_PRJ): $(JAVAHELP_OBJ)
    $(BCB)\BIN\$(LINKER) @&&!
    $(LFLAGS) -L$(LIBPATH) +
    c0d32.obj $(JAVAHELP_OBJ), +
    $(JAVAHELP_PRJ),, +
    import32.lib cw32.lib htmlhelp.lib, +
    $(DEFFILE),
!

BlockPro.lng: BlockProLng.h
    perl makelng.pl BlockPro


# ---------------------------------------------------------------------------
.PATH.CPP = $(PATHCPP)
.PATH.C   = $(PATHCPP)

.cpp.obj:
    $(BCB)\BIN\$(BCC32) $(CFLAG1) $(WARNINGS) -I$(INCLUDEPATH) -D$(USERDEFINES);$(SYSDEFINES) -n$(@D) {$< }

.c.obj:
    $(BCB)\BIN\$(BCC32) $(CFLAG1) $(WARNINGS) -I$(INCLUDEPATH) -D$(USERDEFINES);$(SYSDEFINES) -n$(@D) {$< }
# ---------------------------------------------------------------------------
