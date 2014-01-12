DST   = $(FARHOME)\Plugins\BlockPro
SRC   = $(FARHOME)\Dev\Plugins\BlockPro
DSTBP = $(DST)\BPPlugins
SRCBP = $(SRC)\BPPlugins

ALL: $(DST)\BlockPro.dll $(DST)\BlockPro.hlf $(DST)\BlockPro.lng $(DSTBP)\CtxHelp.bpd $(DSTBP)\JavaHelp.bpd

$(DST)\BlockPro.dll: $(SRC)\BlockPro.dll
    if exist %TEMP%\BlockPro.dll del  %TEMP%\BlockPro.dll
    if exist $(DST)\BlockPro.dll move $(DST)\BlockPro.dll %TEMP%
    copy $(SRC)\BlockPro.dll $(DST)

$(DST)\BlockPro.hlf: $(SRC)\BlockPro.hlf
    copy $(SRC)\BlockPro.hlf $(DST)

$(DST)\BlockPro.lng: $(SRC)\BlockPro.lng
    copy $(SRC)\BlockPro.lng $(DST)

$(DSTBP)\CtxHelp.bpd:  $(SRCBP)\CtxHelp.bpd
    copy $(SRCBP)\CtxHelp.bpd    $(DSTBP)

$(DSTBP)\JavaHelp.bpd: $(SRCBP)\JavaHelp.bpd
    copy $(SRCBP)\JavaHelp.bpd   $(DSTBP)
