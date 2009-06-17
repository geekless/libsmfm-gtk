/*
 *      fm-file-ops-job.c
 *      
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
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

#include "fm-file-ops-job.h"
#include "fm-deep-count-job.h"

static void fm_file_ops_job_finalize  			(GObject *object);

G_DEFINE_TYPE(FmFileOpsJob, fm_file_ops_job, FM_TYPE_JOB);

static gboolean fm_file_ops_job_run(FmJob* job);
static void fm_file_ops_job_cancel(FmJob* job);

static gboolean job_func(GIOSchedulerJob *job, GCancellable *cancellable, gpointer user_data);

static void fm_file_ops_job_class_init(FmFileOpsJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_ops_job_finalize;

	job_class = FM_JOB_CLASS(klass);
	job_class->cancel = fm_file_ops_job_cancel;
	job_class->run = fm_file_ops_job_run;
	job_class->finished = NULL;
}


static void fm_file_ops_job_finalize(GObject *object)
{
	FmFileOpsJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_FILE_OPS_JOB(object));

	self = FM_FILE_OPS_JOB(object);
	g_object_unref(self->cancellable);
	if(self->srcs);
		fm_list_unref(self->srcs);
	if(self->dest)
		fm_path_unref(self->dest);

	G_OBJECT_CLASS(fm_file_ops_job_parent_class)->finalize(object);
}


static void fm_file_ops_job_init(FmFileOpsJob *self)
{
	
}


FmJob *fm_file_ops_job_new(FmFileOpType type, FmPathList* srcs, FmPath* dest)
{
	FmFileOpsJob* job = (FmFileOpsJob*)g_object_new(FM_FILE_OPS_JOB_TYPE, NULL);
	
	return (FmJob*)job;
}

gboolean fm_file_ops_job_run(FmJob* job)
{
	FmFileOpsJob* fop_job = (FmFileOpsJob*)job;

	/* prepare the job, count total work needed with FmDeepCountJob */
	fop_job->dc_job = fm_deep_count_job_new(fop_job->srcs);
	//g_signal_connect(fop_job->dc_job, "finished", );
	fm_job_run(fop_job->dc_job);
/*
	g_io_scheduler_push_job(job_func, dc, 
			(GDestroyNotify)g_object_unref,
			G_PRIORITY_DEFAULT, dc->cancellable);
*/
	return TRUE;
}

void fm_file_ops_job_cancel(FmJob* job)
{
	FmFileOpsJob* dc = (FmFileOpsJob*)job;
	g_cancellable_cancel(dc->cancellable);
}

#if 0
static void file_ops(FmFileOpsJob* job, GFile* dir)
{
	GError* err = NULL;
	if(g_file_is_native(dir)) /* if it's a native file, use posix APIs */
	{
		char* dir_path = g_file_get_path(dir);
		GDir* dir_ent = g_dir_open(dir_path, 0, NULL);
		if(dir_ent)
		{
			char* basename;
			struct stat st;
			while( !g_cancellable_is_cancelled(job->cancellable)
				&& (basename = g_dir_read_name(dir_ent)) )
			{
				char* full_path = g_build_filename(dir_path, basename, NULL);
				if( lstat(full_path, &st) == 0 )
				{
					job->total_size += (goffset)st.st_size;
					job->total_block_size += (st.st_blocks * st.st_blksize);
					if( S_ISDIR(st.st_mode) )
					{
						GFile* sub = g_file_new_for_path(full_path);
						file_ops(job, sub);
						g_object_unref(sub);
					}
				}
				else
				{
					/* error! */
				}
				g_free(full_path);
			}
			g_dir_close(dir_ent);
		}
		g_free(dir_path);
	}
	else /* use gio */
	{
		GFileEnumerator* enu = g_file_enumerate_children(dir, 
						G_FILE_ATTRIBUTE_STANDARD_TYPE","
						G_FILE_ATTRIBUTE_STANDARD_NAME","
						G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
						G_FILE_ATTRIBUTE_STANDARD_SIZE","
						G_FILE_ATTRIBUTE_UNIX_BLOCKS","
						G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE,
						0, job->cancellable, &err);
		while( !g_cancellable_is_cancelled(job->cancellable) )
		{
			GFileInfo* inf = g_file_enumerator_next_file(enu, job->cancellable, &err);
			if(inf)
			{
				if( !g_file_info_get_attribute_boolean(inf, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL) )
				{
					guint64 blk = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_UNIX_BLOCKS);
					guint32 blk_size= g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE);
					++job->count;
					job->total_size += g_file_info_get_size(inf);
					job->total_block_size += (blk * blk_size);
				}
				if( g_file_info_get_file_type(inf)==G_FILE_TYPE_DIRECTORY )
				{
					GFile* sub = g_file_get_child(dir, g_file_info_get_name(inf));
					file_ops(job, sub);
					g_object_unref(sub);
				}
			}
			else
			{
				break; /* FIXME: error handling */
			}
			g_object_unref(inf);
		}
		g_file_enumerator_close(enu, NULL, &err);
		g_object_unref(enu);
	}
}

#endif

static gboolean on_cancelled(FmFileOpsJob* job)
{
	fm_job_emit_cancelled((FmJob*)job);
	return FALSE;
}

static gboolean on_finished(FmFileOpsJob* job)
{
	fm_job_emit_finished((FmJob*)job);
	return FALSE;
}

gboolean job_func(GIOSchedulerJob *job, GCancellable *cancellable, gpointer user_data)
{
	FmFileOpsJob* dc = (FmFileOpsJob*)user_data;
	dc->io_job = job;
	// file_ops( dc, dc->gf );
	if(g_cancellable_is_cancelled(cancellable))
	{
		g_io_scheduler_job_send_to_mainloop(job, on_cancelled, dc, NULL);	
	}
	else
	{
		g_io_scheduler_job_send_to_mainloop(job, on_finished, dc, NULL);	
	}
	return FALSE;
}

