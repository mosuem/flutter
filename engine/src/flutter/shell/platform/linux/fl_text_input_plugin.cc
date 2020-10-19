// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/fl_text_input_plugin.h"

#include <gtk/gtk.h>

#include "flutter/shell/platform/common/cpp/text_input_model.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_json_method_codec.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_method_channel.h"

static constexpr char kChannelName[] = "flutter/textinput";

static constexpr char kBadArgumentsError[] = "Bad Arguments";

static constexpr char kSetClientMethod[] = "TextInput.setClient";
static constexpr char kShowMethod[] = "TextInput.show";
static constexpr char kSetEditingStateMethod[] = "TextInput.setEditingState";
static constexpr char kClearClientMethod[] = "TextInput.clearClient";
static constexpr char kHideMethod[] = "TextInput.hide";
static constexpr char kUpdateEditingStateMethod[] =
    "TextInputClient.updateEditingState";
static constexpr char kPerformActionMethod[] = "TextInputClient.performAction";
static constexpr char kSetEditableSizeAndTransform[] =
    "TextInput.setEditableSizeAndTransform";
static constexpr char kSetMarkedTextRect[] = "TextInput.setMarkedTextRect";

static constexpr char kInputActionKey[] = "inputAction";
static constexpr char kTextInputTypeKey[] = "inputType";
static constexpr char kTextInputTypeNameKey[] = "name";
static constexpr char kTextKey[] = "text";
static constexpr char kSelectionBaseKey[] = "selectionBase";
static constexpr char kSelectionExtentKey[] = "selectionExtent";
static constexpr char kSelectionAffinityKey[] = "selectionAffinity";
static constexpr char kSelectionIsDirectionalKey[] = "selectionIsDirectional";
static constexpr char kComposingBaseKey[] = "composingBase";
static constexpr char kComposingExtentKey[] = "composingExtent";

static constexpr char kTransform[] = "transform";

static constexpr char kTextAffinityDownstream[] = "TextAffinity.downstream";
static constexpr char kMultilineInputType[] = "TextInputType.multiline";

static constexpr int64_t kClientIdUnset = -1;

struct _FlTextInputPlugin {
  GObject parent_instance;

  FlMethodChannel* channel;

  // Client ID provided by Flutter to report events with.
  int64_t client_id;

  // Input action to perform when enter pressed.
  gchar* input_action;

  // Send newline when multi-line and enter is pressed.
  gboolean input_multiline;

  // Input method.
  GtkIMContext* im_context;

  flutter::TextInputModel* text_model;

  // The owning Flutter view.
  FlView* view;

  // A 4x4 matrix that maps from `EditableText` local coordinates to the
  // coordinate system of `PipelineOwner.rootNode`.
  double editabletext_transform[4][4];

  // The smallest rect, in local coordinates, of the text in the composing
  // range, or of the caret in the case where there is no current composing
  // range. This value is updated via `TextInput.setMarkedTextRect` messages
  // over the text input channel.
  GdkRectangle composing_rect;
};

G_DEFINE_TYPE(FlTextInputPlugin, fl_text_input_plugin, G_TYPE_OBJECT)

// Completes method call and returns TRUE if the call was successful.
static gboolean finish_method(GObject* object,
                              GAsyncResult* result,
                              GError** error) {
  g_autoptr(FlMethodResponse) response = fl_method_channel_invoke_method_finish(
      FL_METHOD_CHANNEL(object), result, error);
  if (response == nullptr) {
    return FALSE;
  }
  return fl_method_response_get_result(response, error) != nullptr;
}

// Called when a response is received from TextInputClient.updateEditingState()
static void update_editing_state_response_cb(GObject* object,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  g_autoptr(GError) error = nullptr;
  if (!finish_method(object, result, &error)) {
    g_warning("Failed to call %s: %s", kUpdateEditingStateMethod,
              error->message);
  }
}

