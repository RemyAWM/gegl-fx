/* This file is part of GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2003-2007 Calvin Williamson, Øyvind Kolås
 */

#include "config.h"
#define __GEGL_INIT_C

#include <babl/babl.h>

#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <locale.h>

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <process.h>
#endif

#ifdef G_OS_WIN32

#include <windows.h>

static HMODULE hLibGeglModule = NULL;

/* DllMain prototype */
BOOL WINAPI DllMain (HINSTANCE hinstDLL,
                     DWORD     fdwReason,
                     LPVOID    lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
  hLibGeglModule = hinstDLL;
  return TRUE;
}

static inline gboolean
pid_is_running (gint pid)
{
  HANDLE h;
  DWORD exitcode = 0;

  h = OpenProcess (PROCESS_QUERY_INFORMATION, FALSE, pid);
  GetExitCodeProcess (h, &exitcode);
  CloseHandle (h);

  return exitcode == STILL_ACTIVE;
}

#else

#include <sys/types.h>
#include <signal.h>

static inline gboolean
pid_is_running (gint pid)
{
  return (kill (pid, 0) == 0);
}
#endif


#include <gegl-debug.h>


guint gegl_debug_flags = 0;

#include "gegl-instrument.h"
#include "gegl-init.h"
#include "module/geglmodule.h"
#include "module/geglmoduledb.h"
#include "gegl-types-internal.h"
#include "buffer/gegl-buffer.h"
#include "operation/gegl-operation.h"
#include "operation/gegl-operations.h"
#include "operation/gegl-extension-handler.h"
#include "buffer/gegl-buffer-private.h"
#include "buffer/gegl-buffer-iterator-private.h"
#include "buffer/gegl-tile-backend-ram.h"
#include "buffer/gegl-tile-backend-tiledir.h"
#include "buffer/gegl-tile-backend-file.h"
#include "gegl-config.h"
#include "graph/gegl-node.h"

static const gchar *makefile (void);

static gboolean  gegl_post_parse_hook (GOptionContext *context,
                                       GOptionGroup   *group,
                                       gpointer        data,
                                       GError        **error);


static GeglConfig   *config = NULL;

static GeglModuleDB *module_db   = NULL;

static glong         global_time = 0;

static gboolean      swap_init_done = FALSE;

static gchar        *swap_dir = NULL;

