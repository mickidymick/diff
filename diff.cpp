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

#define INS (1)
#define DEL (2)
#define EQL (3)
#define CHG (4)

typedef struct diff_buff_t {
    string              buff_name; //original buffers name
    int                 buff_num;  //use macro
    yed_buffer_ptr_t    buff;      //original buffers ptr
} diff_buff;

map<string, diff_buff>           d_buffers;
map<string, diff_buff>::iterator d_buffers_it;

typedef struct line_color_t {
    int         line_type;
    vector<int> char_type;
} line_color;

vector<line_color> color_diff;

class Line_diff {
    public:
    int    type;
    int    a_num;
    string a_line;
    int    b_num;
    string b_line;

    Line_diff(int t, int a_n, string a, int b_n, string b) {
        type   = t;
        a_num  = a_n;
        a_line = a;
        b_num  = b_n;
        b_line = b;
    }
};

vector<Line_diff> l_diff;

typedef struct type_t {
    int a;
    int b;
} type;

vector<type> type_arr;

typedef struct buffer_t {
      int              num_lines;
      vector<string>   lines;
} Buffer;

Buffer buffer_a;
Buffer buffer_b;

#include "myers_diff.hpp"

//Base Yed Plugin Functions
static void get_or_make_buffers(char *buff_1, char *buff_2);
static void unload(yed_plugin *self);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static  int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res);
static void clear_buffers(char* b1, char* b2, char* b3, char* b4);

//Diff Functions
static void diff(int n_args, char **args);
static int  init(void);
static void align_buffers(Myers<vector<string>> myers, diff_buff buff_1, diff_buff buff_2);
static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff &d_buff);
static void row_draw(yed_event *event);
static void line_draw(yed_event *event);
static void cursor_move(yed_event *event);

