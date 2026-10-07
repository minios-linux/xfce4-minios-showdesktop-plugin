/*
 * Copyright (c) 2005-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "showdesktop.h"

#include "common/panel-private.h"
#include "common/panel-utils.h"
#include "common/panel-xfconf.h"

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <libintl.h>



#define DRAG_ACTIVATE_TIMEOUT (500)
#define HOVER_ACTIVATE_TIMEOUT (500)
#define MINIOS_(String) dgettext (MINIOS_GETTEXT_PACKAGE, String)



static void
minios_show_desktop_plugin_screen_changed (GtkWidget *widget,
                                    GdkScreen *previous_screen);
static void
minios_show_desktop_plugin_construct (XfcePanelPlugin *panel_plugin);
static void
minios_show_desktop_plugin_free_data (XfcePanelPlugin *panel_plugin);
static gboolean
minios_show_desktop_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                  gint size);
static void
minios_show_desktop_plugin_toggled (GtkToggleButton *button,
                             MiniOSShowDesktopPlugin *plugin);
static gboolean
minios_show_desktop_plugin_button_release_event (GtkToggleButton *button,
                                          GdkEventButton *event,
                                          MiniOSShowDesktopPlugin *plugin);
static void
minios_show_desktop_plugin_show_desktop_changed (XfwScreen *xfw_screen,
                                          GParamSpec *pspec,
                                          MiniOSShowDesktopPlugin *plugin);
static void
minios_show_desktop_plugin_drag_leave (GtkWidget *widget,
                                GdkDragContext *context,
                                guint time,
                                MiniOSShowDesktopPlugin *plugin);
static gboolean
minios_show_desktop_plugin_drag_motion (GtkWidget *widget,
                                 GdkDragContext *context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 MiniOSShowDesktopPlugin *plugin);
static gboolean
minios_show_desktop_plugin_enter (GtkToggleButton *widget,
                           GdkEventCrossing *event,
                           MiniOSShowDesktopPlugin *plugin);
static gboolean
minios_show_desktop_plugin_leave (GtkToggleButton *widget,
                           GdkEventCrossing *event,
                           MiniOSShowDesktopPlugin *plugin);
static void
minios_show_desktop_plugin_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec);
static void
minios_show_desktop_plugin_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec);
static void
minios_showdesktop_configure (XfcePanelPlugin *panel_plugin);
static void
minios_show_desktop_plugin_update_appearance (MiniOSShowDesktopPlugin *plugin);
static void
minios_show_desktop_plugin_get_preferred_width (GtkWidget *widget,
                                                gint *minimum_width,
                                                gint *natural_width);
static void
minios_show_desktop_plugin_get_preferred_height (GtkWidget *widget,
                                                 gint *minimum_height,
                                                 gint *natural_height);



struct _MiniOSShowDesktopPlugin
{
  XfcePanelPlugin __parent__;

  /* the toggle button */
  GtkWidget *button;
  GtkWidget *icon;

  /* Dnd timeout */
  guint drag_timeout;

  /* mouse hover timeout */
  gboolean show_on_hover;
  guint enter_timeout_id;
  gboolean shown_on_hover;

  /* appearance */
  gboolean show_icon;
  guint strip_style;
  gboolean constructed;

  /* the xfw screen */
  XfwScreen *xfw_screen;
};

