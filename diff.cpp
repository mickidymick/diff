extern "C" {
    #include <yed/plugin.h>
}

#define DO_LOG
#define DBG__XSTR(x) #x
#define DBG_XSTR(x) DBG__XSTR(x)
#ifdef DO_LOG
#define DBG(...)                                           \
do {                                                       \
    LOG_FN_ENTER();                                        \
    yed_log(__FILE__ ":" XSTR(__LINE__) ": " __VA_ARGS__); \
    LOG_EXIT();                                            \
} while (0)
#else
#define DBG(...) ;
#endif

#include <string>
#include <vector>
#include <map>

using namespace std;

#define LEFT  (0)
#define RIGHT (1)

#define INIT  (0)
#define INS   (1)
#define DEL   (2)
#define EQL   (3)
#define CHG   (4)
#define NUL   (5)

class file_diff {
    public:
        int    type[2];
        int    row_num[2];
        string line_s[2];

        file_diff(int t_1, int row_1, string line_s_1, int t_2, int row_2, string line_s_2) {
            type[0]    = t_1;
            type[1]    = t_2;
            row_num[0] = row_1;
            row_num[1] = row_2;
            line_s[0]  = line_s_1;
            line_s[1]  = line_s_2;
        }
};

typedef struct line_t {
    int         line_type;
    int         col_begin[2];
    int         col_end[2];
    vector<int> char_type;
} line;

typedef struct diff_main_t {
    yed_buffer_ptr_t  buff_orig[2];
    yed_buffer_ptr_t  buff_diff[2];
    vector<line>      lines;
    vector<string>    line_buff[2];
    vector<file_diff> f_diff;
} diff_main;

diff_main diff_m;

#include "myers_diff.hpp"

//Base Yed Plugin Functions
static int  get_or_make_buffers(char *buff_1, char *buff_2);
static void unload(yed_plugin *self);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static  int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res);
static void clear_buffers(char* b1, char* b2, char* b3, char* b4);

//Diff Functions
static void diff(int n_args, char **args);
static int  init(void);
static void align_buffers(Myers<vector<string>> myers, yed_buffer_ptr_t buff_1, yed_buffer_ptr_t buff_2);
static line diff_adjacent_line(line tmp_line, int loc);
static void update_diff_buffer(yed_buffer_ptr_t buff_orig, yed_buffer_ptr_t buff_diff);
static void line_base_draw(yed_event *event);
static void line_char_draw(yed_event *event);
static void cursor_move(yed_event *event);

extern "C" {
    int yed_plugin_boot(yed_plugin *self) {
        YED_PLUG_VERSION_CHECK();

    /*     ROW_PRE_CLEAR */
        yed_event_handler line_base_draw_eh;
        line_base_draw_eh.kind = EVENT_ROW_PRE_CLEAR;
        line_base_draw_eh.fn   = line_base_draw;
        yed_plugin_add_event_handler(self, line_base_draw_eh);

    /*     LINE_PRE_DRAW */
        yed_event_handler line_char_draw_eh;
        line_char_draw_eh.kind = EVENT_LINE_PRE_DRAW;
        line_char_draw_eh.fn   = line_char_draw;
        yed_plugin_add_event_handler(self, line_char_draw_eh);

    /*     CURSOR_PRE_MOVE */
        yed_event_handler cursor_move_eh;
        cursor_move_eh.kind = EVENT_CURSOR_PRE_MOVE;
        cursor_move_eh.fn   = cursor_move;
        yed_plugin_add_event_handler(self, cursor_move_eh);

        yed_plugin_set_unload_fn(self, unload);
        yed_plugin_set_command(self, "diff", diff);
        yed_plugin_set_completion(self, "diff-compl", diff_completion);
        yed_plugin_set_completion(self, "diff-compl-arg-0", diff_completion_multiple);
        yed_plugin_set_completion(self, "diff-compl-arg-1", diff_completion_multiple);

        return 0;
    }
}