static void
gegl_init_swap_dir (void)
{
  gchar *swapdir = NULL;

  if (config->swap)
    {
      if (g_ascii_strcasecmp (config->swap, "ram") == 0)
        {
          swapdir = NULL;
        }
      else
        {
          swapdir = g_strstrip (g_strdup (config->swap));

          /* Remove any trailing separator, unless the path is only made of a leading separator. */
          while (strlen (swapdir) > strlen (G_DIR_SEPARATOR_S) && g_str_has_suffix (swapdir, G_DIR_SEPARATOR_S))
            swapdir[strlen (swapdir) - strlen (G_DIR_SEPARATOR_S)] = '\0';
        }
    }

  if (swapdir &&
      ! g_file_test (swapdir, G_FILE_TEST_IS_DIR) &&
      g_mkdir_with_parents (swapdir, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
    {
      g_free (swapdir);
      swapdir = NULL;
    }

  g_object_set (config, "swap", swapdir, NULL);

  swap_dir = swapdir;

  swap_init_done = TRUE;
}

/* Return the swap directory, or NULL if swapping is disabled */
const gchar *
gegl_swap_dir (void)
{
  if (!swap_init_done)
  {
    g_critical ("swap parsing not done");
    gegl_init_swap_dir ();
  }

  return swap_dir;
}

static void
gegl_config_use_opencl_notify (GObject    *gobject,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  GeglConfig *cfg = GEGL_CONFIG (gobject);
  gboolean use_opencl = cfg->use_opencl;

  if (use_opencl)
  {
    g_signal_handlers_block_by_func (gobject,
                                     gegl_config_use_opencl_notify,
                                     NULL);

    use_opencl = gegl_cl_init (NULL);

    g_object_set (gobject,
                  "use-opencl", use_opencl,
                  NULL);

    g_signal_handlers_unblock_by_func (gobject,
                                       gegl_config_use_opencl_notify,
                                       NULL);
  }
}

static void
gegl_init_i18n (void)
{
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GEGL_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
gegl_init (gint    *argc,
           gchar ***argv)
{
  GOptionContext *context;
  GError         *error = NULL;
  static gboolean initialized = FALSE;

  if (initialized)
    return;

  initialized = TRUE;

  gegl_init_i18n ();

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_main_group (context, gegl_get_option_group ());

  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_option_context_free (context);
}

static gchar    *cmd_gegl_swap           = NULL;
static gchar    *cmd_gegl_cache_size     = NULL;
static gchar    *cmd_gegl_chunk_size     = NULL;
static gchar    *cmd_gegl_quality        = NULL;
static gchar    *cmd_gegl_tile_size      = NULL;
static gchar    *cmd_babl_tolerance      = NULL;
static gchar    *cmd_gegl_threads        = NULL;
static gboolean *cmd_gegl_disable_opencl = NULL;
static gchar    *cmd_gegl_queue_size     = NULL;

static const GOptionEntry cmd_entries[]=
{
    {
     "babl-tolerance", 0, 0,
     G_OPTION_ARG_STRING, &cmd_babl_tolerance,
     N_("babls error tolerance, a value between 0.2 and 0.000000001"), "<float>"
    },
    {
     "gegl-swap", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_swap,
     N_("Where GEGL stores its swap"), "<uri>"
    },
    {
     "gegl-cache-size", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_cache_size,
     N_("How much memory to (approximately) use for caching imagery"), "<megabytes>"
    },
    {
     "gegl-tile-size", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_tile_size,
     N_("Default size of tiles in GeglBuffers"), "<widthxheight>"
    },
    {
     "gegl-chunk-size", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_chunk_size,
     N_("The count of pixels to compute simultaneously"), "pixel count"
    },
    {
     "gegl-quality", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_quality,
     N_("The quality of rendering a value between 0.0(fast) and 1.0(reference)"), "<quality>"
    },
    {
     "gegl-threads", 0, 0,
     G_OPTION_ARG_STRING, &cmd_gegl_threads,
     N_("The number of concurrent processing threads to use"), "<threads>"
    },
    {
      "gegl-disable-opencl", 0, 0,
      G_OPTION_ARG_NONE, &cmd_gegl_disable_opencl,
      N_("Disable OpenCL"), NULL
    },
    {
      "gegl-queue-size", 0, 0,
      G_OPTION_ARG_STRING, &cmd_gegl_queue_size,
      N_("Maximum size of a file backend's writer thread queue (in MB)"), "<count>"
    },
    { NULL }
};

GOptionGroup *
gegl_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("gegl", "GEGL Options", "Show GEGL Options",
                              NULL, NULL);
  g_option_group_add_entries (group, cmd_entries);

  g_option_group_set_parse_hooks (group, NULL, gegl_post_parse_hook);

  return group;
}

static void gegl_config_set_defaults (GeglConfig *config)
{
  gchar *swapdir = g_build_filename (g_get_user_cache_dir(),
                                     GEGL_LIBRARY,
                                     "swap",
                                     NULL);
  g_object_set (config,
                "swap", swapdir,
                NULL);
}

static void gegl_config_parse_env (GeglConfig *config)
{
  if (g_getenv ("GEGL_QUALITY"))
    {
      const gchar *quality = g_getenv ("GEGL_QUALITY");

      if (g_str_equal (quality, "fast"))
        g_object_set (config, "quality", 0.0, NULL);
      else if (g_str_equal (quality, "good"))
        g_object_set (config, "quality", 0.5, NULL);
      else if (g_str_equal (quality, "best"))
        g_object_set (config, "quality", 1.0, NULL);
      else
        g_object_set (config, "quality", atof (quality), NULL);
    }

  if (g_getenv ("GEGL_CACHE_SIZE"))
    config->tile_cache_size = atoll(g_getenv("GEGL_CACHE_SIZE"))* 1024*1024;

  if (g_getenv ("GEGL_CHUNK_SIZE"))
    config->chunk_size = atoi(g_getenv("GEGL_CHUNK_SIZE"));

  if (g_getenv ("GEGL_TILE_SIZE"))
    {
      const gchar *str = g_getenv ("GEGL_TILE_SIZE");
      config->tile_width = atoi(str);
      str = strchr (str, 'x');
      if (str)
        config->tile_height = atoi(str+1);
    }

  if (g_getenv ("GEGL_THREADS"))
    {
      config->threads = atoi(g_getenv("GEGL_THREADS"));

      if (config->threads > GEGL_MAX_THREADS)
        {
          g_warning ("Tried to use %i threads max is %i",
                     config->threads, GEGL_MAX_THREADS);
          config->threads = GEGL_MAX_THREADS;
        }
    }

  if (g_getenv ("GEGL_USE_OPENCL"))
    {
      const char *opencl_env = g_getenv ("GEGL_USE_OPENCL");

      if (g_ascii_strcasecmp (opencl_env, "yes") == 0)
        ;
      else if (g_ascii_strcasecmp (opencl_env, "no") == 0)
        gegl_cl_hard_disable ();
      else
        g_warning ("Unknown value for GEGL_USE_OPENCL: %s", opencl_env);
    }

  if (g_getenv ("GEGL_QUEUE_SIZE"))
    config->queue_size = atoi(g_getenv ("GEGL_QUEUE_SIZE")) * 1024 * 1024;

  if (g_getenv ("GEGL_SWAP"))
    g_object_set (config, "swap", g_getenv ("GEGL_SWAP"), NULL);
}

GeglConfig *gegl_config (void)
{
  if (!config)
    {
      config = g_object_new (GEGL_TYPE_CONFIG, NULL);
      gegl_config_set_defaults (config);
    }
  return config;
}

static void swap_clean (void)
{
  const gchar  *swap_dir = gegl_swap_dir ();
  GDir         *dir;

  if (! swap_dir)
    return;

  dir = g_dir_open (gegl_swap_dir (), 0, NULL);

  if (dir != NULL)
    {
      GPatternSpec *pattern = g_pattern_spec_new ("*");
      const gchar  *name;

      while ((name = g_dir_read_name (dir)) != NULL)
        {
          if (g_pattern_match_string (pattern, name))
            {
              gint readpid = atoi (name);

              if (!pid_is_running (readpid))
                {
                  gchar *fname = g_build_filename (gegl_swap_dir (),
                                                   name,
                                                   NULL);
                  g_unlink (fname);
                  g_free (fname);
                }
            }
         }

      g_pattern_spec_free (pattern);
      g_dir_close (dir);
    }
}

void
gegl_exit (void)
{
  if (!config)
    {
      g_warning("gegl_exit() called without matching call to gegl_init()");
      return;
    }

  GEGL_INSTRUMENT_START()

  gegl_tile_backend_swap_cleanup ();
  gegl_tile_cache_destroy ();
  gegl_operation_gtype_cleanup ();
  gegl_extension_handler_cleanup ();

  if (module_db != NULL)
    {
      g_object_unref (module_db);
      module_db = NULL;
    }

  babl_exit ();

  GEGL_INSTRUMENT_END ("gegl", "gegl_exit")

  /* used when tracking buffer and tile leaks */
  if (g_getenv ("GEGL_DEBUG_BUFS") != NULL)
    {
      gegl_buffer_stats ();
      gegl_tile_backend_ram_stats ();
      gegl_tile_backend_file_stats ();
      gegl_tile_backend_tiledir_stats ();
    }
  global_time = gegl_ticks () - global_time;
  gegl_instrument ("gegl", "gegl", global_time);

  if (gegl_instrument_enabled)
    {
      g_printf ("\n%s", gegl_instrument_utf8 ());
    }

  if (gegl_buffer_leaks ())
    {
      g_printf ("EEEEeEeek! %i GeglBuffers leaked\n", gegl_buffer_leaks ());
#ifdef GEGL_ENABLE_DEBUG
      if (!(gegl_debug_flags & GEGL_DEBUG_BUFFER_ALLOC))
        g_printerr ("To debug GeglBuffer leaks, set the environment "
                    "variable GEGL_DEBUG to \"buffer-alloc\"\n");
#endif
    }
  gegl_tile_cache_destroy ();

  if (gegl_swap_dir ())
    {
      /* remove all files matching <$GEGL_SWAP>/GEGL-<pid>-*.swap */

      guint         pid     = getpid ();
      GDir         *dir     = g_dir_open (gegl_swap_dir (), 0, NULL);

      gchar        *glob    = g_strdup_printf ("%i-*", pid);
      GPatternSpec *pattern = g_pattern_spec_new (glob);
      g_free (glob);

      if (dir != NULL)
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)) != NULL)
            {
              if (g_pattern_match_string (pattern, name))
                {
                  gchar *fname = g_build_filename (gegl_swap_dir (),
                                                   name,
                                                   NULL);
                  g_unlink (fname);
                  g_free (fname);
                }
            }

          g_dir_close (dir);
        }

      g_pattern_spec_free (pattern);
    }
  g_object_unref (config);
  config = NULL;
  global_time = 0;
}