enum
{
  PROP_0,
  PROP_SHOW_ON_HOVER,
  PROP_SHOW_ICON,
  PROP_STRIP_STYLE,
  N_PROPERTIES,
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (MiniOSShowDesktopPlugin, minios_show_desktop_plugin)



static void
minios_show_desktop_plugin_class_init (MiniOSShowDesktopPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = minios_show_desktop_plugin_construct;
  plugin_class->free_data = minios_show_desktop_plugin_free_data;
  plugin_class->size_changed = minios_show_desktop_plugin_size_changed;
  plugin_class->configure_plugin = minios_showdesktop_configure;

  widget_class->get_preferred_width = minios_show_desktop_plugin_get_preferred_width;
  widget_class->get_preferred_height = minios_show_desktop_plugin_get_preferred_height;

  gobject_class->set_property = minios_show_desktop_plugin_set_property;
  gobject_class->get_property = minios_show_desktop_plugin_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_ON_HOVER,
                                   g_param_spec_boolean ("show-on-hover",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_ICON,
                                   g_param_spec_boolean ("show-icon",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_STRIP_STYLE,
                                   g_param_spec_uint ("strip-style",
                                                      NULL, NULL,
                                                      0, 2, 0,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
}



static void
minios_show_desktop_plugin_init (MiniOSShowDesktopPlugin *plugin)
{
  GtkWidget *button;

  bindtextdomain (MINIOS_GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (MINIOS_GETTEXT_PACKAGE, "UTF-8");

  plugin->xfw_screen = NULL;

  /* monitor screen changes */
  g_signal_connect (G_OBJECT (plugin), "screen-changed",
                    G_CALLBACK (minios_show_desktop_plugin_screen_changed), NULL);

  /* create the toggle button */
  button = plugin->button = xfce_panel_create_toggle_button ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (plugin), button);
  gtk_widget_set_name (button, "minios-showdesktop-button");
  g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (minios_show_desktop_plugin_toggled), plugin);
  g_signal_connect (G_OBJECT (button), "button-release-event",
                    G_CALLBACK (minios_show_desktop_plugin_button_release_event), plugin);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), button);
  gtk_widget_show (button);

  /* allow toggle the button when mouse hover long time.*/
  g_signal_connect (G_OBJECT (plugin->button), "enter-notify-event",
                    G_CALLBACK (minios_show_desktop_plugin_enter), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "leave-notify-event",
                    G_CALLBACK (minios_show_desktop_plugin_leave), plugin);

  /* allow toggle the button when drag something.*/
  gtk_drag_dest_set (GTK_WIDGET (plugin->button), 0, NULL, 0, 0);
  g_signal_connect (G_OBJECT (plugin->button), "drag_motion",
                    G_CALLBACK (minios_show_desktop_plugin_drag_motion), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag_leave",
                    G_CALLBACK (minios_show_desktop_plugin_drag_leave), plugin);

  plugin->icon = gtk_image_new_from_icon_name ("org.xfce.panel.showdesktop", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), plugin->icon);
  gtk_widget_show (plugin->icon);

}



static void
minios_show_desktop_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (panel_plugin);
  const PanelProperty properties[] = {
    { "show-on-hover", G_TYPE_BOOLEAN },
    { "show-icon", G_TYPE_BOOLEAN },
    { "strip-style", G_TYPE_UINT },
    { NULL }
  };

  plugin->constructed = TRUE;
  xfce_panel_plugin_set_small (panel_plugin, TRUE);
  xfce_panel_plugin_menu_show_configure (panel_plugin);
  panel_properties_bind (NULL, G_OBJECT (panel_plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);
  minios_show_desktop_plugin_update_appearance (plugin);
}



static void
minios_show_desktop_plugin_screen_changed (GtkWidget *widget,
                                    GdkScreen *previous_screen)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (widget);
  XfwScreen *xfw_screen;

  panel_return_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (widget));

  /* get the new xfw screen */
  xfw_screen = xfw_screen_get_default ();
  panel_return_if_fail (XFW_IS_SCREEN (xfw_screen));

  /* leave when the xfw screen did not change */
  if (plugin->xfw_screen == xfw_screen)
    {
      g_object_unref (xfw_screen);
      return;
    }

  /* disconnect signals from an existing xfw screen */
  if (plugin->xfw_screen != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->xfw_screen),
                                            minios_show_desktop_plugin_show_desktop_changed, plugin);
      g_object_unref (plugin->xfw_screen);
    }

  /* set the new xfw screen */
  plugin->xfw_screen = xfw_screen;
  g_signal_connect (G_OBJECT (xfw_screen), "notify::show-desktop",
                    G_CALLBACK (minios_show_desktop_plugin_show_desktop_changed), plugin);

  /* toggle the button to the current state or update the tooltip */
  if (G_UNLIKELY (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->button))
                  != xfw_screen_get_show_desktop (xfw_screen)))
    minios_show_desktop_plugin_show_desktop_changed (xfw_screen, NULL, plugin);
  else
    minios_show_desktop_plugin_toggled (GTK_TOGGLE_BUTTON (plugin->button), plugin);
}



static void
minios_show_desktop_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (panel_plugin);

  /* disconnect screen changed signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin),
                                        minios_show_desktop_plugin_screen_changed, NULL);

  /* disconnect handle */
  if (plugin->xfw_screen != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->xfw_screen),
                                            minios_show_desktop_plugin_show_desktop_changed, plugin);
      g_object_unref (plugin->xfw_screen);
    }

  if (plugin->enter_timeout_id != 0)
    g_source_remove (plugin->enter_timeout_id);
}



