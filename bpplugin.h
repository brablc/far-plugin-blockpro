/*
 * Copyright (c) Ondrej Brablc, 2002 <far@brablc.com>
 * You can use, modify, distribute this source code  or
 * any other parts of BlockPro plugin only according to
 * BlockPro  License  (see  Doc\License.txt  for   more
 * information).
 */

#ifndef __BPPLUGIN__HPP
#define __BPPLUGIN__HPP
#include <plugin.hpp>


#define BPPLUGINS_PREFIX "bpplugins:"
#define BPPLUGINS_DIR    "BPPlugins"


struct BlockProData
{
    char * inputFile;
    char * outputFile;
    char * commands;
};


typedef bool (*BlockProFunction)  (struct PluginStartupInfo *PSInfo,
                                   struct BlockProData *BPData);

#endif
