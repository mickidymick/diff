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

typedef struct diff_buff_t {
    string              buff_name; //original buffers name
    int                 buff_num;  //use macro
    yed_buffer_ptr_t    buff;      //original buffers ptr
}diff_buff;

map<string, diff_buff>           d_buffers;
map<string, diff_buff>::iterator d_buffers_it;

static void unload(yed_plugin *self);
static void get_or_make_buffers(char *buff_1, char *buff_2);
static void update_diff_buffer(yed_buffer_ptr_t buff, diff_buff &d_buff);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static  int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res);
static void myers_diff(void);
static void line_draw(yed_event *event);
static void row_draw(yed_event *event);
static void clear_buffers(void);
       void diff(int n_args, char **args);

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

        yed_plugin_set_unload_fn(self, unload);
        yed_plugin_set_command(self, "diff", diff);
        yed_plugin_set_completion(self, "diff-compl", diff_completion);
        yed_plugin_set_completion(self, "diff-compl-arg-0", diff_completion_multiple);
        yed_plugin_set_completion(self, "diff-compl-arg-1", diff_completion_multiple);

        return 0;
    }
}

class Line_diff {
    public:
    int    type;
    int    a_num;
    string a_line;
    int    b_num;
    string b_line;

    Line_diff::Line_diff(int t, int a_n, string a, int b_n, string b) {
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

typedef struct line_obj_t {
    int    line_number;
    string line;
} line_obj;

typedef struct buffer_t {
      int              num_lines;
      vector<line_obj> line_objs;
} Buffer;

class Myers {
    public:
        int              Error; //if there is an error in the construction phase
        int              max;
        Buffer           buffer_a;
        Buffer           buffer_b;

        // Member Functions

        //Constructor
        Myers::Myers() {
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
                Error = 1;
            }else if (buff_2 == NULL) {
                yed_cerr("Couldn't find the second diff buffer.");
                Error = 1;
            }

            int row;
            yed_line *line;

            row = 1;
            bucket_array_traverse(buff_1->lines, line) {
                array_zero_term(line->chars);
                A.clear();
                A.append((char *)(line->chars.data));
                A.append("\n");
                line_obj tmp_a;
                tmp_a.line        = A;
                tmp_a.line_number = row;
                buffer_a.line_objs.push_back(tmp_a);
                row++;
            }
            buffer_a.num_lines = row - 1;

            row = 1;
            bucket_array_traverse(buff_2->lines, line) {
                array_zero_term(line->chars);
                B.clear();
                B.append((char *)(line->chars.data));
                B.append("\n");
                line_obj tmp_b;
                tmp_b.line        = B;
                tmp_b.line_number = row;
                buffer_b.line_objs.push_back(tmp_b);
                row++;
            }
            buffer_b.num_lines = row - 1;

//             LOG_FN_ENTER();
//             yed_log("%s\n", buff_1->name);
//             for(auto i : buffer_a.line_objs) {
//                 yed_log("%d: %s", i.line_number, i.line.c_str());
//             }
//             yed_log("%s\n", buff_2->name);
//             for(auto i : buffer_b.line_objs) {
//                 yed_log("%d: %s", i.line_number, i.line.c_str());
//             }
//             LOG_EXIT();
            Error = 0;
        }

        int wrap_indx(int size, int indx) {
            if ( indx < 0 ) {
                return (size + indx);
            }

            return indx;
        }

        //Finds the Shortest Edit Path
        vector<vector<int>> shortest_edit(void) {
            int max;
            int tmp_k;
            int tmp_k_add;
            int tmp_k_min;
            int x;
            int y;
            int size;

            max  = buffer_a.num_lines + buffer_b.num_lines;
            size = 2 * max + 1;
            type_arr.resize(size);

            vector<int> v(2 * max + 1);
            v[1] = 0;
            vector<vector<int>> trace;


            for(int d = 0; d <= max; d++) {
                trace.push_back(v);
                for(int k = -d; k <= d; k+=2) {
                    if (k == -d || (k != d && v[wrap_indx(size, k - 1)] < v[wrap_indx(size, k + 1)])) {
                        x = v[wrap_indx(size, k + 1)];
                    } else {
                        x = v[wrap_indx(size, k - 1)] + 1;
                    }

                    y = x - k;

                    while (x < buffer_a.num_lines &&
                           y < buffer_b.num_lines &&
                           buffer_a.line_objs[x].line == buffer_b.line_objs[y].line) {
                               x++;
                               y++;
                    }

                    v[wrap_indx(size, k)] = x;

                    if (x >= buffer_a.num_lines &&
                        y >= buffer_b.num_lines) {
//                         LOG_FN_ENTER();
//                         yed_log("Shortest Edit: %d\n", d);
//                         LOG_EXIT();
                        return trace;
                    }
                }
            }
        }

        backtrack_yield(int prev_x, int prev_y, int x, int y) {
//             DBG("(%d, %d) -> (%d, %d)\n", prev_x, prev_y, x, y);

            string a_line;
            string b_line;

            if (prev_x >= 0 && prev_x < buffer_a.line_objs.size()) {
                a_line = buffer_a.line_objs[prev_x].line;
            }
            if (prev_y >= 0 && prev_y < buffer_b.line_objs.size()) {
                b_line = buffer_b.line_objs[prev_y].line;
            }

            if (x == prev_x) {
                Line_diff tmp_line_diff(INS, 0, "", prev_y + 1, b_line);
                l_diff.insert(l_diff.begin(), tmp_line_diff);
                type_arr[prev_y + 1].b = INS;
            } else if (y == prev_y) {
                Line_diff tmp_line_diff(DEL, prev_x + 1, a_line, 0, "");
                l_diff.insert(l_diff.begin(), tmp_line_diff);
                type_arr[prev_x + 1].a = DEL;
            } else {
                Line_diff tmp_line_diff(EQL, prev_x + 1, a_line, prev_y + 1, b_line);
                l_diff.insert(l_diff.begin(), tmp_line_diff);
                type_arr[prev_x + 1].a = EQL;
                type_arr[prev_y + 1].b = EQL;
            }
        }

        //Backtracks the shortest edit path finiding the path
        void backtrack(vector<vector<int>> trace, int x, int y) {
            int         max;
            int         size;
            int         k;
            int         prev_k;
            int         prev_x;
            int         prev_y;
            vector<int> v;

            max  = buffer_a.num_lines + buffer_b.num_lines;
            size = 2 * max + 1;

            for (int d = trace.size() - 1; d >= 0; d--) {

                v = trace[d];
                k = x - y;

                if ( k == -d ||
                     (k != d && v[wrap_indx(size, k - 1)] < v[wrap_indx(size, k + 1)])) {
                    prev_k = k + 1;
                } else {
                    prev_k = k - 1;
                }

                prev_x = v[wrap_indx(size, prev_k)];
                prev_y = prev_x - prev_k;

                while (x > prev_x && y > prev_y) {
                    backtrack_yield(x - 1, y - 1, x, y);
                    x--;
                    y--;
                }

                if (d > 0) {
                    backtrack_yield(prev_x, prev_y, x, y);
                }

                x = prev_x;
                y = prev_y;
            }
        }

        //Main diff func
        void diff(void) {
            if (Error) {
                yed_cerr("Error!");
                return;
            }

            vector<vector<int>> se = shortest_edit();

            backtrack(se, buffer_a.num_lines, buffer_b.num_lines);

//             LOG_FN_ENTER();
//             yed_log("   \n");
//             for (int i = 0; i < l_diff.size(); i++) {
//                 string type  = l_diff[i].type == INS ? "+" : l_diff[i].type == DEL ? "-" : " ";
//                 string left  = l_diff[i].type == INS ? " " : to_string(l_diff[i].a_num);
//                 string right = l_diff[i].type == DEL ? " " : to_string(l_diff[i].b_num);
//                 string l     = l_diff[i].type == INS ? l_diff[i].b_line : l_diff[i].a_line;
//                 yed_log("%s   %s   %s   %s\n", type.c_str(), left.c_str(), right.c_str(), l.c_str());
//             }
//             LOG_EXIT();
        }
};

void diff(int n_args, char **args) {
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

    clear_buffers();

    buff_1 = args[0];
    buff_2 = args[1];

    if (strcmp(buff_1, buff_2) == 0) {
        yed_cerr("The two buffers must be different!");
        return;
    }
    get_or_make_buffers(buff_1, buff_2);

    snprintf(tmp_buff_1, 512, "diff:%s", buff_1);
    snprintf(tmp_buff_2, 512, "diff:%s", buff_2);

    d_buffers_it_1 = d_buffers.find(string(tmp_buff_1));
    d_buffers_it_2 = d_buffers.find(string(tmp_buff_2));

    Myers myers;
    myers.diff();

    if (d_buffers_it_1 != d_buffers.end() && d_buffers_it_2 != d_buffers.end()) {
        snprintf(tmp_buff_1, 512, "*diff:%s", buff_1);
        snprintf(tmp_buff_2, 512, "*diff:%s", buff_2);
        YEXE("frame-new");
        YEXE("buffer", tmp_buff_1);
        YEXE("frame-vsplit");
        YEXE("buffer", tmp_buff_2);
    }
    LOG_FN_ENTER();
    yed_log("%s, %s", buff_1, buff_2);
    LOG_EXIT();
}

static void line_draw(yed_event *event) {
    yed_attrs       *attr;
    yed_attrs        new_attr;
    int              loc;
    yed_buffer_ptr_t buff_1;
    yed_buffer_ptr_t buff_2;
    buff_1      = NULL;
    buff_2      = NULL;
    map<string, diff_buff>::iterator d_buffers_it_1;
    map<string, diff_buff>::iterator d_buffers_it_2;
    char tmp_buff[512];

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

    snprintf(tmp_buff, 512, "*%s", d_buffers_it_1->first.c_str());

    yed_attrs *attr_tmp;
    yed_line  *line;

    if (strcmp(event->frame->buffer->name, tmp_buff) == 0) {
        if (type_arr[event->row].a == INS) {
            attr_tmp = &yed_parse_attrs("bg &green");
        } else if (type_arr[event->row].a == DEL) {
            attr_tmp = &yed_parse_attrs("&red.bg");
        } else if (type_arr[event->row].a == EQL) {
            attr_tmp = &yed_parse_attrs("bg &blue");
        }

        line = yed_buff_get_line(event->frame->buffer, event->row);
        if (line == NULL) { return; }

        for (int loc = 1; loc <= line->visual_width; loc += 1) {
            yed_eline_set_col_attrs(event, loc, attr_tmp);
        }
    }
}

static void row_draw(yed_event *event) {

}

int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res) {
    char *tmp[2] = {"file", "diff-compl"};
    return yed_complete_multiple(2, tmp, name, comp_res);
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

static void clear_buffers(void) {
    char        tmp_buff[512];
    yed_buffer *buffer;
    for(d_buffers_it = d_buffers.begin(); d_buffers_it != d_buffers.end(); d_buffers_it++) {
        snprintf(tmp_buff, 512, "*%s", d_buffers_it->first.c_str());
        buffer = yed_get_buffer(tmp_buff);

        if (buffer != NULL) {
            yed_free_buffer(buffer);
        }
    }
    d_buffers.clear();
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
//                 d_buffers_it->second.lines     = vector<buffer_line>();

//                 vector<string> tmp_s_lines;
//                 bucket_array_traverse(d_buffers_it->second.buff->lines, line) {
//                     string tmp((char *)line->chars.data);
//                     tmp_s_lines.push_back(tmp);
//                 }
//                 d_buffers_it->second.s_lines   = tmp_s_lines;

            }else if (strcmp(buff_2, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buffers_it = d_buffers.emplace(string(buff), diff_buff{}).first;
                d_buffers_it->second.buff_name = string(check_buff);
                d_buffers_it->second.buff      = tree_it_val(buffers_it);
                d_buffers_it->second.buff_num  = RIGHT;
//                 d_buffers_it->second.lines     = vector<buffer_line>();
//                 vector<string> tmp_s_lines;
//                 bucket_array_traverse(d_buffers_it->second.buff->lines, line) {
//                     string tmp((char *)line->chars.data);
//                     tmp_s_lines.push_back(tmp);
//                 }
//                 d_buffers_it->second.s_lines   = tmp_s_lines;
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
