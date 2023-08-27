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

typedef struct buffer_line_t {
    int old_row;         //row that this line is from in the old buffer
    int collapsed_start; //start of the collapsed row in the old buffer
    int collapsed_end;   //end of the collapsed row in the old buffer
    int row_type;        //type of this row
    int start_col;       //start of the change
    int end_col;         //end of the change
    buffer_line(){
      old_row         =  0;
      collapsed_start = -1;
      collapsed_end   = -1;
      row_type        = -1;
      start_col       = -1;
      end_col         = -1;
    };
}buffer_line;

typedef struct diff_buff_t {
    string              buff_name;    //original buffers name
    int                 buff_num;     //use macro
    yed_buffer_ptr_t    buff;         //original buffers ptr
    vector<buffer_line> lines; //all the lines in the new buffer
}diff_buff;

map<string, diff_buff> d_buffers;
map<string, diff_buff>::iterator d_buffers_it;
static int                                   len;

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
        len       = 0;

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
    get_or_make_buffers(buff_1, buff_2);

    snprintf(tmp_buff_1, 512, "diff:%s", buff_1);
    snprintf(tmp_buff_2, 512, "diff:%s", buff_2);

    d_buffers_it_1 = d_buffers.find(string(tmp_buff_1));
    d_buffers_it_2 = d_buffers.find(string(tmp_buff_2));

    myers_diff();

    if (d_buffers_it_1 != d_buffers.end() && d_buffers_it_2 != d_buffers.end()) {
        snprintf(tmp_buff_1, 512, "*diff:%s", buff_1);
        snprintf(tmp_buff_2, 512, "*diff:%s", buff_2);
        YEXE("frame-new");
        YEXE("buffer", tmp_buff_1);
        YEXE("frame-vsplit");
        YEXE("buffer", tmp_buff_2);
    }
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

    snprintf(tmp_buff, 512, "*%s", d_buffers_it_2->first.c_str());

    if (strcmp(event->frame->buffer->name, tmp_buff) == 0) {
        if (d_buffers_it_2->second.lines[event->row-1].start_col != -1) {
            loc = 1;
            new_attr = yed_parse_attrs("&bad swap");
            array_traverse(event->line_attrs, attr) {
                if (loc >= d_buffers_it_2->second.lines[event->row-1].start_col &&
                    loc <= d_buffers_it_2->second.lines[event->row-1].end_col) {
                        yed_combine_attrs(attr, &new_attr);
                }
                loc++;
            }
        }
    }
}

static void row_draw(yed_event *event) {

}

static void myers_diff(void) {
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
        return;
    }else if (buff_2 == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return;
    }

    yed_line *line;
/*     vector<int> Va; */
/*     vector<int> Vb; */
/*     Va.push_back(0); */
/*     Vb.push_back(0); */

    map<int, int> Vb;

    bucket_array_traverse(buff_1->lines, line) {
        array_zero_term(line->chars);
        A.append((const char *)(line->chars.data));
        A.append("\n");
/*         Va.push_back(array_len(line->chars) + 1); */
    }

    int row   = 0;
    int total = 0;
    bucket_array_traverse(buff_2->lines, line) {
        array_zero_term(line->chars);
        B.append((const char *)(line->chars.data));
        B.append("\n");
/*         Vb.push_back(array_len(line->chars) + 1); */

        row++;
        total += (array_len(line->chars) + 1);
        Vb.insert({total, row});
    }
    d_buffers_it_2->second.lines.resize(row + 1);

    MyersDiff<string> diffs{A, B};
    return;
    int b_row          = 0; //index of row for current diff obj
    int b_first_row    = 0; //first row diff obj relates to in file b
    int b_last_row     = 0; //last row diff obj relates to in file b
    int b_col_first    = 0; //index of first character in the first row indexed on 0
    int b_col_last     = 0; //index of last character in the last row indexed on 0
    int b_col_last_tmp = 0; //index of last character in the last row indexed on 0
    int line_first = -1; //index of first different character in line
    int line_last  = -1; //index of last different character in line
    int b_loc      = 0;
    int b_loc_e    = 0;
    int b_first_i  = 0; //index of row for first diff obj on the line indexed on 0
    int b_last_i   = 0; //index of row for last diff obj on the line indexed on 0


    int b_cur_row  = 1;
    int b_begin    = 0; //index of begining character of diff obj on the line/lines indexed on 0
    int b_end      = 0; //index of ending character of diff obj on the line/lines indexed on 0

#if 0
    for (int i=0; i<diffs.diffs().size(); i++) {
        b_begin     = diffs.diffs()[i].text.from - B.begin();
        b_end       = diffs.diffs()[i].text.till - B.begin();

        b_first_row = Vb.lower_bound(b_begin)->second;
        b_last_row  = Vb.lower_bound(b_end)->second;

        b_col_first = b_begin - std::prev(Vb.lower_bound(b_begin), 1)->first + 1;
        b_col_last  = b_end - std::prev(Vb.lower_bound(b_end), 1)->first + 1;

        if (diffs.diffs()[i].operation == INSERT) {
            for (int j=b_first_row; j<=b_last_row; j++) {
                if (j == b_first_row) {
                    if (d_buffers_2_it_2->second->lines[j].start_col == -1) {
                        d_buffers_2_it_2->second->lines[j].start_col = b_col_first;

                        d_buffers_it_2->second.lines.back().old_row         = b_loc_e;
                        d_buffers_it_2->second.lines.back().collapsed_start = 0;
                        d_buffers_it_2->second.lines.back().collapsed_end   = 0;
                        d_buffers_it_2->second.lines.back().row_type        = 0;
                    }
                }

                if (j == b_last_row) {
                    d_buffers_2_it_2->second->lines[j].end_col = b_col_last;
                    yed_cerr("row:%d start:%d end:%d\n", i,
                            d_buffers_it_2->second.lines[i].start_col,
                            d_buffers_it_2->second.lines[i].end_col);
                }
            }
        }
    }