static int get_or_make_buffers(char *buff_1, char *buff_2) {
    yed_buffer *buffer;
    char        tmp_buff[512];

    buffer = yed_get_buffer(buff_1);
    if (buffer == NULL) {
        YEXE("buffer-hidden", buff_1);
        buffer = yed_get_buffer(buff_1);
        if(buffer == NULL) {
            return 1;
        }
    }
    diff_m.buff_orig[LEFT] = buffer;

    buffer = yed_get_buffer(buff_2);
    if (buffer == NULL) {
        YEXE("buffer-hidden", buff_2);
        buffer = yed_get_buffer(buff_2);
        if(buffer == NULL) {
            return 1;
        }
    }
    diff_m.buff_orig[RIGHT] = buffer;

    snprintf(tmp_buff, 512, "*diff:%s", buff_1);
    buffer = yed_get_buffer(tmp_buff);
    if (buffer == NULL) {
        buffer = yed_create_buffer(tmp_buff);
        buffer->flags |= BUFF_SPECIAL;
    }
    diff_m.buff_diff[LEFT] = buffer;
    update_diff_buffer(diff_m.buff_orig[LEFT], diff_m.buff_diff[LEFT]);

    snprintf(tmp_buff, 512, "*diff:%s", buff_2);
    buffer = yed_get_buffer(tmp_buff);
    if (buffer == NULL) {
        buffer = yed_create_buffer(tmp_buff);
        buffer->flags |= BUFF_SPECIAL;
    }
    diff_m.buff_diff[RIGHT] = buffer;
    update_diff_buffer(diff_m.buff_orig[RIGHT], diff_m.buff_diff[RIGHT]);

    return 0;
}

static void unload(yed_plugin *self) {
}

static int diff_completion(char *name, struct yed_completion_results_t *comp_res) {
    int                                          ret;
    array_t                                      list;
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


static int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res) {
    char *tmp[2] = {"file", "diff-compl"};
    return yed_complete_multiple(2, tmp, name, comp_res);
}

static void clear_buffers(char* b1, char* b2, char* b3, char* b4) {
    yed_buffer *buffer;

    buffer = yed_get_buffer(b1);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }

    buffer = yed_get_buffer(b2);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }

    buffer = yed_get_buffer(b3);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }

    buffer = yed_get_buffer(b4);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }
}

static void diff(int n_args, char **args) {
    char                                 *buff_1;
    char                                 *buff_2;
    char                                  tmp_buff_1[512];
    char                                  tmp_buff_2[512];

    if (n_args != 2) {
        yed_cerr("Expected 2 arguments, two buffer names");
        return;
    }

    buff_1 = args[0];
    buff_2 = args[1];

    if (strcmp(buff_1, buff_2) == 0) {
        yed_cerr("The two buffers must be different!");
        return;
    }

    snprintf(tmp_buff_1, 512, "*diff:%s", buff_1);
    snprintf(tmp_buff_2, 512, "*diff:%s", buff_2);

    clear_buffers(buff_1, buff_2, tmp_buff_1, tmp_buff_2);

    if (get_or_make_buffers(buff_1, buff_2)) {
        yed_cerr("Could not get buffers!");
        return;
    }

    if (init()) {
        yed_cerr("Could not initialize the diff!");
        return;
    }

    Myers<vector<string>> myers(diff_m.line_buff[LEFT], diff_m.line_buff[LEFT].size(),
                                diff_m.line_buff[RIGHT], diff_m.line_buff[RIGHT].size());
    diff_m.f_diff = myers.diff();

    if (diff_m.f_diff.size() == 0) {
        yed_cerr("Files were the same!");
        return;
    }

    YEXE("frame-new");
    YEXE("buffer", diff_m.buff_diff[LEFT]->name);
    YEXE("frame-vsplit");
    YEXE("buffer", diff_m.buff_diff[RIGHT]->name);

    align_buffers(myers, diff_m.buff_diff[LEFT], diff_m.buff_diff[RIGHT]);

    LOG_FN_ENTER();
    yed_log("%s, %s", buff_1, buff_2);
    LOG_EXIT();
}

