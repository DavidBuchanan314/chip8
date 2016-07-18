#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chip8.h"

#define WIN_WIDTH 512
#define WIN_HEIGHT 256

static GtkWidget *drawarea;
static GdkPixbuf *pixbuf;
static GtkWidget *playpause;
char *filename;

static gboolean	on_tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer data)
{
	chip8_tick();
	uint8_t *rawimage = chip8_getframe();

	guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
	
	for (int y = 0; y < CHIP8_HEIGHT; y++) {
		for (int x = 0; x < CHIP8_WIDTH; x++) {
			guint ptr = y * CHIP8_WIDTH + x;
			uint8_t value = rawimage[ptr]?0xFF:0x0;
			ptr *= 3;
			pixels[ptr] = value;
			pixels[ptr+1] = value;
			pixels[ptr+2] = value;
		}
	}
	
	gtk_widget_queue_draw(drawarea);

  return G_SOURCE_CONTINUE;
}

static gint draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, WIN_WIDTH, WIN_HEIGHT, GDK_INTERP_NEAREST); // TODO: Use OpenGL
	gdk_cairo_set_source_pixbuf(cr, scaled, 0, 0);
	cairo_paint(cr);
	g_object_unref(scaled);
	
	return TRUE;
}

static void open_callback(GtkApplication* app, GFile *files[], gint n_files, gchar *hint, gpointer user_data)
{
	if (n_files != 1) printf("Warning: too many files specified. Ignoring extras.\n");
	filename = g_file_get_path(files[0]);
	chip8_loadfile(filename);
	chip8_playpause();
	g_application_activate(G_APPLICATION (app));
}

static void update_start_icon() {
	if (chip8_getstatus()) {
		gtk_image_set_from_icon_name(GTK_IMAGE (playpause), "media-playback-pause", GTK_ICON_SIZE_BUTTON);
	} else {
		gtk_image_set_from_icon_name(GTK_IMAGE (playpause), "media-playback-start", GTK_ICON_SIZE_BUTTON);
	}
}

static void open_button_callback(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(
		"Open file",
		GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET (button))),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"Cancel", GTK_RESPONSE_CANCEL,
		"Open", GTK_RESPONSE_ACCEPT,
		NULL
	);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (dialog));
		chip8_loadfile(filename);
		if (!chip8_getstatus()) chip8_playpause(); // start emulator if not already running
		update_start_icon();
	}
	
	gtk_widget_destroy(dialog);
}

static void start_button_callback(GtkButton *button, gpointer user_data)
{
	chip8_playpause();
	update_start_icon();
}

static void reset_button_callback(GtkButton *button, gpointer user_data)
{
	chip8_loadfile(filename);
}

static void key_callback(GtkWidget *widget, GdkEventKey *event)
{
	char *keymap = "x123qweasdzc4rfv";
	if (event -> keyval > 0xFF) return;
	char *match = strchr(keymap, event -> keyval);
	if (match == NULL) return;
	int index = match-keymap;
	chip8_keystate[index] = event -> type == GDK_KEY_PRESS;
	
	if (event -> type == GDK_KEY_PRESS && chip8_waiting) {
		chip8_lastkey = index;
		chip8_playpause();
	}
}

static void activate(GtkApplication* app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *header;
	
	window = gtk_application_window_new(app);
	gtk_window_set_default_size(GTK_WINDOW (window), WIN_WIDTH, WIN_HEIGHT);
	gtk_window_set_type_hint(GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);
	gtk_window_set_resizable(GTK_WINDOW (window), FALSE);
	gtk_widget_set_events(window, GDK_KEY_RELEASE_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(GTK_WINDOW (window), "key_press_event", G_CALLBACK(key_callback), NULL);
	g_signal_connect(GTK_WINDOW (window), "key_release_event", G_CALLBACK(key_callback), NULL);
	
	header = gtk_header_bar_new();
	gtk_header_bar_set_title(GTK_HEADER_BAR (header), "CHIP-8");
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR (header), TRUE);
	gtk_window_set_titlebar(GTK_WINDOW (window), header);
	
	button = gtk_button_new_with_label("Open");
	g_signal_connect(button, "clicked", G_CALLBACK (open_button_callback), NULL);
	gtk_header_bar_pack_start(GTK_HEADER_BAR (header), button);
	
	button = gtk_button_new();
	g_signal_connect(button, "clicked", G_CALLBACK (start_button_callback), NULL);
	playpause = gtk_image_new();
	update_start_icon();
	gtk_container_add(GTK_CONTAINER (button), playpause);
	gtk_header_bar_pack_end(GTK_HEADER_BAR (header), button);
	
	button = gtk_button_new();
	g_signal_connect(button, "clicked", G_CALLBACK (reset_button_callback), NULL);
	gtk_container_add(GTK_CONTAINER (button), gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_BUTTON));
	gtk_header_bar_pack_end(GTK_HEADER_BAR (header), button);
	
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, CHIP8_WIDTH, CHIP8_HEIGHT);
	
	drawarea = gtk_drawing_area_new ();
	g_signal_connect(drawarea, "draw", G_CALLBACK (draw_callback), NULL);
	gtk_container_add(GTK_CONTAINER (window), drawarea);
	gtk_widget_add_tick_callback (drawarea, on_tick, NULL, NULL);
	
	gtk_widget_show_all(window);
}

int main(int argc, char *argv[])
{
	GtkApplication *app;
	int status;

	chip8_init();
	memset(chip8_keystate, 0, sizeof chip8_keystate);

	app = gtk_application_new("com.github.davidbuchanan314.chip8", G_APPLICATION_HANDLES_OPEN);
	g_signal_connect(app, "activate", G_CALLBACK (activate), NULL);
	g_signal_connect(app, "open", G_CALLBACK (open_callback), NULL);
	status = g_application_run(G_APPLICATION (app), argc, argv);
	g_object_unref(app);

	return status;
}