// Informs Flutter of text input changes.
static void update_editing_state(FlTextInputPlugin* self) {
  g_autoptr(FlValue) args = fl_value_new_list();
  fl_value_append_take(args, fl_value_new_int(self->client_id));
  g_autoptr(FlValue) value = fl_value_new_map();

  TextRange selection = self->text_model->selection();
  fl_value_set_string_take(
      value, kTextKey,
      fl_value_new_string(self->text_model->GetText().c_str()));
  fl_value_set_string_take(value, kSelectionBaseKey,
                           fl_value_new_int(selection.base()));
  fl_value_set_string_take(value, kSelectionExtentKey,
                           fl_value_new_int(selection.extent()));

  int composing_base = self->text_model->composing()
                           ? self->text_model->composing_range().base()
                           : -1;
  int composing_extent = self->text_model->composing()
                             ? self->text_model->composing_range().extent()
                             : -1;
  fl_value_set_string_take(value, kComposingBaseKey,
                           fl_value_new_int(composing_base));
  fl_value_set_string_take(value, kComposingExtentKey,
                           fl_value_new_int(composing_extent));

  // The following keys are not implemented and set to default values.
  fl_value_set_string_take(value, kSelectionAffinityKey,
                           fl_value_new_string(kTextAffinityDownstream));
  fl_value_set_string_take(value, kSelectionIsDirectionalKey,
                           fl_value_new_bool(FALSE));

  fl_value_append(args, value);

  fl_method_channel_invoke_method(self->channel, kUpdateEditingStateMethod,
                                  args, nullptr,
                                  update_editing_state_response_cb, self);
}

// Called when a response is received from TextInputClient.performAction()
static void perform_action_response_cb(GObject* object,
                                       GAsyncResult* result,
                                       gpointer user_data) {
  g_autoptr(GError) error = nullptr;
  if (!finish_method(object, result, &error)) {
    g_warning("Failed to call %s: %s", kPerformActionMethod, error->message);
  }
}

// Inform Flutter that the input has been activated.
static void perform_action(FlTextInputPlugin* self) {
  g_return_if_fail(FL_IS_TEXT_INPUT_PLUGIN(self));
  g_return_if_fail(self->client_id != 0);
  g_return_if_fail(self->input_action != nullptr);

  g_autoptr(FlValue) args = fl_value_new_list();
  fl_value_append_take(args, fl_value_new_int(self->client_id));
  fl_value_append_take(args, fl_value_new_string(self->input_action));

  fl_method_channel_invoke_method(self->channel, kPerformActionMethod, args,
                                  nullptr, perform_action_response_cb, self);
}

// Signal handler for GtkIMContext::preedit-start
static void im_preedit_start_cb(FlTextInputPlugin* self) {
  self->text_model->BeginComposing();

  // Set the top-level window used for system input method windows.
  GdkWindow* window =
      gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(self->view)));
  gtk_im_context_set_client_window(self->im_context, window);
}

// Signal handler for GtkIMContext::preedit-changed
static void im_preedit_changed_cb(FlTextInputPlugin* self) {
  g_autofree gchar* buf = nullptr;
  gint cursor_offset = 0;
  gtk_im_context_get_preedit_string(self->im_context, &buf, nullptr,
                                    &cursor_offset);
  cursor_offset += self->text_model->composing_range().base();
  self->text_model->UpdateComposingText(buf);
  self->text_model->SetSelection(TextRange(cursor_offset, cursor_offset));

  update_editing_state(self);
}

// Signal handler for GtkIMContext::commit
static void im_commit_cb(FlTextInputPlugin* self, const gchar* text) {
  self->text_model->AddText(text);
  if (self->text_model->composing()) {
    self->text_model->CommitComposing();
  }
  update_editing_state(self);
}

// Signal handler for GtkIMContext::preedit-end
static void im_preedit_end_cb(FlTextInputPlugin* self) {
  self->text_model->EndComposing();
  update_editing_state(self);
}

// Signal handler for GtkIMContext::retrieve-surrounding
static gboolean im_retrieve_surrounding_cb(FlTextInputPlugin* self) {
  auto text = self->text_model->GetText();
  size_t cursor_offset = self->text_model->GetCursorOffset();
  gtk_im_context_set_surrounding(self->im_context, text.c_str(), -1,
                                 cursor_offset);
  return TRUE;
}

