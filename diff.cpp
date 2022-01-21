extern "C" {
    #include <yed/plugin.h>
}


#include "dmp_diff.hpp"
#include <string>
#include <vector>

using namespace std;

#define LEFT  (0)
#define RIGHT (1)

typedef struct buffer_line_t {
    int old_row;         //row that this line is from in the old buffer
    int collapsed_start; //start of the collapsed row in the old buffer
    int collapsed_end;   //end of the collapsed row in the old buffer
    int row_type;        //type of this row
    int start_col;       //start of the change
    int end_col;         //end of the change
}buffer_line;

typedef struct diff_buff_t {
    char             *buff_name;    //original buffers name
    int               buff_num;     //use macro
    yed_buffer_ptr_t  buff;         //original buffers ptr
    vector<buffer_line> lines; //all the lines in the new buffer
}diff_buff;

typedef char                                *buff_name_ptr;
typedef diff_buff                           *diff_buff_ptr;

extern "C" {
    use_tree_c(buff_name_ptr, diff_buff_ptr,     strcmp);
}
static tree(buff_name_ptr, diff_buff_ptr)    d_buffers;
static tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it; //keyed on new buffers name
static int                                   len;

static void unload(yed_plugin *self);
static void get_or_make_buffers(char *buff_1, char *buff_2);
static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff_ptr d_buff);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static void tree_empty(void);
static void free_diff_buff(diff_buff_ptr buff);
static void myers_diff(void);
static void line_draw(yed_event *event);
static void row_draw(yed_event *event);
       void diff(int n_args, char **args);

extern "C" {
    int yed_plugin_boot(yed_plugin *self) {
        YED_PLUG_VERSION_CHECK();

        d_buffers = tree_make(buff_name_ptr, diff_buff_ptr);
        len       = 0;

    /*     ROW_PRE_CLEAR */
        yed_event_handler row_draw_eh;
        row_draw_eh.kind = EVENT_ROW_PRE_CLEAR;
        row_draw_eh.fn   = row_draw;
/*         yed_plugin_add_event_handler(self, row_draw_eh); */

    /*     LINE_PRE_DRAW */
        yed_event_handler line_draw_eh;
        line_draw_eh.kind = EVENT_LINE_PRE_DRAW;
        line_draw_eh.fn   = line_draw;
/*         yed_plugin_add_event_handler(self, line_draw_eh); */

        yed_plugin_set_unload_fn(self, unload);
        yed_plugin_set_command(self, "diff", diff);
        yed_plugin_set_completion(self, "diff-compl-arg-0", diff_completion);
        yed_plugin_set_completion(self, "diff-compl-arg-1", diff_completion);

        return 0;
    }
}

void diff(int n_args, char **args) {
    char                                 *buff_1;
    char                                 *buff_2;
    char                                  tmp_buff_1[512];
    char                                  tmp_buff_2[512];
    tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it_1;
    tree_it(buff_name_ptr, diff_buff_ptr) d_buffers_it_2;

    if (n_args != 2) {
        yed_cerr("expected 2 arguments, two buffer names");
        return;
    }

    buff_1 = args[0];
    buff_2 = args[1];
    get_or_make_buffers(buff_1, buff_2);

    snprintf(tmp_buff_1, 512, "diff:%s", buff_1);
    snprintf(tmp_buff_2, 512, "diff:%s", buff_2);
    d_buffers_it_1 = tree_lookup(d_buffers, tmp_buff_1);
    d_buffers_it_2 = tree_lookup(d_buffers, tmp_buff_2);

    if (tree_it_good(d_buffers_it_1) && tree_it_good(d_buffers_it_2)) {
        snprintf(tmp_buff_1, 512, "*diff:%s", buff_1);
        snprintf(tmp_buff_2, 512, "*diff:%s", buff_2);
        YEXE("frame-new");
        YEXE("buffer", tmp_buff_1);
        YEXE("frame-vsplit");
        YEXE("buffer", tmp_buff_2);
    }

    myers_diff();
}

