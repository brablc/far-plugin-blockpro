#ifndef __BLOCKPRO_H
#define __BLOCKPRO_H

extern "C"
{
  void   WINAPI _export SetStartupInfo(const struct PluginStartupInfo *Info);
  HANDLE WINAPI _export OpenPlugin(int OpenFrom,int Item);
  void   WINAPI _export GetPluginInfo(struct PluginInfo *Info);
  int    WINAPI _export Configure(int ItemNumber);
};

#endif