static void swap_clean (void);

void
gegl_get_version (int *major,
                  int *minor,
                  int *micro)
{
  if (major != NULL)
    *major = GEGL_MAJOR_VERSION;

  if (minor != NULL)
    *minor = GEGL_MINOR_VERSION;

  if (micro != NULL)
    *micro = GEGL_MICRO_VERSION;
}

void
gegl_load_module_directory (const gchar *path)
{
  g_return_if_fail (g_file_test (path, G_FILE_TEST_IS_DIR));

  gegl_module_db_load (module_db, path);
}

static gboolean
gegl_post_parse_hook (GOptionContext *context,
                      GOptionGroup   *group,
                      gpointer        data,
                      GError        **error)
{
  GeglConfig *config;

  g_assert (global_time == 0);
  global_time = gegl_ticks ();

  if (g_getenv ("GEGL_DEBUG_TIME") != NULL)
    gegl_instrument_enable ();

  gegl_instrument ("gegl", "gegl_init", 0);

  config = gegl_config ();

  gegl_config_parse_env (config);

  babl_init ();

#ifdef GEGL_ENABLE_DEBUG
  {
    const char *env_string;
    env_string = g_getenv ("GEGL_DEBUG");
    if (env_string != NULL)
      {
        gegl_debug_flags =
          g_parse_debug_string (env_string,
                                gegl_debug_keys,
                                G_N_ELEMENTS (gegl_debug_keys));
        env_string = NULL;
      }
  }
#endif /* GEGL_ENABLE_DEBUG */

  if (cmd_gegl_swap)
    g_object_set (config, "swap", cmd_gegl_swap, NULL);
  if (cmd_gegl_quality)
    config->quality = atof (cmd_gegl_quality);
  if (cmd_gegl_cache_size)
    config->tile_cache_size = atoll (cmd_gegl_cache_size)*1024*1024;
  if (cmd_gegl_chunk_size)
    config->chunk_size = atoi (cmd_gegl_chunk_size);
  if (cmd_gegl_tile_size)
    {
      const gchar *str = cmd_gegl_tile_size;
      config->tile_width = atoi(str);
      str = strchr (str, 'x');
      if (str)
        config->tile_height = atoi(str+1);
    }
  if (cmd_gegl_threads)
    config->threads = atoi (cmd_gegl_threads);
  /* FIXME: This comes after babl init and is useless */
  if (cmd_babl_tolerance)
    g_object_set (config, "babl-tolerance", atof(cmd_babl_tolerance), NULL);
  if (cmd_gegl_disable_opencl)
    gegl_cl_hard_disable ();
  if (cmd_gegl_queue_size)
    config->queue_size = atoi (cmd_gegl_queue_size) * 1024 * 1024;

  gegl_init_swap_dir ();

  GEGL_INSTRUMENT_START();

  gegl_operation_gtype_init ();

  if (!module_db)
    {
      const gchar *gegl_path = g_getenv ("GEGL_PATH");

      module_db = gegl_module_db_new (FALSE);

      if (gegl_path)
        {
          gegl_module_db_load (module_db, gegl_path);
        }
      else
        {
          gchar *module_path;

#ifdef G_OS_WIN32
          {
            gchar *prefix;
            prefix = g_win32_get_package_installation_directory_of_module ( hLibGeglModule );
            module_path = g_build_filename (prefix, "lib", GEGL_LIBRARY, NULL);
            g_free(prefix);
          }
#else
          module_path = g_build_filename (LIBDIR, GEGL_LIBRARY, NULL);
#endif

          gegl_module_db_load (module_db, module_path);
          g_free (module_path);

          /* also load plug-ins from ~/.local/share/gegl-0.0/plugins */
          module_path = g_build_filename (g_get_user_data_dir (),
                                          GEGL_LIBRARY,
                                          "plug-ins",
                                          NULL);

          if (g_mkdir_with_parents (module_path,
                                    S_IRUSR | S_IWUSR | S_IXUSR) == 0)
            {
              gchar *makefile_path = g_build_filename (module_path,
                                                       "Makefile",
                                                       NULL);

              if (! g_file_test (makefile_path, G_FILE_TEST_EXISTS))
                g_file_set_contents (makefile_path, makefile (), -1, NULL);

              g_free (makefile_path);
            }

          gegl_module_db_load (module_db, module_path);
          g_free (module_path);
        }
    }

  GEGL_INSTRUMENT_END ("gegl_init", "load modules");

  gegl_instrument ("gegl", "gegl_init", gegl_ticks () - global_time);

  swap_clean ();

  g_signal_connect (G_OBJECT (config),
                   "notify::use-opencl",
                   G_CALLBACK (gegl_config_use_opencl_notify),
                   NULL);
  g_object_set (config, "use-opencl", config->use_opencl, NULL);

  return TRUE;
}

gboolean
gegl_get_debug_enabled (void)
{
#ifdef GEGL_ENABLE_DEBUG
  return gegl_debug_flags != 0;
#else
  return FALSE;
#endif
}


static const gchar *makefile (void)
{
  return
    "# This is a generic makefile for GEGL operations. Just add .c files,\n"
    "# rename mentions of the filename and opname to the new name, and it should \n"
    "# compile. Operations in this dir should be loaded by GEGL by default\n"
    "# If the operation being written depends on extra libraries, you'd better\n"
    "# add a dedicated target with the extra bits linked in.\n"
    "\n\n"
    "CFLAGS  += `pkg-config gegl --cflags`  -I. -fPIC\n"
    "LDFLAGS += `pkg-config gegl --libs` -shared\n"
    "SHREXT=.so\n"
    "CFILES = $(wildcard ./*.c)\n"
    "SOBJS  = $(subst ./,,$(CFILES:.c=$(SHREXT)))\n"
    "all: $(SOBJS)\n"
    "%$(SHREXT): %.c $(GEGLHEADERS)\n"
    "	@echo $@; $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDADD)\n"
    "clean:\n"
    "	rm -f *$(SHREXT) $(OFILES)\n";
}