static void line_draw(yed_event *event) {
/*     yed_attrs       *attr; */
/*     yed_attrs        new_attr; */
/*     int              loc; */
/*     yed_buffer_ptr_t buff_1; */
/*     yed_buffer_ptr_t buff_2; */
/*  */
/*     diff_buff_ptr    diff_buff_1; */
/*     diff_buff_ptr    diff_buff_2; */
/*  */
/*     buff_1 = NULL; */
/*     buff_2 = NULL; */
/*  */
/*     tree_traverse(d_buffers, d_buffers_it) { */
/*         if (tree_it_val(d_buffers_it)->buff_num == LEFT) { */
/*             diff_buff_1 = tree_it_val(d_buffers_it); */
/*             buff_1 = diff_buff_1->buff; */
/*         } */
/*  */
/*         if (tree_it_val(d_buffers_it)->buff_num == RIGHT) { */
/*             diff_buff_2 = tree_it_val(d_buffers_it); */
/*             buff_2 = diff_buff_2->buff; */
/*         } */
/*     } */
/*  */
/*     if (buff_1 == NULL) { */
/*         yed_cerr("Couldn't find the first diff buffer."); */
/*         return; */
/*     } else if (buff_2 == NULL) { */
/*         yed_cerr("Couldn't find the second diff buffer."); */
/*         return; */
/*     } */
/*  */
/*     if (event->frame->buffer == diff_buff_2->buff) { */
/*         if (diff_buff_2->lines[event->row].start_col != -1) { */
/*             loc = 0; */
/*             new_attr = yed_active_style_get_bad(); */
/*             array_traverse(event->line_attrs, attr) { */
/*                 if (loc >= diff_buff_2->lines[event->row].start_col && */
/*                     loc <= diff_buff_2->lines[event->row].end_col) { */
/*                         yed_combine_attrs(attr, &new_attr); */
/*                     } */
/*                 loc++; */
/*             } */
/*         } */
/*     } */
}

static void row_draw(yed_event *event) {

}

