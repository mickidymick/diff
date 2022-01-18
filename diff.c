#include <yed/plugin.h>

typedef struct diff_buff_t {
    char             *buff_name;
    yed_buffer_ptr_t  buff;
}diff_buff;

typedef char                                *buff_name_ptr;
typedef diff_buff                           *diff_buff_ptr;
use_tree_c(buff_name_ptr, diff_buff_ptr,     strcmp);
static tree(buff_name_ptr, diff_buff_ptr)    d_buffers;
static tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it;
static int                                   len;

static void unload(yed_plugin *self);
static void get_or_make_buffers(void);
static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff_ptr d_buff);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static void tree_empty(void);
static void free_diff_buff(diff_buff_ptr buff);
static void d_buff_post_load(yed_event *event);
       void diff(int n_args, char **args);

int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();

    yed_plugin_set_unload_fn(self, unload);
    d_buffers = tree_make(buff_name_ptr, diff_buff_ptr);

    yed_event_handler buff_eh;
    buff_eh.kind = EVENT_BUFFER_POST_LOAD;
    buff_eh.fn   = d_buff_post_load;
    yed_plugin_add_event_handler(self, buff_eh);

    len = 0;
    get_or_make_buffers();

    yed_plugin_set_command(self, "diff", diff);
    yed_plugin_set_completion(self, "diff-compl-arg-0", diff_completion);
    yed_plugin_set_completion(self, "diff-compl-arg-1", diff_completion);

    return 0;
}

int diff_completion(char *name, struct yed_completion_results_t *comp_res) {
    int ret = 0;
    array_t list;
    list = array_make(char *);
    char *tmp;
    char loc[256];

    tree_traverse(d_buffers, d_buffers_it) {
        array_push(list, tree_it_val(d_buffers_it)->buff_name);
    }

    FN_BODY_FOR_COMPLETE_FROM_ARRAY(name, array_len(list), (char **)array_data(list), comp_res, ret);
    array_free(list);
    return ret;
}

void diff(int n_args, char **args) {
    char *buff_1;
    char *buff_2;
    char  tmp_buff_1[512];
    char  tmp_buff_2[512];
    tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it_1;
    tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it_2;

    if (n_args != 2) {
        yed_cerr("expected 2 arguments, two buffer names");
        return;
    }

    buff_1 = args[0];
    buff_2 = args[1];

    snprintf(tmp_buff_1, 512, "%s-diff", buff_1);
    snprintf(tmp_buff_2, 512, "%s-diff", buff_2);
    d_buffers_it_1 = tree_lookup(d_buffers, tmp_buff_1);
    d_buffers_it_2 = tree_lookup(d_buffers, tmp_buff_2);

    if (tree_it_good(d_buffers_it_1) && tree_it_good(d_buffers_it_2)) {
        snprintf(tmp_buff_1, 512, "*%s-diff", buff_1);
        snprintf(tmp_buff_2, 512, "*%s-diff", buff_2);
        YEXE("frame-new");
        YEXE("buffer", tmp_buff_1);
        YEXE("frame-vsplit");
        YEXE("buffer", tmp_buff_2);
    }
}

static void d_buff_post_load(yed_event *event) {
    get_or_make_buffers();
}

static void unload(yed_plugin *self) {
    yed_buffer *buff;
    char        tmp_buff[512];
    char       *old_key;

    tree_traverse(d_buffers, d_buffers_it) {
        snprintf(tmp_buff, 512, "*%s", tree_it_key(d_buffers_it));
        old_key = tree_it_key(d_buffers_it);
        free_diff_buff(tree_it_val(d_buffers_it));
        buff = yed_get_buffer(tmp_buff);
        if (buff != NULL) {
            yed_free_buffer(buff);
        }
        tree_delete(d_buffers, old_key);
        free(old_key);
    }
}

static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff_ptr d_buff) {
    yed_line *line;
    int       row;

    yed_buff_clear_no_undo(buff);

    row = 1;
    bucket_array_traverse(d_buff->buff->lines, line) {
        yed_buffer_add_line_no_undo(buff);
        yed_buff_set_line_no_undo(buff, row, line);
        row += 1;
    }
}

static void free_diff_buff(diff_buff_ptr buff) {
        free(tree_it_val(d_buffers_it)->buff_name);

}

static void tree_empty(void) {
    char *old_key;

    while(tree_len(d_buffers) > 0) {
        d_buffers_it = tree_last(d_buffers);
        if (tree_it_good(d_buffers_it)) {
            old_key = tree_it_key(d_buffers_it);
            free_diff_buff(tree_it_val(d_buffers_it));
            tree_delete(d_buffers, old_key);
            free(old_key);
        }
    }
}

static void get_or_make_buffers(void) {
    char                                         tmp_buff[512];
    char                                         buff[512];
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;
    diff_buff_ptr                                d_buff_ptr;
    yed_buffer                                  *buffer;

    tree_traverse(ys->buffers, buffers_it) {
        if (tree_it_val(buffers_it)->kind == BUFF_KIND_FILE &&
            tree_it_val(buffers_it)->flags != BUFF_SPECIAL) {
            snprintf(buff, 512, "%s-diff", tree_it_key(buffers_it));
            d_buff_ptr = (diff_buff*) malloc(sizeof(diff_buff));
            d_buff_ptr->buff_name = strdup(tree_it_key(buffers_it));
            d_buff_ptr->buff = tree_it_val(buffers_it);
            tree_insert(d_buffers, strdup(buff), d_buff_ptr);
        }
    }

    tree_traverse(d_buffers, d_buffers_it) {
        snprintf(tmp_buff, 512, "*%s", tree_it_key(d_buffers_it));
        buffer = yed_get_buffer(tmp_buff);

        if (buffer == NULL) {
            buffer = yed_create_buffer(tmp_buff);
            buffer->flags |= BUFF_SPECIAL;
        }

        update_diff_buffer(buffer, tree_it_val(d_buffers_it));
    }
}
