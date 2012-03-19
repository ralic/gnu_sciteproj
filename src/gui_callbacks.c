/**
 * gui_callbacks.c - GUI callback code for SciteProj
 *
 *  Copyright 2006 Roy Wood, 2009-2012 Andreas Rönnquist
 *
 * This file is part of SciteProj.
 * 
 * SciteProj is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SciteProj is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SciteProj.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <string.h>
#include <sys/stat.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>

#include <stdlib.h>
#include <glib/gi18n.h>

#include <locale.h>

#include "gui_callbacks.h"
#include "clicked_node.h"

#include "gui.h"
#include "drag_drop.h"
#include "tree_manipulation.h"
#include "scite_utils.h"
#include "string_utils.h"
#include "prefs.h"
#include "statusbar.h"
#include "graphics.h"
#include "about.h"
#include "properties_dialog.h"
#include "file_utils.h"

#include "addfiles.h"
#include "recent_files.h"
#include "remove.h"
#include "rename.h"
#include "filelist.h"

#include "gtk3_compat.h"


/**
 *		Expands all folders
 */
static gboolean foreach_expand(GtkTreeModel *model,GtkTreePath *path,GtkTreeIter *iter,gpointer data)
{
	expand_tree_row(path,TRUE);
	return FALSE;
}


/**
 *		Collapses all folders
 */
gboolean foreach_collapse(GtkTreeModel *model,GtkTreePath *path,GtkTreeIter *iter,gpointer data)
{
	collapse_tree_row(path);
	return FALSE;
}



/**
 *
 */
void sort_ascending_cb()
{
	GError *err = NULL;	
	
	if (clicked_node.valid && clicked_node.type==ITEMTYPE_FILE) {
		goto EXITPOINT;
	}

	sort_children(&(clicked_node.iter),&err,compare_strings_smaller);
	
EXITPOINT:
	//
	if (err) g_error_free(err);
}


/**
 *
 */
void sort_descending_cb()
{
	GError *err = NULL;	
	
	if (clicked_node.valid && clicked_node.type==ITEMTYPE_FILE) {
		goto EXITPOINT;
	}
	
	sort_children(&clicked_node.iter,&err,compare_strings_bigger);
	
	
EXITPOINT:
	//
	if (err) g_error_free(err);
}



/**
 * Open the selected file.
 *	This is called when a file is rightclicked and open is selected in the menu
 */
void popup_open_file_cb()
{
	gchar *command = NULL;
	GError *err = NULL;
	GtkWidget *dialog = NULL;
	gchar *absFilePath = NULL;
	
	// several files in selection?
		
	// We can only open files
	
	if (!clicked_node.valid || clicked_node.type != ITEMTYPE_FILE) {
		goto EXITPOINT;
	}
	
	if (!open_filename(clicked_node.name,(gchar*)(get_project_directory()),&err)) {
		goto EXITPOINT;
	}
	
	add_file_to_recent(clicked_node.name,NULL);
	
	
EXITPOINT:
	
	if (err != NULL) {
		dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
			_("Could not open selected file: \n\n%s"), err->message);
		
		gtk_dialog_run(GTK_DIALOG (dialog));
	}
	
	if (command) g_free(command);
	if (absFilePath) g_free(absFilePath);
	if (err) g_error_free(err);
	if (dialog) gtk_widget_destroy(dialog);	
}





/**
 *
 */
void collapse_all_items_cb()
{
	gtk_tree_model_foreach(gtk_tree_view_get_model(GTK_TREE_VIEW(projectTreeView)),foreach_collapse,NULL);
}


/**
 *		edit_options_cb
 *			opens the user-specific options-file ($HOME/.sciteproj) in SciTE. 
 */
void edit_options_cb()
{
	GError *err=NULL;
	gchar *command=NULL;
	
	if ((command = g_strdup_printf("open:%s\n", prefs_filename)) == NULL) {
		g_set_error(&err, APP_SCITEPROJ_ERROR, -1, 
			"%s: %s, g_strdup_printf() = NULL",
			"Error formatting SciTE command",
			__func__);
	}
	else {
		if (send_scite_command(command, &err)) {
			// Try to activate SciTE; ignore errors
			
			activate_scite(NULL);
			
			if (gPrefs.give_scite_focus==TRUE) {
				send_scite_command((gchar*)"focus:0",NULL);
			}
		}
	}	
}


/**
 *
 */
void expand_all_items_cb()
{
	gtk_tree_model_foreach(gtk_tree_view_get_model(GTK_TREE_VIEW(projectTreeView)),foreach_expand,NULL);
}


/**
 * step-through function for expand/collapse folder
 *	
 * @param tree_view
 * @param newiter
 * @param tree_path
 */