extern "C" {
    int yed_plugin_boot(yed_plugin *self) {
        YED_PLUG_VERSION_CHECK();

    /*     ROW_PRE_CLEAR */
        yed_event_handler row_draw_eh;
        row_draw_eh.kind = EVENT_ROW_PRE_CLEAR;
        row_draw_eh.fn   = row_draw;
        yed_plugin_add_event_handler(self, row_draw_eh);

    /*     LINE_PRE_DRAW */
        yed_event_handler line_draw_eh;
        line_draw_eh.kind = EVENT_LINE_PRE_DRAW;
        line_draw_eh.fn   = line_draw;
        yed_plugin_add_event_handler(self, line_draw_eh);

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

static void get_or_make_buffers(char *buff_1, char *buff_2) {
    char                                         tmp_buff[512];
    char                                         buff[512];
    char                                        *check_buff;
    yed_line                                    *line;
    yed_buffer                                  *buffer;
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;

    buffers_it = tree_lookup(ys->buffers, buff_1);
    if (!tree_it_good(buffers_it)) {
        YEXE("buffer-hidden", buff_1);
    }

    buffers_it = tree_lookup(ys->buffers, buff_2);
    if (!tree_it_good(buffers_it)) {
        YEXE("buffer-hidden", buff_2);
    }

    tree_traverse(ys->buffers, buffers_it) {
        if (tree_it_val(buffers_it)->kind == BUFF_KIND_FILE &&
            tree_it_val(buffers_it)->flags != BUFF_SPECIAL) {

            check_buff = tree_it_key(buffers_it);
            if (strcmp(buff_1, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buffers_it = d_buffers.emplace(string(buff), diff_buff{}).first;
                d_buffers_it->second.buff_name = string(check_buff);
                d_buffers_it->second.buff      = tree_it_val(buffers_it);
                d_buffers_it->second.buff_num  = LEFT;
            }else if (strcmp(buff_2, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buffers_it = d_buffers.emplace(string(buff), diff_buff{}).first;
                d_buffers_it->second.buff_name = string(check_buff);
                d_buffers_it->second.buff      = tree_it_val(buffers_it);
                d_buffers_it->second.buff_num  = RIGHT;
            }
        }
    }

    for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
        snprintf(tmp_buff, 512, "*%s", d_buffers_it->first.c_str());
        buffer = yed_get_buffer(tmp_buff);

        if (buffer == NULL) {
            buffer = yed_create_buffer(tmp_buff);
            buffer->flags |= BUFF_SPECIAL;
        }

        update_diff_buffer(buffer, d_buffers_it->second);
    }
}

static void unload(yed_plugin *self) {
}

static int diff_completion(char *name, struct yed_completion_results_t *comp_res) {
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

static int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res) {
    char *tmp[2] = {"file", "diff-compl"};
    return yed_complete_multiple(2, tmp, name, comp_res);
}

static void clear_buffers(char* b1, char* b2, char* b3, char* b4) {
    char        tmp_buff[512];
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

    d_buffers.clear();
}

static void diff(int n_args, char **args) {
    char                                 *buff_1;
    char                                 *buff_2;
    char                                  tmp_buff_1[512];
    char                                  tmp_buff_2[512];
    map<string, diff_buff>::iterator d_buffers_it_1;
    map<string, diff_buff>::iterator d_buffers_it_2;

    if (n_args != 2) {
        yed_cerr("expected 2 arguments, two buffer names");
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

    get_or_make_buffers(buff_1, buff_2);

    snprintf(tmp_buff_1, 512, "diff:%s", buff_1);
    snprintf(tmp_buff_2, 512, "diff:%s", buff_2);

    d_buffers_it_1 = d_buffers.find(string(tmp_buff_1));
    d_buffers_it_2 = d_buffers.find(string(tmp_buff_2));

    if (init()) {
        yed_cerr("could not initialize the diff");
        return;
    }
    l_diff.clear();

    Myers<vector<string>> myers(buffer_a.lines, buffer_a.num_lines, buffer_b.lines, buffer_b.num_lines);
    int err = myers.diff();

    if (err) {
        return;
    }

    if (d_buffers_it_1 != d_buffers.end() && d_buffers_it_2 != d_buffers.end()) {
        snprintf(tmp_buff_1, 512, "*diff:%s", buff_1);
        snprintf(tmp_buff_2, 512, "*diff:%s", buff_2);
        YEXE("frame-new");
        YEXE("buffer", tmp_buff_1);
        YEXE("frame-vsplit");
        YEXE("buffer", tmp_buff_2);
    }

    align_buffers(myers, d_buffers_it_1->second, d_buffers_it_2->second);

    LOG_FN_ENTER();
    yed_log("%s, %s", buff_1, buff_2);
    LOG_EXIT();
}

static int init(void) {
    string A;
    string B;
    yed_buffer_ptr_t buff_1;
    yed_buffer_ptr_t buff_2;
    buff_1      = NULL;
    buff_2      = NULL;
    map<string, diff_buff>::iterator d_buffers_it_1;
    map<string, diff_buff>::iterator d_buffers_it_2;

    for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
        if (d_buffers_it->second.buff_num == LEFT) {
            buff_1 = d_buffers_it->second.buff;
            d_buffers_it_1 = d_buffers_it;
        }else if (d_buffers_it->second.buff_num == RIGHT) {
            buff_2 = d_buffers_it->second.buff;
            d_buffers_it_2 = d_buffers_it;
        }
    }

    if (buff_1 == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return 1;
    }else if (buff_2 == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return 1;
    }

    int row;
    yed_line *line;

    row = 1;
    bucket_array_traverse(buff_1->lines, line) {
        array_zero_term(line->chars);
        A.clear();
        A.append((char *)(line->chars.data));
        A.append("\n");
        buffer_a.lines.push_back(A);
        row++;
    }
    buffer_a.num_lines = row - 1;

    row = 1;
    bucket_array_traverse(buff_2->lines, line) {
        array_zero_term(line->chars);
        B.clear();
        B.append((char *)(line->chars.data));
        B.append("\n");
        buffer_b.lines.push_back(B);
        row++;
    }
    buffer_b.num_lines = row - 1;

    return 0;
}

static void align_buffers(Myers<vector<string>> myers, diff_buff buff_1, diff_buff buff_2) {
    char          tmp_buff[512];
    yed_buffer_t* buff;
    int           a = 0;
    int           b = 0;
    int           tmp_i;
    line_color    line;
    int           add = 0;
    int           sub = 0;
    color_diff.clear();
    line.line_type = 0;
    color_diff.push_back(line);

    for (int i = 0; i < l_diff.size(); i++) {
        line_color line1;
        if (l_diff[i].type == INS) {
            snprintf(tmp_buff, 512, "*diff:%s", buff_1.buff->name);
            buff = yed_get_buffer(tmp_buff);
            if (buff != NULL) {
                DBG("%d: INS\n", i+1);
                add++;

//                 vector<string> tmp_1;
//                 for (int c = 0; c < buffer_a.lines[i].size(); c++) {
//                     tmp_1.push_back(string(1, buffer_a.lines[i][c]));
//                 }
//                 vector<string> tmp_2;
//                 for (int c = 0; c < buffer_b.lines[i].size(); c++) {
//                     tmp_2.push_back(string(1, buffer_b.lines[i][c]));
//                 }
//                 Myers<vector<string>> myers_r(tmp_1, tmp_1.size(), tmp_2, tmp_2.size());
//                 int err = myers_r.diff();
            }
        } else if (l_diff[i].type == DEL) {
            snprintf(tmp_buff, 512, "*diff:%s", buff_2.buff->name);
            buff = yed_get_buffer(tmp_buff);
            if (buff != NULL) {
                DBG("%d: DEL\n", i+1);
                sub++;
            }
        } else {
            DBG("%d: EQL\n", i+1);
            if (i > 0) {
                tmp_i = i - 1;
            } else {
                tmp_i = i;
            }



            int wierd = 0;
            if (add < sub) {
                if (add > 0) {
                    for (int j = 0; j < add; j++) {
                        line_color line2;
                        line2.line_type = CHG;
                        color_diff.push_back(line2);
                        DBG("%d:   CHG\n", tmp_i);
                        wierd = 1;
                    }
                }

                snprintf(tmp_buff, 512, "*diff:%s", buff_2.buff->name);
                buff = yed_get_buffer(tmp_buff);
                if (buff != NULL) {
                    for (int j = add; j < sub; j++) {
                        line_color line2;
                        char dashes[512];
                        memset(dashes, '-', 512);
                        if (wierd) {
                            yed_buff_insert_line_no_undo(buff, l_diff[tmp_i].b_num + b + 1);
                            yed_buff_insert_string(buff, dashes, l_diff[tmp_i].b_num + b + 1, 1);
                        } else {
                            yed_buff_insert_line_no_undo(buff, l_diff[tmp_i].b_num + b);
                            yed_buff_insert_string(buff, dashes, l_diff[tmp_i].b_num + b, 1);
                        }
                        DBG("%d:   DEL b:%d row:%d f:%d\n", tmp_i, b, l_diff[tmp_i].b_num, l_diff[tmp_i].b_num + b);
                        b++;
                        line2.line_type = DEL;
                        color_diff.push_back(line2);
                    }
                }
            } else if (add > sub) {
                if (sub > 0) {
                    for (int j = 0; j < sub; j++) {
                        line_color line2;
                        line2.line_type = CHG;
                        color_diff.push_back(line2);
                        DBG("%d:   CHG\n", tmp_i);
                        wierd = 1;
                    }
                }

                snprintf(tmp_buff, 512, "*diff:%s", buff_1.buff->name);
                buff = yed_get_buffer(tmp_buff);
                if (buff != NULL) {
                    for (int j = sub; j < add; j++) {
                        line_color line2;
                        char dashes[512];
                        memset(dashes, '-', 512);
                        if (wierd) {
                            yed_buff_insert_line_no_undo(buff, l_diff[tmp_i].a_num + a);
                            yed_buff_insert_string(buff, dashes, l_diff[tmp_i].a_num + a, 1);
                        } else {
                            yed_buff_insert_line_no_undo(buff, l_diff[tmp_i].a_num + a);
                            yed_buff_insert_string(buff, dashes, l_diff[tmp_i].a_num + a, 1);
                        }
                        DBG("%d:   INS a:%d row:%d f:%d\n", tmp_i, a, l_diff[tmp_i].a_num, l_diff[tmp_i].a_num + a);
                        a++;
                        line2.line_type = INS;
                        color_diff.push_back(line2);
                    }
                }
            } else if (add > 0 && sub > 0) {
                for (int j = 0; j < add; j++) {
                    line_color line2;
                    line2.line_type = CHG;
                    color_diff.push_back(line2);
                    DBG("%d:   CHG\n", tmp_i);
                }
            }

            line1.line_type = EQL;
            color_diff.push_back(line1);

            add   = 0;
            sub   = 0;
            wierd = 0;
        }
    }
}

static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff &d_buff) {
    yed_line *line;
    int       row;

    yed_buff_clear_no_undo(buff);

    row = 1;
    bucket_array_traverse(d_buff.buff->lines, line) {
        yed_buffer_add_line_no_undo(buff);
        yed_buff_set_line_no_undo(buff, row, line);
        row += 1;
    }
}

static void row_draw(yed_event *event) {
    yed_attrs       *attr;
    yed_attrs        new_attr;
    int              loc;
    yed_buffer_ptr_t buff_1;
    yed_buffer_ptr_t buff_2;
    buff_1      = NULL;
    buff_2      = NULL;
    map<string, diff_buff>::iterator d_buffers_it_1;
    map<string, diff_buff>::iterator d_buffers_it_2;
    char tmp_buff_a[512];
    char tmp_buff_b[512];

    if (d_buffers.size() == 0) {
        return;
    }

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL) {
        return;
    }

    for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
        if (d_buffers_it->second.buff_num == LEFT) {
            buff_1 = d_buffers_it->second.buff;
            d_buffers_it_1 = d_buffers_it;
        }else if (d_buffers_it->second.buff_num == RIGHT) {
            buff_2 = d_buffers_it->second.buff;
            d_buffers_it_2 = d_buffers_it;
        }
    }

    if (buff_1 == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return;
    }else if (buff_2 == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return;
    }

    yed_attrs attr_tmp;
    yed_line  *line;

    snprintf(tmp_buff_a, 512, "*%s", d_buffers_it_1->first.c_str());
    snprintf(tmp_buff_b, 512, "*%s", d_buffers_it_2->first.c_str());

    if (strcmp(event->frame->buffer->name, tmp_buff_a) == 0) {
        if (color_diff[event->row].line_type == INS) {
            attr_tmp = yed_parse_attrs("&active &cyan swap"); //blue
        } else if (color_diff[event->row].line_type == DEL) {
            attr_tmp = yed_parse_attrs("&active &blue swap"); //red
        } else if (color_diff[event->row].line_type == CHG) {
            attr_tmp = yed_parse_attrs("&active &magenta swap");
        } else {
            return;
        }

        line = yed_buff_get_line(event->frame->buffer, event->row);
        if (line == NULL) { return; }

        event->row_base_attr = attr_tmp;

    }else if (strcmp(event->frame->buffer->name, tmp_buff_b) == 0) {
        if (color_diff[event->row].line_type == INS) {
            attr_tmp = yed_parse_attrs("&active &blue swap"); //green
        } else if (color_diff[event->row].line_type == DEL) {
            attr_tmp = yed_parse_attrs("&active &cyan swap"); //blue
        } else if (color_diff[event->row].line_type == CHG) {
            attr_tmp = yed_parse_attrs("&active &magenta swap");
        } else {
            return;
        }

        line = yed_buff_get_line(event->frame->buffer, event->row);
        if (line == NULL) { return; }

        event->row_base_attr = attr_tmp;
    }
}