static gboolean
minios_show_desktop_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                  gint size)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (panel_plugin);
  gint icon_size;

  panel_return_val_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (panel_plugin), FALSE);

  size /= xfce_panel_plugin_get_nrows (panel_plugin);
  icon_size = xfce_panel_plugin_get_icon_size (panel_plugin);
  gtk_image_set_pixel_size (GTK_IMAGE (plugin->icon), icon_size);
  minios_show_desktop_plugin_update_appearance (plugin);

  return TRUE;
}



static void
minios_show_desktop_plugin_toggled (GtkToggleButton *button,
                             MiniOSShowDesktopPlugin *plugin)
{
  gboolean active;
  const gchar *text;

  panel_return_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  panel_return_if_fail (XFW_IS_SCREEN (plugin->xfw_screen));

  plugin->shown_on_hover = FALSE;

  /* toggle the desktop */
  active = gtk_toggle_button_get_active (button);
  if (active != xfw_screen_get_show_desktop (plugin->xfw_screen))
    xfw_screen_set_show_desktop (plugin->xfw_screen, active);

  if (active)
    text = _("Restore the minimized windows");
  else
    text = _("Minimize all open windows and show the desktop");

  gtk_widget_set_tooltip_text (GTK_WIDGET (button), text);
  panel_utils_set_atk_info (GTK_WIDGET (button), _("Show Desktop"), text);
}



static gboolean
minios_show_desktop_plugin_button_release_event (GtkToggleButton *button,
                                          GdkEventButton *event,
                                          MiniOSShowDesktopPlugin *plugin)
{
  XfwWorkspaceManager *manager;
  XfwWorkspace *active_ws;
  GList *windows, *li;
  XfwWindow *window;

  panel_return_val_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (XFW_IS_SCREEN (plugin->xfw_screen), FALSE);

  if (event->button == 2)
    {
      manager = xfw_screen_get_workspace_manager (plugin->xfw_screen);
      li = xfw_workspace_manager_list_workspace_groups (manager);
      if (li == NULL)
        return FALSE;

      active_ws = xfw_workspace_group_get_active_workspace (li->data);
      windows = xfw_screen_get_windows (plugin->xfw_screen);

      for (li = windows; li != NULL; li = li->next)
        {
          window = XFW_WINDOW (li->data);

          if (xfw_window_get_workspace (window) != active_ws)
            continue;

          /* toggle the shade state */
          if (xfw_window_is_shaded (window))
            xfw_window_set_shaded (window, FALSE, NULL);
          else
            xfw_window_set_shaded (window, TRUE, NULL);
        }
    }

  return FALSE;
}



static void
minios_show_desktop_plugin_show_desktop_changed (XfwScreen *xfw_screen,
                                          GParamSpec *pspec,
                                          MiniOSShowDesktopPlugin *plugin)
{
  panel_return_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (plugin));
  panel_return_if_fail (XFW_IS_SCREEN (xfw_screen));
  panel_return_if_fail (plugin->xfw_screen == xfw_screen);

  if (plugin->shown_on_hover)
    return;

  /* update button to user action */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button),
                                xfw_screen_get_show_desktop (xfw_screen));
}



static gboolean
minios_show_desktop_plugin_drag_timeout (gpointer data)
{
  MiniOSShowDesktopPlugin *plugin = (MiniOSShowDesktopPlugin *) data;

  plugin->drag_timeout = 0;

  /* activate button to toggle show desktop */
  g_signal_emit_by_name (G_OBJECT (plugin->button), "clicked", plugin);

  return FALSE;
}



static void
minios_show_desktop_plugin_drag_leave (GtkWidget *widget,
                                GdkDragContext *context,
                                guint time,
                                MiniOSShowDesktopPlugin *plugin)
{
  if (plugin->drag_timeout != 0)
    {
      g_source_remove (plugin->drag_timeout);
      plugin->drag_timeout = 0;
    }

  gtk_drag_unhighlight (GTK_WIDGET (widget));
}



