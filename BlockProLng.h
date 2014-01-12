// .Language=English,English
//
enum
{
  MCaption,                  // "Block Processor"
  MPluginCpt,                // "Block Processor "
  MActions,                  // "F1,Enter,Ins,Del"
  MJumpMenuActions,          // "Enter-Goto,F3-View,F5-ClpBrd"
  MRunning,                  // "External command is running ..."
  MSystemError,              // "System error"
  MEditor,                   // "Editor"
  MEditorSaveTo,             // "File to save output:"
  MViewLine,                 // "View line"
//
//; Errors
  MRunError,                 // "Error executing the command!"
  MRunExit,                  // "Exit code: %X!"
  MNoText,                   // "No text selected!"
  MTooDeep,                  // "Too deep nesting of menus!"
  MItemsExist,               // "Cannot delete menu with items!"
  MOverWrite,                // "Overwrite existing file?"
  MNoFiles,                  // "No files have been selected!"
  MWrongHotkey,              // "Hotkey cannot be space or empty!"
  MNoDesc,                   // "Description has not been entered!"
  MDisabledItem,             // "This item is disabled in this mode!"
  MNoLastOutput,             // "Cannot find jump line menu source file!"
//
//; Buttons
  MOK,                       // "OK"
  MCancel,                   // "Cancel"
//
//; Script Configuration
  MNewItem,                  // "&Item"
  MNewMenu,                  // "&Menu"
  MNewExtension,             // "&Extension menu"

  MHotkey,                   // "&Hotkey"
  MExtensions,               // "&Extensions"
  MDescr,                    // "&Label"
  MScript,                   // "&Command"

  MInputProcess,             // "Process &input from:"
  MInputSelected,            // "Only selected &text"
  MInputSelectedLines,       // "Lines with &selection"
  MInputFilenameList,        // "&Filename list"
  MInputSubdir,              // "Subdir"

  MInputSelection,           // "&Auto select:"
  MInputSelectionWord,       // "&Word under cursor"
  MInputSelectionLine,       // "C&urrent line"
  MInputSelectionEntire,     // "Enti&re file"

  MOutputProcess,            // "Process &output to:"
  MOutputToEditor,           // "Current &editor/file"
  MOutputToNewEditor,        // "&New editor"
  MOutputToNewViewer,        // "New &viewer"
  MOutputToJumpMenu,         // "Line &jump menu"
  MOutputToPrompt,           // "Prompt for output"
  MOutputToFixedName,        // "Fixed path"

  MOutputToDialog,           // "&Preview output"
  MOutputToClipboard,        // "Copy first line to clip&board"
  MKeepAutoSelect,           // "&Keep auto selection"
  MRemoveAnySelect,          // "Re&move any selection"
  MIgnoreReturnCode,         // "I&gnore return code"
  MAutoSave,                 // "Auto save current e&ditor"
//
//; Plugin Configuration
  MConfigAddToPluginMenu,    // "&Enable entire file processing"
  MConfigCreateBackups,      // "Create &backup of the file processed"
  MConfigLinesToDisplay,     // "&Number of lines for preview"
  MConfigRememberDelay,      // "&Remember choice after delay in milisec"
  MNonPersistentBlocks       // "Non &persistent blocks for autoselect"
};