// Signal handler for GtkIMContext::delete-surrounding
static gboolean im_delete_surrounding_cb(FlTextInputPlugin* self,
                                         gint offset,
                                         gint n_chars) {
  if (self->text_model->DeleteSurrounding(offset, n_chars)) {
    update_editing_state(self);
  }
  return TRUE;
}

// Called when the input method client is set up.
static FlMethodResponse* set_client(FlTextInputPlugin* self, FlValue* args) {
  if (fl_value_get_type(args) != FL_VALUE_TYPE_LIST ||
      fl_value_get_length(args) < 2) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        kBadArgumentsError, "Expected 2-element list", nullptr));
  }

  self->client_id = fl_value_get_int(fl_value_get_list_value(args, 0));
  FlValue* config_value = fl_value_get_list_value(args, 1);
  g_free(self->input_action);
  FlValue* input_action_value =
      fl_value_lookup_string(config_value, kInputActionKey);
  if (fl_value_get_type(input_action_value) == FL_VALUE_TYPE_STRING) {
    self->input_action = g_strdup(fl_value_get_string(input_action_value));
  }

  // Clear the multiline flag, then set it only if the field is multiline.
  self->input_multiline = FALSE;
  FlValue* input_type_value =
      fl_value_lookup_string(config_value, kTextInputTypeKey);
  if (fl_value_get_type(input_type_value) == FL_VALUE_TYPE_MAP) {
    FlValue* input_type_name =
        fl_value_lookup_string(input_type_value, kTextInputTypeNameKey);
    if (fl_value_get_type(input_type_name) == FL_VALUE_TYPE_STRING &&
        g_strcmp0(fl_value_get_string(input_type_name), kMultilineInputType) ==
            0) {
      self->input_multiline = TRUE;
    }
  }

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Shows the input method.
static FlMethodResponse* show(FlTextInputPlugin* self) {
  gtk_im_context_focus_in(self->im_context);

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Updates the editing state from Flutter.
static FlMethodResponse* set_editing_state(FlTextInputPlugin* self,
                                           FlValue* args) {
  const gchar* text =
      fl_value_get_string(fl_value_lookup_string(args, kTextKey));
  self->text_model->SetText(text);

  int64_t selection_base =
      fl_value_get_int(fl_value_lookup_string(args, kSelectionBaseKey));
  int64_t selection_extent =
      fl_value_get_int(fl_value_lookup_string(args, kSelectionExtentKey));
  // Flutter uses -1/-1 for invalid; translate that to 0/0 for the model.
  if (selection_base == -1 && selection_extent == -1) {
    selection_base = selection_extent = 0;
  }

  self->text_model->SetText(text);
  self->text_model->SetSelection(TextRange(selection_base, selection_extent));

  int64_t composing_base =
      fl_value_get_int(fl_value_lookup_string(args, kComposingBaseKey));
  int64_t composing_extent =
      fl_value_get_int(fl_value_lookup_string(args, kComposingExtentKey));
  if (composing_base == -1 && composing_extent == -1) {
    self->text_model->EndComposing();
  } else {
    size_t composing_start = std::min(composing_base, composing_extent);
    size_t cursor_offset = selection_base - composing_start;
    self->text_model->SetComposingRange(
        TextRange(composing_base, composing_extent), cursor_offset);
  }

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Called when the input method client is complete.
static FlMethodResponse* clear_client(FlTextInputPlugin* self) {
  self->client_id = kClientIdUnset;

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Hides the input method.
static FlMethodResponse* hide(FlTextInputPlugin* self) {
  gtk_im_context_focus_out(self->im_context);

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Update the IM cursor position.
//
// As text is input by the user, the framework sends two streams of updates
// over the text input channel: updates to the composing rect (cursor rect when
// not in IME composing mode) and updates to the matrix transform from local
// coordinates to Flutter root coordinates. This function is called after each
// of these updates. It transforms the composing rect to GTK window coordinates
// and notifies GTK of the updated cursor position.
static void update_im_cursor_position(FlTextInputPlugin* self) {
  // Skip update if not composing to avoid setting to position 0.
  if (!self->text_model->composing()) {
    return;
  }

  // Transform the x, y positions of the cursor from local coordinates to
  // Flutter view coordinates.
  gint x = self->composing_rect.x * self->editabletext_transform[0][0] +
           self->composing_rect.y * self->editabletext_transform[1][0] +
           self->editabletext_transform[3][0] + self->composing_rect.width;
  gint y = self->composing_rect.x * self->editabletext_transform[0][1] +
           self->composing_rect.y * self->editabletext_transform[1][1] +
           self->editabletext_transform[3][1] + self->composing_rect.height;

  // Transform from Flutter view coordinates to GTK window coordinates.
  GdkRectangle preedit_rect;
  gtk_widget_translate_coordinates(
      GTK_WIDGET(self->view), gtk_widget_get_toplevel(GTK_WIDGET(self->view)),
      x, y, &preedit_rect.x, &preedit_rect.y);

  // Set the cursor location in window coordinates so that GTK can position any
  // system input method windows.
  gtk_im_context_set_cursor_location(self->im_context, &preedit_rect);
}

// Handles updates to the EditableText size and position from the framework.
//
// On changes to the size or position of the RenderObject underlying the
// EditableText, this update may be triggered. It provides an updated size and
// transform from the local coordinate system of the EditableText to root
// Flutter coordinate system.
static FlMethodResponse* set_editable_size_and_transform(
    FlTextInputPlugin* self,
    FlValue* args) {
  FlValue* transform = fl_value_lookup_string(args, kTransform);
  size_t transform_len = fl_value_get_length(transform);
  g_warn_if_fail(transform_len == 16);

  for (size_t i = 0; i < transform_len; ++i) {
    double val = fl_value_get_float(fl_value_get_list_value(transform, i));
    self->editabletext_transform[i / 4][i % 4] = val;
  }
  update_im_cursor_position(self);

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Handles updates to the composing rect from the framework.
//
// On changes to the state of the EditableText in the framework, this update
// may be triggered. It provides an updated rect for the composing region in
// local coordinates of the EditableText. In the case where there is no
// composing region, the cursor rect is sent.
static FlMethodResponse* set_marked_text_rect(FlTextInputPlugin* self,
                                              FlValue* args) {
  self->composing_rect.x =
      fl_value_get_float(fl_value_lookup_string(args, "x"));
  self->composing_rect.y =
      fl_value_get_float(fl_value_lookup_string(args, "y"));
  self->composing_rect.width =
      fl_value_get_float(fl_value_lookup_string(args, "width"));
  self->composing_rect.height =
      fl_value_get_float(fl_value_lookup_string(args, "height"));
  update_im_cursor_position(self);

  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

// Called when a method call is received from Flutter.
static void method_call_cb(FlMethodChannel* channel,
                           FlMethodCall* method_call,
                           gpointer user_data) {
  FlTextInputPlugin* self = FL_TEXT_INPUT_PLUGIN(user_data);

  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  g_autoptr(FlMethodResponse) response = nullptr;
  if (strcmp(method, kSetClientMethod) == 0) {
    response = set_client(self, args);
  } else if (strcmp(method, kShowMethod) == 0) {
    response = show(self);
  } else if (strcmp(method, kSetEditingStateMethod) == 0) {
    response = set_editing_state(self, args);
  } else if (strcmp(method, kClearClientMethod) == 0) {
    response = clear_client(self);
  } else if (strcmp(method, kHideMethod) == 0) {
    response = hide(self);
  } else if (strcmp(method, kSetEditableSizeAndTransform) == 0) {
    response = set_editable_size_and_transform(self, args);
  } else if (strcmp(method, kSetMarkedTextRect) == 0) {
    response = set_marked_text_rect(self, args);
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  g_autoptr(GError) error = nullptr;
  if (!fl_method_call_respond(method_call, response, &error)) {
    g_warning("Failed to send method call response: %s", error->message);
  }
}

static void view_weak_notify_cb(gpointer user_data, GObject* object) {
  FlTextInputPlugin* self = FL_TEXT_INPUT_PLUGIN(object);
  self->view = nullptr;
}

static void fl_text_input_plugin_dispose(GObject* object) {
  FlTextInputPlugin* self = FL_TEXT_INPUT_PLUGIN(object);

  g_clear_object(&self->channel);
  g_clear_pointer(&self->input_action, g_free);
  g_clear_object(&self->im_context);
  if (self->text_model != nullptr) {
    delete self->text_model;
    self->text_model = nullptr;
  }

  G_OBJECT_CLASS(fl_text_input_plugin_parent_class)->dispose(object);
}

static void fl_text_input_plugin_class_init(FlTextInputPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_text_input_plugin_dispose;
}

static void fl_text_input_plugin_init(FlTextInputPlugin* self) {
  self->client_id = kClientIdUnset;
  self->im_context = gtk_im_multicontext_new();
  self->input_multiline = FALSE;
  g_signal_connect_object(self->im_context, "preedit-start",
                          G_CALLBACK(im_preedit_start_cb), self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object(self->im_context, "preedit-end",
                          G_CALLBACK(im_preedit_end_cb), self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object(self->im_context, "preedit-changed",
                          G_CALLBACK(im_preedit_changed_cb), self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object(self->im_context, "commit", G_CALLBACK(im_commit_cb),
                          self, G_CONNECT_SWAPPED);
  g_signal_connect_object(self->im_context, "retrieve-surrounding",
                          G_CALLBACK(im_retrieve_surrounding_cb), self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object(self->im_context, "delete-surrounding",
                          G_CALLBACK(im_delete_surrounding_cb), self,
                          G_CONNECT_SWAPPED);
  self->text_model = new flutter::TextInputModel();
}

FlTextInputPlugin* fl_text_input_plugin_new(FlBinaryMessenger* messenger,
                                            FlView* view) {
  g_return_val_if_fail(FL_IS_BINARY_MESSENGER(messenger), nullptr);

  FlTextInputPlugin* self = FL_TEXT_INPUT_PLUGIN(
      g_object_new(fl_text_input_plugin_get_type(), nullptr));

  g_autoptr(FlJsonMethodCodec) codec = fl_json_method_codec_new();
  self->channel =
      fl_method_channel_new(messenger, kChannelName, FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(self->channel, method_call_cb, self,
                                            nullptr);
  self->view = view;
  g_object_weak_ref(G_OBJECT(view), view_weak_notify_cb, self);

  return self;
}

gboolean fl_text_input_plugin_filter_keypress(FlTextInputPlugin* self,
                                              GdkEventKey* event) {
  g_return_val_if_fail(FL_IS_TEXT_INPUT_PLUGIN(self), FALSE);

  if (self->client_id == kClientIdUnset) {
    return FALSE;
  }

  if (gtk_im_context_filter_keypress(self->im_context, event)) {
    return TRUE;
  }

  // Handle the enter/return key.
  gboolean do_action = FALSE;
  // Handle navigation keys.
  gboolean changed = FALSE;
  if (event->type == GDK_KEY_PRESS) {
    switch (event->keyval) {
      case GDK_KEY_BackSpace:
        changed = self->text_model->Backspace();
        break;
      case GDK_KEY_Delete:
      case GDK_KEY_KP_Delete:
        // Already handled inside Flutter.
        break;
      case GDK_KEY_End:
      case GDK_KEY_KP_End:
        changed = self->text_model->MoveCursorToEnd();
        break;
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter:
      case GDK_KEY_ISO_Enter:
        if (self->input_multiline == TRUE) {
          self->text_model->AddCodePoint('\n');
          changed = TRUE;
        }
        do_action = TRUE;
        break;
      case GDK_KEY_Home:
      case GDK_KEY_KP_Home:
        changed = self->text_model->MoveCursorToBeginning();
        break;
      case GDK_KEY_Left:
      case GDK_KEY_KP_Left:
        // Already handled inside Flutter.
        break;
      case GDK_KEY_Right:
      case GDK_KEY_KP_Right:
        // Already handled inside Flutter.
        break;
    }
  }

  if (changed) {
    update_editing_state(self);
  }
  if (do_action) {
    perform_action(self);
  }

  return FALSE;
}