static gboolean
minios_show_desktop_plugin_drag_motion (GtkWidget *widget,
                                 GdkDragContext *context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 MiniOSShowDesktopPlugin *plugin)
{
  if (plugin->drag_timeout == 0)
    plugin->drag_timeout = g_timeout_add (DRAG_ACTIVATE_TIMEOUT,
                                          minios_show_desktop_plugin_drag_timeout,
                                          plugin);

  gtk_drag_highlight (GTK_WIDGET (widget));

  gdk_drag_status (context, 0, time);

  return TRUE;
}



static gboolean
minios_show_desktop_plugin_enter_timeout (gpointer data)
{
  MiniOSShowDesktopPlugin *plugin = (MiniOSShowDesktopPlugin *) data;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->button)))
    {
      plugin->shown_on_hover = TRUE;
      xfw_screen_set_show_desktop (plugin->xfw_screen, TRUE);
    }

  plugin->enter_timeout_id = 0;

  return FALSE;
}



static gboolean
minios_show_desktop_plugin_enter (GtkToggleButton *widget,
                           GdkEventCrossing *event,
                           MiniOSShowDesktopPlugin *plugin)
{
  if (!plugin->show_on_hover)
    return FALSE;

  if (plugin->enter_timeout_id == 0
      && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->button)))
    {
      plugin->enter_timeout_id = g_timeout_add (HOVER_ACTIVATE_TIMEOUT,
                                                minios_show_desktop_plugin_enter_timeout,
                                                plugin);
    }

  return FALSE;
}



static gboolean
minios_show_desktop_plugin_leave (GtkToggleButton *button,
                           GdkEventCrossing *event,
                           MiniOSShowDesktopPlugin *plugin)
{
  if (!plugin->show_on_hover)
    return FALSE;

  if (plugin->shown_on_hover)
    {
      plugin->shown_on_hover = FALSE;
      xfw_screen_set_show_desktop (plugin->xfw_screen, FALSE);
    }

  if (plugin->enter_timeout_id != 0)
    {
      g_source_remove (plugin->enter_timeout_id);
      plugin->enter_timeout_id = 0;
    }

  return FALSE;
}



static void
minios_show_desktop_plugin_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_SHOW_ON_HOVER:
      plugin->show_on_hover = g_value_get_boolean (value);
      break;

    case PROP_SHOW_ICON:
      plugin->show_icon = g_value_get_boolean (value);
      if (plugin->constructed && plugin->button != NULL)
        minios_show_desktop_plugin_update_appearance (plugin);
      break;

    case PROP_STRIP_STYLE:
      plugin->strip_style = g_value_get_uint (value);
      if (plugin->constructed && plugin->button != NULL)
        minios_show_desktop_plugin_update_appearance (plugin);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
minios_show_desktop_plugin_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_SHOW_ON_HOVER:
      g_value_set_boolean (value, plugin->show_on_hover);
      break;

    case PROP_SHOW_ICON:
      g_value_set_boolean (value, plugin->show_icon);
      break;

    case PROP_STRIP_STYLE:
      g_value_set_uint (value, plugin->strip_style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gint
minios_show_desktop_plugin_strip_thickness (MiniOSShowDesktopPlugin *plugin)
{
  switch (plugin->strip_style)
    {
    case 0: return 4;
    case 1: return 8;
    case 2: return 14;
    default: return 8;
    }
}


static void
minios_show_desktop_plugin_get_preferred_width (GtkWidget *widget,
                                                gint *minimum_width,
                                                gint *natural_width)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (widget);
  gint width;

  if (!plugin->constructed || plugin->show_icon)
    {
      GTK_WIDGET_CLASS (minios_show_desktop_plugin_parent_class)->get_preferred_width (widget, minimum_width, natural_width);
      return;
    }

  if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) == GTK_ORIENTATION_HORIZONTAL)
    width = minios_show_desktop_plugin_strip_thickness (plugin);
  else
    width = xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin));

  if (minimum_width != NULL) *minimum_width = width;
  if (natural_width != NULL) *natural_width = width;
}


static void
minios_show_desktop_plugin_get_preferred_height (GtkWidget *widget,
                                                 gint *minimum_height,
                                                 gint *natural_height)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (widget);
  gint height;

  if (!plugin->constructed || plugin->show_icon)
    {
      GTK_WIDGET_CLASS (minios_show_desktop_plugin_parent_class)->get_preferred_height (widget, minimum_height, natural_height);
      return;
    }

  if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) == GTK_ORIENTATION_HORIZONTAL)
    height = xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin));
  else
    height = minios_show_desktop_plugin_strip_thickness (plugin);

  if (minimum_height != NULL) *minimum_height = height;
  if (natural_height != NULL) *natural_height = height;
}