static void line_draw(yed_event *event) {
//     yed_attrs       *attr;
//     yed_attrs        new_attr;
//     int              loc;
//     yed_buffer_ptr_t buff_1;
//     yed_buffer_ptr_t buff_2;
//     buff_1      = NULL;
//     buff_2      = NULL;
//     map<string, diff_buff>::iterator d_buffers_it_1;
//     map<string, diff_buff>::iterator d_buffers_it_2;
//     char tmp_buff[512];

//     if (d_buffers.size() == 0) {
//         return;
//     }

//     if (event->frame         == NULL
//     ||  event->frame->buffer == NULL) {
//         return;
//     }

//     for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
//         if (d_buffers_it->second.buff_num == LEFT) {
//             buff_1 = d_buffers_it->second.buff;
//             d_buffers_it_1 = d_buffers_it;
//         }else if (d_buffers_it->second.buff_num == RIGHT) {
//             buff_2 = d_buffers_it->second.buff;
//             d_buffers_it_2 = d_buffers_it;
//         }
//     }

//     if (buff_1 == NULL) {
//         yed_cerr("Couldn't find the first diff buffer.");
//         return;
//     }else if (buff_2 == NULL) {
//         yed_cerr("Couldn't find the second diff buffer.");
//         return;
//     }

//     snprintf(tmp_buff, 512, "*%s", d_buffers_it_1->first.c_str());

//     yed_attrs *attr_tmp;
//     yed_line  *line;

//     if (strcmp(event->frame->buffer->name, tmp_buff) == 0) {
//         if (type_arr[event->row].a == INS) {
//             attr_tmp = &yed_parse_attrs("bg &green");
//         } else if (type_arr[event->row].a == DEL) {
//             attr_tmp = &yed_parse_attrs("&red.bg");
//         } else if (type_arr[event->row].a == EQL) {
//             attr_tmp = &yed_parse_attrs("bg &blue");
//         }

//         line = yed_buff_get_line(event->frame->buffer, event->row);
//         if (line == NULL) { return; }

//         for (int loc = 1; loc <= line->visual_width; loc += 1) {
//             yed_eline_set_col_attrs(event, loc, attr_tmp);
//         }
//     }
}