static int init(void) {
    yed_line *line;
    string    A;
    string    B;

    if (diff_m.buff_orig[LEFT] == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return 1;
    }else if (diff_m.buff_orig[RIGHT] == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return 1;
    }

    diff_m.line_buff[LEFT].clear();
    diff_m.line_buff[RIGHT].clear();

    bucket_array_traverse(diff_m.buff_orig[LEFT]->lines, line) {
        array_zero_term(line->chars);
        A.clear();
        A.append((char *)(line->chars.data));
        A.append("\n");
        diff_m.line_buff[LEFT].push_back(A);
    }

    bucket_array_traverse(diff_m.buff_orig[RIGHT]->lines, line) {
        array_zero_term(line->chars);
        B.clear();
        B.append((char *)(line->chars.data));
        B.append("\n");
        diff_m.line_buff[RIGHT].push_back(B);
    }

    return 0;
}

static void align_buffers(Myers<vector<string>> myers, yed_buffer_ptr_t buff_1, yed_buffer_ptr_t buff_2) {
    line init_line;
    int  save_i;
    int  num_tmp_ins;
    int  num_tmp_del;
    int  left_added_row;
    int  right_added_row;

    init_line.line_type = INIT;

    diff_m.lines.clear();
    diff_m.lines.push_back(init_line);

    save_i          = 0;
    num_tmp_ins     = 0;
    num_tmp_del     = 0;
    left_added_row  = 0;
    right_added_row = 0;
    for (int i = 0; i < diff_m.f_diff.size(); i++) {

        if (diff_m.f_diff[i].type[LEFT] == INIT) {
            continue;
        }

        save_i = i;
        if (diff_m.f_diff[i].type[RIGHT] == INS) {
//             DBG("INS\n");
            num_tmp_ins++;
        } else if (diff_m.f_diff[i].type[LEFT] == DEL) {
//             DBG("DEL\n");
            num_tmp_del++;
        } else {
//             DBG("EQL\n");

            if (num_tmp_ins < num_tmp_del) {
                for (int j = num_tmp_ins; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    tmp_line           = diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[RIGHT] + right_added_row - j);
                    diff_m.lines.push_back(tmp_line);
//                     DBG("row:%d   1 CHG", diff_m.f_diff[i].row_num[RIGHT] + right_added_row - j);
                }

                for (int j = num_tmp_ins; j < num_tmp_del; j++) {
                    line tmp_line;
                    char dashes[512];
                    memset(dashes, '-', 512);
                    yed_buff_insert_line_no_undo(diff_m.buff_diff[RIGHT],
                        diff_m.f_diff[i].row_num[RIGHT] + right_added_row);
                    yed_buff_insert_string(diff_m.buff_diff[RIGHT],
                        dashes, diff_m.f_diff[i].row_num[RIGHT] + right_added_row, 1);
//                     DBG("row:%d   DEL", diff_m.f_diff[i].row_num[RIGHT] + right_added_row);
                    right_added_row++;
                    tmp_line.line_type = DEL;
                    diff_m.lines.push_back(tmp_line);
                }
            } else if (num_tmp_ins > num_tmp_del) {
                for (int j = num_tmp_del; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    tmp_line           = diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[LEFT] + left_added_row - j);
                    diff_m.lines.push_back(tmp_line);
//                     DBG("row:%d   2 CHG", diff_m.f_diff[i].row_num[LEFT] + left_added_row - j);
                }

                for (int j = num_tmp_del; j < num_tmp_ins; j++) {
                    line tmp_line;
                    char dashes[512];
                    memset(dashes, '-', 512);
                    yed_buff_insert_line_no_undo(diff_m.buff_diff[LEFT],
                        diff_m.f_diff[i].row_num[LEFT] + left_added_row);
                    yed_buff_insert_string(diff_m.buff_diff[LEFT],
                        dashes, diff_m.f_diff[i].row_num[LEFT] + left_added_row, 1);
//                     DBG("row:%d   INS", diff_m.f_diff[i].row_num[LEFT] + left_added_row);
                    left_added_row++;
                    tmp_line.line_type = INS;
                    diff_m.lines.push_back(tmp_line);
                }
            } else {
                for (int j = num_tmp_ins; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    tmp_line           = diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[LEFT] + left_added_row - j);
                    diff_m.lines.push_back(tmp_line);
//                     DBG("row:%d   3 CHG", diff_m.f_diff[i].row_num[LEFT] + left_added_row - j);
                }
            }

            line tmp_line;
            tmp_line.line_type = EQL;
            diff_m.lines.push_back(tmp_line);

            num_tmp_ins = 0;
            num_tmp_del = 0;
        }
    }

    if (num_tmp_ins < num_tmp_del) {
        for (int j = num_tmp_ins; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            tmp_line           = diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row - j);
            diff_m.lines.push_back(tmp_line);
//             DBG("row:%d   1 CHG", diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row - j);
        }

        for (int j = num_tmp_ins; j < num_tmp_del; j++) {
            line tmp_line;
            char dashes[512];
            memset(dashes, '-', 512);
            yed_buff_insert_line_no_undo(diff_m.buff_diff[RIGHT],
                diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row);
            yed_buff_insert_string(diff_m.buff_diff[RIGHT],
                dashes, diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row, 1);
//             DBG("row:%d   DEL", diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row);
            right_added_row++;
            tmp_line.line_type = DEL;
            diff_m.lines.push_back(tmp_line);
        }
    } else if (num_tmp_ins > num_tmp_del) {
        for (int j = num_tmp_del; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            tmp_line           = diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j);
            diff_m.lines.push_back(tmp_line);
//             DBG("row:%d   2 CHG", diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j);
        }

        for (int j = num_tmp_del; j < num_tmp_ins; j++) {
            line tmp_line;
            char dashes[512];
            memset(dashes, '-', 512);
            yed_buff_insert_line_no_undo(diff_m.buff_diff[LEFT],
                diff_m.f_diff[save_i].row_num[LEFT] + left_added_row);
            yed_buff_insert_string(diff_m.buff_diff[LEFT],
                dashes, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row, 1);
//             DBG("row:%d   INS", diff_m.f_diff[save_i].row_num[LEFT] + left_added_row);
            left_added_row++;
            tmp_line.line_type = INS;
            diff_m.lines.push_back(tmp_line);
        }
    } else {
        for (int j = num_tmp_ins; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            tmp_line = diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j);
            diff_m.lines.push_back(tmp_line);
//             DBG("row:%d   3 CHG", diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j);
        }
    }
}