#endif






    #if 0
    d_buffers_it_2->second.lines.emplace_back();
    for (int i=0; i<diffs.diffs().size(); i++) {
        b_begin    = diffs.diffs()[i].text.from - B.begin();
        b_end      = diffs.diffs()[i].text.till - B.begin();
        b_last_row = (Vb.lower_bound(b_end))->second;

        if (diffs.diffs()[i].operation == INSERT) {
            if ( d_buffers_it_2->second.lines[b_cur_row].start_col == -1) {
                d_buffers_it_2->second.lines[b_cur_row].start_col = b_begin - b_col_last + 1;
                b_first_i = i;
            }
            if (b_end - b_col_last + 1 > line_last) {
                line_last = b_end - b_col_last + 1;
            }
            yed_cprint("%s  %dB row:%d col:%d\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - B.begin(), b_loc, b_begin - Vb[b_loc - 1] + 1);
        }else if (diffs.diffs()[i].operation == DELETE) {
            yed_cprint("%s  %dA\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - A.begin());
        }else {
            yed_cprint("%s  %dA\n", diffs.diffs()[i].str().c_str(), diffs.diffs()[i].text.from - A.begin());
        }

/*             if (line_first > -1) { */
/*                 auto s_it = diffs.diffs()[i].text.from; */
/*                 b_loc_e = b_loc; */
/*                 for (; s_it<diffs.diffs()[i].text.till; s_it++) { */
/*                     if (*s_it == '\n') { */
/*                         d_buffers_it_2->second.lines.emplace_back(); */
/*                         d_buffers_it_2->second.lines.back().old_row         = b_loc_e; */
/*                         d_buffers_it_2->second.lines.back().collapsed_start = 0; */
/*                         d_buffers_it_2->second.lines.back().collapsed_end   = 0; */
/*                         d_buffers_it_2->second.lines.back().row_type        = 0; */
/*                         d_buffers_it_2->second.lines.back().start_col       = line_first; */
/*                         d_buffers_it_2->second.lines.back().end_col         = line_last; */
/*                         yed_cerr("row:%d start:%d end:%d\n", d_buffers_it_2->second.lines.size(), d_buffers_it_2->second.lines.back().start_col, d_buffers_it_2->second.lines.back().end_col); */
/*  */
/*                         line_first = -1; */
/*                         line_last  = -1; */
/*                         b_loc_e++; */
/*                     } */
/*                 } */
/*             } */

        if (b_end >= b_col_last) {
            for (int j=b_first_i; j<i; j++) {
                auto s_it = diffs.diffs()[j].text.from;
                b_loc_e = j;
                for (; s_it<diffs.diffs()[j].text.till; s_it++) {
                    if (*s_it == '\n') {
                        d_buffers_it_2->second.lines.emplace_back();
                        d_buffers_it_2->second.lines.back().old_row         = b_loc_e;
                        d_buffers_it_2->second.lines.back().collapsed_start = 0;
                        d_buffers_it_2->second.lines.back().collapsed_end   = 0;
                        d_buffers_it_2->second.lines.back().row_type        = 0;
                        d_buffers_it_2->second.lines.back().start_col       = line_first;
                        d_buffers_it_2->second.lines.back().end_col         = line_last;
                        yed_cerr("row:%d start:%d end:%d\n", d_buffers_it_2->second.lines.size(), d_buffers_it_2->second.lines.back().start_col, d_buffers_it_2->second.lines.back().end_col);

                        line_first = -1;
                        line_last  = -1;
                        b_loc_e++;
                    }
                }
            }
            b_col_last += Vb[b_loc];
        }
    }
    #endif

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
    yed_buffer                                  *buffer;
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;

    buffers_it = tree_lookup(ys->buffers, buff_1);
    if (!tree_it_good(buffers_it)) {
        YEXE("buffer", buff_1);
    }

    buffers_it = tree_lookup(ys->buffers, buff_2);
    if (!tree_it_good(buffers_it)) {
        YEXE("buffer", buff_2);
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
                d_buffers_it->second.lines     = vector<buffer_line>();
            }else if (strcmp(buff_2, check_buff) == 0) {
                snprintf(buff, 512, "diff:%s", check_buff);
                d_buffers_it = d_buffers.emplace(string(buff), diff_buff{}).first;
                d_buffers_it->second.buff_name = string(check_buff);
                d_buffers_it->second.buff      = tree_it_val(buffers_it);
                d_buffers_it->second.buff_num  = RIGHT;
                d_buffers_it->second.lines     = vector<buffer_line>();
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