static void
minios_show_desktop_plugin_update_appearance (MiniOSShowDesktopPlugin *plugin)
{
  GtkStyleContext *context;
  gint size;

  panel_return_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (plugin));
  if (plugin->button == NULL || plugin->icon == NULL)
    return;

  context = gtk_widget_get_style_context (plugin->button);
  size = xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin));
  size /= MAX (xfce_panel_plugin_get_nrows (XFCE_PANEL_PLUGIN (plugin)), 1);

  gtk_style_context_remove_class (context, "minios-strip");
  gtk_style_context_remove_class (context, "strip-soft");
  gtk_style_context_remove_class (context, "strip-thin");
  gtk_style_context_remove_class (context, "strip-wide");

  xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (plugin), plugin->show_icon);

  if (plugin->show_icon)
    {
      gtk_widget_show (plugin->icon);
      gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);
      gtk_widget_set_size_request (plugin->button, -1, -1);
    }
  else
    {
      GtkOrientation orientation = xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin));
      gint thickness;

      gtk_style_context_add_class (context, "minios-strip");
      switch (plugin->strip_style)
        {
        case 0:
          gtk_style_context_add_class (context, "strip-thin");
          break;
        case 1:
          gtk_style_context_add_class (context, "strip-soft");
          break;
        case 2:
          gtk_style_context_add_class (context, "strip-wide");
          break;
        default:
          gtk_style_context_add_class (context, "strip-soft");
          break;
        }
      thickness = minios_show_desktop_plugin_strip_thickness (plugin);

      gtk_widget_hide (plugin->icon);
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_set_size_request (GTK_WIDGET (plugin), thickness, size);
          gtk_widget_set_size_request (plugin->button, thickness, size);
        }
      else
        {
          gtk_widget_set_size_request (GTK_WIDGET (plugin), size, thickness);
          gtk_widget_set_size_request (plugin->button, size, thickness);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (plugin));
  gtk_widget_queue_resize (plugin->button);
}



void
minios_showdesktop_configure (XfcePanelPlugin *panel_plugin)
{
  MiniOSShowDesktopPlugin *plugin = MINIOS_SHOW_DESKTOP_PLUGIN (panel_plugin);
  GtkBuilder *builder;
  GObject *dialog;
  GObject *show_on_mouse_hover;
  GtkWidget *show_icon;
  GtkWidget *style_box;
  GtkWidget *style_label;
  GtkWidget *style_combo;
  GtkWidget *box;

  panel_return_if_fail (MINIOS_SHOW_DESKTOP_IS_PLUGIN (plugin));

  /* setup the dialog */
  builder = panel_utils_builder_new (panel_plugin, "/org/xfce/panel/showdesktop-dialog.glade", &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

  show_on_mouse_hover = gtk_builder_get_object (builder, "show-on-hover");
  g_object_bind_property (G_OBJECT (plugin), "show-on-hover",
                          G_OBJECT (show_on_mouse_hover), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  box = GTK_WIDGET (gtk_builder_get_object (builder, "vbox3"));
  show_icon = gtk_check_button_new_with_mnemonic (MINIOS_("Show _icon"));
  gtk_box_pack_start (GTK_BOX (box), show_icon, FALSE, FALSE, 0);
  g_object_bind_property (G_OBJECT (plugin), "show-icon",
                          G_OBJECT (show_icon), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_widget_show (show_icon);

  style_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  style_label = gtk_label_new_with_mnemonic (MINIOS_("Strip _style:"));
  style_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (style_combo), MINIOS_("Thin"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (style_combo), MINIOS_("Medium"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (style_combo), MINIOS_("Wide"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (style_label), style_combo);
  gtk_box_pack_start (GTK_BOX (style_box), style_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (style_box), style_combo, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), style_box, FALSE, FALSE, 0);
  g_object_bind_property (G_OBJECT (plugin), "strip-style",
                          G_OBJECT (style_combo), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (G_OBJECT (show_icon), "active",
                          G_OBJECT (style_box), "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_show_all (style_box);

  gtk_widget_show (GTK_WIDGET (dialog));
}