static line diff_adjacent_line(line tmp_line, int loc) {
    char *left;
    char *right;
    int   left_len;
    int   right_len;
    int   last_col_ins;
    int   last_col_del;
    int   last_eql_left;
    int   last_eql_right;

    last_col_ins   = 0;
    last_col_del   = 0;
    last_eql_left  = 0;
    last_eql_right = 0;
    if (array_len(diff_m.buff_diff[LEFT]->lines) > loc && array_len(diff_m.buff_diff[RIGHT]->lines) > loc) {
//         DBG("row:%d", loc);
//         DBG("%s", yed_get_line_text(diff_m.buff_diff[LEFT], loc));
//         DBG("%s", yed_get_line_text(diff_m.buff_diff[RIGHT], loc));
        left  = yed_get_line_text(diff_m.buff_diff[LEFT], loc);
        left_len = array_len(yed_buff_get_line(diff_m.buff_diff[LEFT], loc)->chars);
        right = yed_get_line_text(diff_m.buff_diff[RIGHT], loc);
        right_len = array_len(yed_buff_get_line(diff_m.buff_diff[RIGHT], loc)->chars);
//         DBG("left size:%d", left_len);
//         DBG("right size:%d", right_len);
        Myers<string> myers_r(left, left_len, right, right_len);
        vector<file_diff> tmp_file_diff = myers_r.diff();
//         DBG("%d", tmp_file_diff.size());

        tmp_line.col_begin[LEFT]  = 0;
        tmp_line.col_begin[RIGHT] = 0;
        tmp_line.col_end[LEFT]    = 0;
        tmp_line.col_end[RIGHT]   = 0;

        for (int c = 0; c < tmp_file_diff.size(); c++) {
            if (tmp_file_diff[c].type[RIGHT] == INS) {
                tmp_line.char_type.push_back(INS);
                if (tmp_line.col_begin[RIGHT] == 0) {
                    tmp_line.col_begin[RIGHT] = tmp_file_diff[c].row_num[RIGHT];
                }
                last_col_ins = tmp_file_diff[c].row_num[RIGHT];
                if (last_eql_left > last_col_del) {
                    last_col_del = last_eql_left;
                }
//                 DBG("col:%d INS", tmp_file_diff[c].row_num[RIGHT]);
            } else if (tmp_file_diff[c].type[LEFT] == DEL) {
                tmp_line.char_type.push_back(DEL);
                if (tmp_line.col_begin[LEFT] == 0) {
                    tmp_line.col_begin[LEFT] = tmp_file_diff[c].row_num[LEFT];
                }
                last_col_del = tmp_file_diff[c].row_num[LEFT];
                if (last_eql_right > last_col_ins) {
                    last_col_ins = last_eql_right;
                }
//                 DBG("col:%d DEL", tmp_file_diff[c].row_num[LEFT]);
            } else {
                if (tmp_line.col_begin[LEFT] != 0
                && tmp_line.col_begin[RIGHT] == 0) {
                    tmp_line.col_begin[RIGHT] = tmp_file_diff[c].row_num[RIGHT];
                } else if (tmp_line.col_begin[RIGHT] != 0
                && tmp_line.col_begin[LEFT] == 0) {
                    tmp_line.col_begin[LEFT] = tmp_file_diff[c].row_num[LEFT];
                }

                last_eql_left  = tmp_file_diff[c].row_num[LEFT];
                last_eql_right = tmp_file_diff[c].row_num[RIGHT];

                tmp_line.char_type.push_back(EQL);
//                 DBG("col:%d EQL", tmp_file_diff[c].row_num[LEFT]);
            }
        }
        tmp_line.col_end[LEFT]  = last_col_del;
        tmp_line.col_end[RIGHT] = last_col_ins;
//         DBG("LEFT  begin:%d end:%d", tmp_line.col_begin[LEFT], tmp_line.col_end[LEFT]);
//         DBG("RIGHT begin:%d end:%d", tmp_line.col_begin[RIGHT], tmp_line.col_end[RIGHT]);
//         DBG("\n");
    }
    return tmp_line;
}

