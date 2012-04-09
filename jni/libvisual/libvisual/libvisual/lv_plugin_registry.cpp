#define _SVID_SOURCE

#include "config.h"
#include "lv_plugin_registry.h"

#include "lv_common.h"
#include "lv_actor.h"
#include "lv_input.h"
#include "lv_morph.h"
#include "lv_transform.h"
#include "lv_util.hpp"
#include "gettext.h"

#include <vector>
#include <map>
#include <iterator>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <cstdlib>

#include <dirent.h>

#if defined(VISUAL_OS_WIN32)
#include <windows.h>
#endif

namespace LV {

  namespace {

    typedef std::map<PluginType, PluginList> PluginListMap;

    struct PluginHasType
        : public std::unary_function<bool, VisPluginRef const*>
    {
        PluginType type;

        PluginHasType (PluginType const& type_)
            : type (type_)
        {}

        bool operator() (VisPluginRef const* ref)
        {
            return (type == ref->info->type);
        }
    };

    struct PluginHasName
        : public std::unary_function<bool, VisPluginRef const*>
    {
        std::string name;

        PluginHasName (std::string const& name_)
            : name (name_)
        {}

        bool operator() (VisPluginRef const* ref)
        {
            return (name == ref->info->plugname);
        }
    };

  }

  class PluginRegistry::Impl
  {
  public:

      std::vector<std::string> plugin_paths;

      PluginListMap plugin_list_map;

      void get_plugins_from_dir (PluginList& list, std::string const& dir);
  };

  PluginRegistry::PluginRegistry ()
      : m_impl (new Impl)
  {
      visual_log (VISUAL_LOG_DEBUG, "Initializing plugin registry");

      // Add the standard plugin paths
      add_path (VISUAL_PLUGIN_PATH "/actor");
      add_path (VISUAL_PLUGIN_PATH "/input");
      add_path (VISUAL_PLUGIN_PATH "/morph");
      add_path (VISUAL_PLUGIN_PATH "/transform");

#if !defined(VISUAL_OS_WIN32) || defined(VISUAL_WITH_CYGWIN)
      // Add homedirectory plugin paths
      char const* home_env = std::getenv ("HOME");

      if (home_env) {
          std::string home_dir (home_env);

          add_path (home_dir + "/.libvisual/actor");
          add_path (home_dir + "/.libvisual/input");
          add_path (home_dir + "/.libvisual/morph");
          add_path (home_dir + "/.libvisual/transform");
      }
#endif
  }

  PluginRegistry::~PluginRegistry ()
  {
      // empty;
  }

  void PluginRegistry::add_path (std::string const& path)
  {
      visual_log (VISUAL_LOG_INFO, "Adding to plugin search path: %s", path.c_str());

      m_impl->plugin_paths.push_back (path);

      PluginList plugins;
      m_impl->get_plugins_from_dir (plugins, path);

      for (PluginList::iterator plugin = plugins.begin (), plugin_end = plugins.end ();
           plugin != plugin_end;
           ++plugin)
      {
          PluginList& list = m_impl->plugin_list_map[(*plugin)->info->type];
          list.push_back (*plugin);
      }
  }

  VisPluginRef* PluginRegistry::find_plugin (PluginType type, std::string const& name)
  {
      PluginList const& list = get_plugins_by_type (type);

      PluginList::const_iterator iter =
          std::find_if (list.begin (),
                        list.end (),
                        PluginHasName (name));

      return iter != list.end () ? *iter : 0;
  }

  bool PluginRegistry::has_plugin (PluginType type, std::string const& name)
  {
      return find_plugin (type, name) != 0;
  }

  PluginList const& PluginRegistry::get_plugins_by_type (PluginType type) const
  {
      static PluginList empty;

      PluginListMap::const_iterator match = m_impl->plugin_list_map.find (type);
      if (match == m_impl->plugin_list_map.end ())
          return empty;

      return match->second;
  }

  void PluginRegistry::Impl::get_plugins_from_dir (PluginList& list, std::string const& dir)
  {
      list.clear ();

#if defined(VISUAL_OS_WIN32)
      std::string pattern = dir + "/*";

      WIN32_FIND_DATA file_data;
      HANDLE hList = FindFirstFile (pattern.c_str (), &file_data);

      if (hList == INVALID_HANDLE_VALUE) {
          FindClose (hList);
          return;
      }

      bool finished = false;

      while (!finished) {
          VisPluginRef **ref = NULL;

          if (!(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
              std::string full_path = dir + "/" + file_data.cFileName;

              if (str_has_suffix (full_path, ".dll")) {
                  int count = 0;
                  ref = visual_plugin_get_references (full_path.c_str (), &count);

                  if (ref) {
                      for (int i = 0; i < count; i++)
                          list.push_back (ref[i]);

                      visual_mem_free (ref);
                  }
              }
          }

          if (!FindNextFile (hList, &file_data)) {
              if (GetLastError () == ERROR_NO_MORE_FILES) {
                  finished = true;
              }
          }
      }

      FindClose (hList);
#else
      // NOTE: This typecast is needed for glibc versions that define
      // alphasort() as taking const void * arguments

      typedef int (*ScandirCompareFunc) (const struct dirent **, const struct dirent **);

      struct dirent **namelist;

      int n = scandir (dir.c_str (), &namelist, NULL, ScandirCompareFunc (alphasort));
      if (n < 0)
          return;

      // First two entries are '.' and '..'
      visual_mem_free (namelist[0]);
      visual_mem_free (namelist[1]);

      for (int i = 2; i < n; i++) {
          std::string full_path = dir + "/" + namelist[i]->d_name;

          if (str_has_suffix (full_path, ".so")) {
              int count = 0;
              VisPluginRef** ref = visual_plugin_get_references (full_path.c_str (), &count);

              if (ref) {
                  for (int j = 0; j < count; j++)
                      list.push_back (ref[j]);

                  visual_mem_free (ref);
              }
          }

          visual_mem_free (namelist[i]);
      }

      visual_mem_free (namelist);

#endif
  }

} // LV namespace