static void fix_folders_step_through(GtkTreeView *tree_view, GtkTreeIter newiter,GtkTreePath *tree_path)
{
	GtkTreeModel *tree_model = gtk_tree_view_get_model(tree_view);
	
	gchar *relFilePath;
	
	GError *error;
	gint nodeItemType;
	
	GtkTreeIter iter=newiter;

	do {
		
		gtk_tree_model_get(tree_model, &iter, COLUMN_ITEMTYPE, &nodeItemType, -1);
		

		if (nodeItemType==ITEMTYPE_GROUP) {

			GtkTreePath *srcPath = gtk_tree_model_get_path(tree_model, &iter);
			gboolean groupIsExpanded = tree_row_is_expanded(srcPath);
			
			if (groupIsExpanded) {
				set_tree_node_icon(&iter,directory_open_pixbuf,&error);
			} else {
				set_tree_node_icon(&iter,directory_closed_pixbuf,&error);
			}
			
			gtk_tree_model_get(tree_model, &iter, COLUMN_FILEPATH, &relFilePath, -1);
			
			if (gtk_tree_model_iter_has_child(tree_model,&iter)) {
				
				GtkTreeIter newIter;
				gtk_tree_model_iter_children(tree_model,&newIter,&iter);
				fix_folders_step_through(tree_view,newIter,tree_path);
			}
			
			g_free(relFilePath);
			gtk_tree_path_free(srcPath);
		
		} else {
			
		}
	

	} while(gtk_tree_model_iter_next(tree_model,&iter));
}


/**
 * Callback for expand/collapse event of GtkTreeView
 *
 * @param treeView is not used
 * @param arg1 is not used
 * @param arg2 is not used
 * @param user_data is not used
 */
void row_expand_or_collapse_cb(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *tree_path, gpointer user_data)
{
	/* Switch the folder icon open/closed*/
	
	
	// make sure all icons the folder (and folders inside it) are set to a correct icon.
	fix_folders_step_through(tree_view,*iter,tree_path);
}



/**
 * Callback for "Quit" menu item
 */
void quit_menu_cb()
{
	prompt_user_to_save_project();
	
	if (!project_is_dirty()) {
		gtk_main_quit();
	}
}


/**
 * Callback for "About" menu item
 */
void about_menu_cb()
{
	show_about_dialog();	
}


/**
 * Callback for "Save Project As..." menu item
 */
void saveproject_as_menu_cb()
{
	GError *err = NULL;
	
	if (!save_project(NULL,&err)) {
		GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("An error occurred while saving the project: %s"), err->message);
		
		if (dialog) {
			gtk_dialog_run(GTK_DIALOG(dialog));
			
			gtk_widget_destroy(dialog);
		}
	}
	
	if (err) g_error_free(err);
}


/**
 * Callback for "Save Project" menu item
 */
void saveproject_menu_cb()
{
	GError *err = NULL;
	
	gchar *temp_filepath=get_project_filepath();
	
	if (!save_project(temp_filepath,&err)) {
		GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("An error occurred while saving the project: %s"), err->message);
		
		if (dialog) {
			gtk_dialog_run(GTK_DIALOG(dialog));
			
			gtk_widget_destroy(dialog);
		}
	}
	
	if (err) g_error_free(err);
}



/**
 * Callback for "Open Project" menu item
 */
void openproject_menu_cb()
{
	GError *err = NULL;
	
	if (!load_project(NULL, &err)) {
		GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("An error occurred while opening project file: %s"), err->message);
		
		if (dialog) {
			gtk_dialog_run(GTK_DIALOG(dialog));
			
			gtk_widget_destroy(dialog);
		}
	}
	
	if (err) g_error_free(err);
}



/**
 * Callback for "Create Group" menu item
 */
void creategroup_menu_cb()
{
	ask_name_add_group(NULL);
}


/**
 *
 */
gboolean key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer userData)
{
	switch (event->keyval) 
	{
		case GDK_KEY_BackSpace: 
		{
			debug_printf((gchar*)"key_press_cb: keyval = %d = GDK_BackSpace, hardware_keycode = %d\n", event->keyval, event->hardware_keycode);
			break;
		}
		
		case GDK_KEY_Delete: 
		{
			do_remove_node(TRUE);
			break;
		}
		case GDK_KEY_Insert: 
		{
			break;
		}
		case GDK_KEY_F2:
		{
			do_rename_node(TRUE);
			return TRUE;
		}
		case GDK_KEY_F5:
		{
			print_filelist();
			break;
		}
		default: 
		{
			debug_printf("key_press_cb: keyval = %d = '%c', hardware_keycode = %d\n", event->keyval, (char) event->keyval, event->hardware_keycode);
			return FALSE;
		}
	}
	
	if (event->state & GDK_SHIFT_MASK) debug_printf(", GDK_SHIFT_MASK");
	if (event->state & GDK_CONTROL_MASK) debug_printf(", GDK_CONTROL_MASK");
	if (event->state & GDK_MOD1_MASK) debug_printf(", GDK_MOD1_MASK");
	if (event->state & GDK_MOD2_MASK) debug_printf(", GDK_MOD2_MASK");
	if (event->state & GDK_MOD3_MASK) debug_printf(", GDK_MOD3_MASK");
	if (event->state & GDK_MOD4_MASK) debug_printf(", GDK_MOD4_MASK");
	if (event->state & GDK_MOD5_MASK) debug_printf(", GDK_MOD5_MASK");
	
	debug_printf("\n");
	
	return FALSE;
}


/**
 *		search function for the gtk_tree_view_set_search_equal_func
 *		@return TRUE when rows DONT match, FALSE when rows match
 */
gboolean tree_view_search_equal_func(GtkTreeModel *model,gint column,const gchar *key,GtkTreeIter *iter,gpointer search_data)
{
	gchar *filename;
	// For some reason this should return TRUE if the row DONT match
	gboolean res=TRUE;
	
	gtk_tree_model_get(model, iter, COLUMN_FILENAME, &filename, -1);
	
	// zero when matches, which means we should return FALSE
	if (g_ascii_strncasecmp(key,filename,strlen(key))==0) res=FALSE;
	
	g_free(filename);

	
	return res;
}