static void update_diff_buffer(yed_buffer_ptr_t buff_orig, yed_buffer_ptr_t buff_diff) {
    yed_line *line;
    int       row;

    yed_buff_clear_no_undo(buff_diff);

    row = 1;
    bucket_array_traverse(buff_orig->lines, line) {
        yed_buffer_add_line_no_undo(buff_diff);
        yed_buff_set_line_no_undo(buff_diff, row, line);
        row ++;
    }
}

static void line_base_draw(yed_event *event) {
    yed_line  *tmp_line;
    yed_attrs  tmp_attr;

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL
    ||  event->row           >= array_len(event->frame->buffer->lines)) {
        return;
    }

    if (event->frame->buffer == diff_m.buff_diff[LEFT]) {
        if (diff_m.lines[event->row].line_type == INS) {
            tmp_attr = yed_parse_attrs("&active &cyan swap"); //blue
        } else if (diff_m.lines[event->row].line_type == DEL) {
            tmp_attr = yed_parse_attrs("&active &blue swap"); //red
        } else if (diff_m.lines[event->row].line_type == CHG) {
            tmp_attr = yed_parse_attrs("&active &magenta swap");
        } else {
            return;
        }

        tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
        if (tmp_line == NULL) { return; }

        event->row_base_attr = tmp_attr;

    }else if (event->frame->buffer == diff_m.buff_diff[RIGHT]) {
        if (diff_m.lines[event->row].line_type == INS) {
            tmp_attr = yed_parse_attrs("&active &blue swap"); //green
        } else if (diff_m.lines[event->row].line_type == DEL) {
            tmp_attr = yed_parse_attrs("&active &cyan swap"); //blue
        } else if (diff_m.lines[event->row].line_type == CHG) {
            tmp_attr = yed_parse_attrs("&active &magenta swap");
        } else {
            return;
        }

        tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
        if (tmp_line == NULL) { return; }

        event->row_base_attr = tmp_attr;
    }
}