static void cursor_move(yed_event *event) {
    yed_buffer_ptr_t buff_1;
    yed_buffer_ptr_t buff_2;
    map<string, diff_buff>::iterator d_buffers_it_1;
    map<string, diff_buff>::iterator d_buffers_it_2;
    char tmp_buff_a[512];
    char tmp_buff_b[512];
    int current_row;

    if (d_buffers.size() == 0) {
        return;
    }

    if (event->frame                           == NULL
    ||  event->frame->buffer                   == NULL
    ||  event->frame->tree                     == NULL
    ||  event->frame->tree->parent             == NULL
    ||  event->frame->tree->parent->split_kind != FTREE_VSPLIT
    ||  event->frame->tree->parent->child_trees[0]->frame == NULL
    ||  event->frame->tree->parent->child_trees[1]->frame == NULL
    ||  event->frame->tree->parent->child_trees[0]->frame->buffer == NULL
    ||  event->frame->tree->parent->child_trees[1]->frame->buffer == NULL) {
        return;
    }

    for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
        if (d_buffers_it->second.buff_num == LEFT) {
            buff_1 = d_buffers_it->second.buff;
            d_buffers_it_1 = d_buffers_it;
        }else if (d_buffers_it->second.buff_num == RIGHT) {
            buff_2 = d_buffers_it->second.buff;
            d_buffers_it_2 = d_buffers_it;
        }
    }

    if (buff_1 == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return;
    }else if (buff_2 == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return;
    }

    snprintf(tmp_buff_a, 512, "*%s", d_buffers_it_1->first.c_str());
    snprintf(tmp_buff_b, 512, "*%s", d_buffers_it_2->first.c_str());

    if (strcmp(event->frame->buffer->name, tmp_buff_a) == 0) {
            current_row = event->frame->tree->parent->child_trees[1]->frame->cursor_line;
//             yed_set_cursor_far_within_frame(event->frame->tree->parent->child_trees[1]->frame, event->new_row, 1);
    }else if (strcmp(event->frame->buffer->name, tmp_buff_b) == 0) {
            current_row = event->frame->tree->parent->child_trees[0]->frame->cursor_line;
            yed_set_cursor_far_within_frame(event->frame->tree->parent->child_trees[0]->frame, event->new_row, 1);
    }
}
