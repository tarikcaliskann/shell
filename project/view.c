#include "view.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "controller.h"  // Bu satır eklendi

static GtkWidget *output_view;
static GtkWidget *input_entry;
static GtkWidget *message_view;
static ShmBuf *shmp;

void update_messages(GtkWidget *message_view, ShmBuf *shmp) {
    if (message_view == NULL || shmp == NULL) return;

    char *msg = model_read_messages(shmp);
    if (msg) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(message_view));
        gtk_text_buffer_set_text(buffer, msg, -1);
        free(msg);
    }
}

gboolean timer_callback(gpointer data) {
    update_messages(message_view, shmp);
    return TRUE;
}

void on_enter(GtkEntry *entry, gpointer user_data) {
    const char *input = gtk_entry_get_text(entry);
    if (input == NULL || *input == '\0') {
        return;
    }

    char *result = handle_input(shmp, input);
    if (result) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_view));
        gtk_text_buffer_set_text(buffer, result, -1);
        free(result);
    }

    gtk_entry_set_text(entry, "");
}

void setup_gui(int *argc, char ***argv, ShmBuf *shared_buf) {
    shmp = shared_buf;
    gtk_init(argc, argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Multi-User Shell");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Output view for command results
    output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_view), FALSE);
    GtkWidget *output_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(output_scroll), output_view);
    gtk_box_pack_start(GTK_BOX(vbox), output_scroll, TRUE, TRUE, 0);

    // Message view for shared messages
    message_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(message_view), FALSE);
    GtkWidget *msg_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(msg_scroll), message_view);
    gtk_box_pack_start(GTK_BOX(vbox), msg_scroll, TRUE, TRUE, 0);

    // Input entry
    input_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry), 
                                 "Enter command or @msg message");
    g_signal_connect(input_entry, "activate", G_CALLBACK(on_enter), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), input_entry, FALSE, FALSE, 0);

    // Set up timer to update messages every second
    g_timeout_add(1000, timer_callback, NULL);

    gtk_widget_show_all(window);
    gtk_main();
}

int main(int argc, char *argv[]) {
    ShmBuf *shared_buf = buf_init();
    if (shared_buf == NULL) {
        fprintf(stderr, "Failed to initialize shared buffer\n");
        return EXIT_FAILURE;
    }

    setup_gui(&argc, &argv, shared_buf);
    cleanup(shared_buf);
    return EXIT_SUCCESS;
}