static void line_char_draw(yed_event *event) {
    yed_line  *tmp_line;
    yed_attrs  tmp_attr;

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL
    ||  event->row           >= array_len(event->frame->buffer->lines)) {
        return;
    }

    if (event->frame->buffer == diff_m.buff_diff[LEFT]) {
        if (diff_m.lines[event->row].line_type == CHG) {
            tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
            if (tmp_line == NULL) { return; }

            for (int col = 1; col <= tmp_line->visual_width; col++) {
//                 DBG("color diff len:%d", diff_m.lines[event->row].char_type.size());
                if (diff_m.lines[event->row].char_type.size() < col) {
                    return;
                }

//                 DBG("beg:%d col:%d end:%d", diff_m.lines[event->row].col_begin[LEFT], col,
//                                             diff_m.lines[event->row].col_end[LEFT]);
                if (col >= diff_m.lines[event->row].col_begin[LEFT]
                && col <= diff_m.lines[event->row].col_end[LEFT]) {
                    tmp_attr = yed_parse_attrs("&active &red swap");
//                     DBG("red r:%d c:%d\n", event->row, col);
                    yed_eline_combine_col_attrs(event, col, &tmp_attr);
                }
            }
        }

    } else if (event->frame->buffer == diff_m.buff_diff[RIGHT]) {
        if (diff_m.lines[event->row].line_type == CHG) {
            tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
            if (tmp_line == NULL) { return; }

            for (int col = 1; col <= tmp_line->visual_width; col++) {
//                 DBG("color diff len:%d", diff_m.lines[event->row].char_type.size());
                if (diff_m.lines[event->row].char_type.size() < col) {
                    return;
                }

                if (col >= diff_m.lines[event->row].col_begin[RIGHT]
                && col <= diff_m.lines[event->row].col_end[RIGHT]) {
                    tmp_attr = yed_parse_attrs("&active &red swap");
//                     DBG("red r:%d c:%d\n", event->row, col);
                    yed_eline_combine_col_attrs(event, col, &tmp_attr);
                }
            }
        }
    }
}

static void cursor_move(yed_event *event) {
    int current_row;

    if (event->frame                                              == NULL
    ||  event->frame->buffer                                      == NULL
    ||  event->frame->tree                                        == NULL
    ||  event->frame->tree->parent                                == NULL
    ||  event->frame->tree->parent->split_kind                    != FTREE_VSPLIT
    ||  event->frame->tree->parent->child_trees[0]->frame         == NULL
    ||  event->frame->tree->parent->child_trees[1]->frame         == NULL
    ||  event->frame->tree->parent->child_trees[0]->frame->buffer == NULL
    ||  event->frame->tree->parent->child_trees[1]->frame->buffer == NULL) {
        return;
    }

    if (event->frame->buffer == diff_m.buff_diff[RIGHT]) {
            current_row = event->frame->tree->parent->child_trees[0]->frame->cursor_line;
            yed_set_cursor_far_within_frame(event->frame->tree->parent->child_trees[0]->frame, event->new_row, 1);
    }
}
