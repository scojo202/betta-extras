/*
 * b-video-window.h :
 *
 * Copyright (C) 2019 Scott O. Johnson (scojo202@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gtk/gtk.h>

#pragma once

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BVideoWindow,b_video_window,B,VIDEO_WINDOW,GtkApplicationWindow)

#define B_TYPE_VIDEO_WINDOW                  (b_video_window_get_type ())

void b_video_window_on(BVideoWindow *self);
void b_video_window_attach_settings(BVideoWindow *vw, GSettings *settings);

G_END_DECLS
