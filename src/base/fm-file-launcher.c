/*
 *      fm-file-launcher.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <libintl.h>
#include <gio/gdesktopappinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include "fm-file-launcher.h"
#include "fm-file-info-job.h"

static void launch_files(GAppLaunchContext* ctx, GAppInfo* app, GList* file_infos)
{

}

gboolean fm_launch_desktop_entry(GAppLaunchContext* ctx, const char* file_or_id, GList* uris, GError** err)
{
    GKeyFile* kf = g_key_file_new();
    gboolean loaded;
    gboolean ret = FALSE;

    if(g_path_is_absolute(file_or_id))
        loaded = g_key_file_load_from_file(kf, file_or_id, 0, err);
    else
    {
        char* tmp = g_strconcat("applications/", file_or_id, NULL);
        loaded = g_key_file_load_from_data_dirs(kf, tmp, NULL, 0, err);
        g_free(tmp);
    }

    if(loaded)
    {
        GList* _uris = NULL;
        GAppInfo* app = NULL;
        char* type = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "Type", NULL);
        if(type)
        {
            if(strcmp(type, "Application") == 0)
                app = g_desktop_app_info_new_from_keyfile(kf);
            else if(strcmp(type, "Link") == 0)
            {
                char* url = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "URL", NULL);
                if(url)
                {
                    char* scheme = g_uri_parse_scheme(url);
                    if(scheme)
                    {
                        /* Damn! this actually relies on gconf to work. */
                        /* FIXME: use our own way to get a usable browser later. */
                        app = g_app_info_get_default_for_uri_scheme(scheme);
                        uris = _uris = g_list_prepend(NULL, url);
                        g_free(scheme);
                    }
                }
            }
            else if(strcmp(type, "Directory") == 0)
            {
                /* FIXME: how should this work? It's not defined in the spec. */
            }
            if(app)
                ret = g_app_info_launch_uris(app, uris, ctx, err);
        }
    }
    g_key_file_free(kf);

    return ret;
}

gboolean fm_launch_files(GAppLaunchContext* ctx, GList* file_infos, FmFileLauncher* launcher, gpointer user_data)
{
    GList* l;
    GHashTable* hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    GList* folders = NULL;
    FmFileInfo* fi;
    GError* err = NULL;
    GAppInfo* app;

    for(l = file_infos; l; l=l->next)
    {
        GList* fis;
        fi = (FmFileInfo*)l->data;
        if(fm_file_info_is_dir(fi))
            folders = g_list_prepend(folders, fi);
        else
        {
            /* FIXME: handle shortcuts, such as the items in menu:// */
            if(fm_path_is_native(fi->path))
            {
                char* filename;
                if(fm_file_info_is_desktop_entry(fi))
                {
                    /* if it's a desktop entry file, directly launch it. */
                    filename = fm_path_to_str(fi->path);
                    if(!fm_launch_desktop_entry(ctx, filename, NULL, &err))
                    {
                        if(launcher->error)
                            launcher->error(ctx, err, user_data);
                        g_error_free(err);
                        err = NULL;
                    }
                    continue;
                }
                else if(fm_file_info_is_executable_type(fi))
                {
                    /* if it's an executable file, directly execute it. */
                    filename = fm_path_to_str(fi->path);
                    /* FIXME: we need to use eaccess/euidaccess here. */
                    if(g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE))
                    {
                        app = g_app_info_create_from_commandline(filename, NULL, 0, NULL);
                        if(app)
                        {
                            if(!g_app_info_launch(app, NULL, ctx, &err))
                            {
                                if(launcher->error)
                                    launcher->error(ctx, err, user_data);
                                g_error_free(err);
                                err = NULL;
                            }
                            g_object_unref(app);
                            continue;
                        }
                    }
                    g_free(filename);
                }
            }
            else /* not a native path */
            {
                if(fm_file_info_is_shortcut(fi) && !fm_file_info_is_dir(fi))
                {
                    /* FIXME: special handling for shortcuts */
                    if(fm_path_is_xdg_menu(fi->path) && fi->target)
                    {
                        if(!fm_launch_desktop_entry(ctx, fi->target, NULL, &err))
                        {
                            if(launcher->error)
                                launcher->error(ctx, err, user_data);
                            g_error_free(err);
                            err = NULL;
                        }
                        continue;
                    }
                }
            }
            if(fi->type && fi->type->type)
            {
                fis = g_hash_table_lookup(hash, fi->type->type);
                fis = g_list_prepend(fis, fi);
                g_hash_table_insert(hash, fi->type->type, fis);
            }
        }
    }

    if(g_hash_table_size(hash) > 0)
    {
        GHashTableIter it;
        const char* type;
        GList* fis;
        g_hash_table_iter_init(&it, hash);
        while(g_hash_table_iter_next(&it, &type, &fis))
        {
            GAppInfo* app = g_app_info_get_default_for_type(type, FALSE);
            if(!app)
            {
                if(launcher->get_app)
                {
                    FmMimeType* mime_type = ((FmFileInfo*)fis->data)->type;
                    app = launcher->get_app(fis, mime_type, user_data, NULL);
                }
            }
            if(app)
            {
                for(l=fis; l; l=l->next)
                {
                    char* uri;
                    fi = (FmFileInfo*)l->data;
                    uri = fm_path_to_uri(fi->path);
                    l->data = uri;
                }
                fis = g_list_reverse(fis);
                g_app_info_launch_uris(app, fis, ctx, err);
                /* free URI strings */
                g_list_foreach(fis, (GFunc)g_free, NULL);
                g_object_unref(app);
            }
            g_list_free(fis);
        }
    }
    g_hash_table_destroy(hash);

    if(folders)
    {
        folders = g_list_reverse(folders);
        if(launcher->open_folder)
        {
            launcher->open_folder(ctx, folders, user_data, &err);
            if(err)
            {
                if(launcher->error)
                    launcher->error(ctx, err, user_data);
                g_error_free(err);
                err = NULL;
            }
        }
        g_list_free(folders);
    }
    return TRUE;
}

gboolean fm_launch_paths(GAppLaunchContext* ctx, GList* paths, FmFileLauncher* launcher, gpointer user_data)
{
    FmJob* job = fm_file_info_job_new(NULL, 0);
    GList* l;
    gboolean ret;
    for(l=paths;l;l=l->next)
        fm_file_info_job_add(FM_FILE_INFO_JOB(job), (FmPath*)l->data);
    ret = fm_job_run_sync_with_mainloop(job);
    if(ret)
    {
        GList* file_infos = fm_list_peek_head_link(FM_FILE_INFO_JOB(job)->file_infos);
        if(file_infos)
            ret = fm_launch_files(ctx, file_infos, launcher, user_data);
        else
            ret = FALSE;
    }
    g_object_unref(job);
    return ret;
}