static void myers_diff(void) {
    string A;
    string B;

    yed_buffer_ptr_t buff_1;
    yed_buffer_ptr_t buff_2;

    diff_buff_ptr diff_buff_1;
    diff_buff_ptr diff_buff_2;

    diff_buff_1 = NULL;
    diff_buff_2 = NULL;
    buff_1      = NULL;
    buff_2      = NULL;

    tree_traverse(d_buffers, d_buffers_it) {
        if (tree_it_val(d_buffers_it)->buff_num == LEFT) {
            diff_buff_1 = tree_it_val(d_buffers_it);
            buff_1 = diff_buff_1->buff;
        }

        if (tree_it_val(d_buffers_it)->buff_num == RIGHT) {
            diff_buff_2 = tree_it_val(d_buffers_it);
            buff_2 = diff_buff_2->buff;
        }
    }

    if (buff_1 == NULL || diff_buff_1 == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return;
    } else if (buff_2 == NULL || diff_buff_2 == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return;
    }

    yed_line *line;
    vector<int> Va;
    vector<int> Vb;
    Va.push_back(0);
    Vb.push_back(0);

    bucket_array_traverse(buff_1->lines, line) {
        array_zero_term(line->chars);
        A.append((const char *)(line->chars.data));
        A.append("\n");
        Va.push_back(array_len(line->chars) + 1);
    }

    bucket_array_traverse(buff_2->lines, line) {
        array_zero_term(line->chars);
        B.append((const char *)(line->chars.data));
        B.append("\n");
        Vb.push_back(array_len(line->chars) + 1);
    }

    MyersDiff<string> diffs{A, B};

    int line_first = -1;
    int line_last = -1;

    int b_begin   = 0;
    int b_end     = 0;
    int b_last    = 0;
    int b_loc     = 0;
    int b_loc_e   = 0;

    for (int i=0; i<diffs.diffs().size(); i++) {
        if (diffs.diffs()[i].operation == INSERT) {
            b_begin = diffs.diffs()[i].text.from - B.begin();
            b_end   = diffs.diffs()[i].text.till - B.begin();

            while (b_end > b_last) {
                b_loc++;
                b_last += Vb[b_loc];
            }
            if (line_first == -1) {
                line_first = b_begin - Vb[b_loc - 1] + 1;
            }
            if (b_end - Vb[b_loc - 1] + 1 > line_last) {
                line_last = b_end - Vb[b_loc - 1];
            }
            yed_cprint("%s  %dB row:%d col:%d\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - B.begin(), b_loc, b_begin - Vb[b_loc - 1] + 1);
        }else if (diffs.diffs()[i].operation == DELETE) {
            yed_cprint("%s  %dA\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - A.begin());
        }else {
            yed_cprint("%s  %dA\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - A.begin());

            if (line_first > -1) {

                auto s_it = diffs.diffs()[i].text.from;
                b_loc_e = b_loc;
                for (; s_it<diffs.diffs()[i].text.till; s_it++) {
                    if (*s_it == '\n') {
/*                         diff_buff_2->lines.emplace_back(); */
/*                         diff_buff_2->lines.back().old_row         = b_loc_e; */
/*                         diff_buff_2->lines.back().collapsed_start = 0; */
/*                         diff_buff_2->lines.back().collapsed_end   = 0; */
/*                         diff_buff_2->lines.back().row_type        = 0; */
/*                         diff_buff_2->lines.back().start_col       = line_first; */
/*                         diff_buff_2->lines.back().end_col         = line_last; */
/*  */
                        line_first = -1;
                        line_last  = -1;
/*                         b_loc_e++; */
                        break;
                    }
                }
            }
        }

    }

}

int diff_completion(char *name, struct yed_completion_results_t *comp_res) {
    int                                          ret;
    array_t                                      list;
    char                                        *tmp;
    char                                         loc[256];
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;

    ret = 0;
    list = array_make(char *);

    tree_traverse(ys->buffers, buffers_it) {
        if (tree_it_val(buffers_it)->kind == BUFF_KIND_FILE &&
            tree_it_val(buffers_it)->flags != BUFF_SPECIAL) {

            array_push(list, tree_it_key(buffers_it));
        }
    }

    FN_BODY_FOR_COMPLETE_FROM_ARRAY(name, array_len(list), (char **)array_data(list), comp_res, ret);
    array_free(list);
    return ret;
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

static void get_or_make_buffers(char *buff_1, char *buff_2) {
    char                                         tmp_buff[512];
    char                                         buff[512];
    char                                        *check_buff;
    diff_buff_ptr                                d_buff_ptr;
    yed_buffer                                  *buffer;
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;

    tree_traverse(ys->buffers, buffers_it) {
        if (tree_it_val(buffers_it)->kind == BUFF_KIND_FILE &&
            tree_it_val(buffers_it)->flags != BUFF_SPECIAL) {

            check_buff = tree_it_key(buffers_it);
            if (strcmp(buff_1, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buff_ptr = (diff_buff*) malloc(sizeof(diff_buff));
                d_buff_ptr->buff_name = strdup(check_buff);
                d_buff_ptr->buff = tree_it_val(buffers_it);
                d_buff_ptr->buff_num = LEFT;
                d_buff_ptr->lines = vector<buffer_line>(0);
                tree_insert(d_buffers, strdup(buff), d_buff_ptr);
                yed_cprint("made 1");
            }else if (strcmp(buff_2, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buff_ptr = (diff_buff*) malloc(sizeof(diff_buff));
                d_buff_ptr->buff_name = strdup(check_buff);
                d_buff_ptr->buff = tree_it_val(buffers_it);
                d_buff_ptr->buff_num = RIGHT;
                d_buff_ptr->lines = vector<buffer_line>(0);
                tree_insert(d_buffers, strdup(buff), d_buff_ptr);
                yed_cprint("made 2");
            }
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
