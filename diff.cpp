extern "C" {
    #include <yed/plugin.h>
}

// TODO:
// --FEATURES
//   add expand and contract feature for truncated lines
//   add the ability to make changes to files
//   maybe add the ability to use the xdiff library instead of the scratch built algs
//
// --OPTIMIZATIONS
//
// --BUGS
//   (I think this is fixed)  fix bug in char diff "highlights the entire right line"

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
#include <unordered_map>
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
#define TRUNC (7)

class file_diff {
    public:
        int type[2];
        int row_num[2];

        file_diff(int t_1, int row_1, int t_2, int row_2) {
            type[0]    = t_1;
            type[1]    = t_2;
            row_num[0] = row_1;
            row_num[1] = row_2;
        }
};

typedef struct line_t {
    int         line_type;
    int         trunc_rows;
    int         is_expanded;
    int         col_begin[2];
    int         col_end[2];
    vector<int> char_type;
} line;

typedef struct diff_main_t {
    yed_buffer_ptr_t  buff_orig[2];
    yed_buffer_ptr_t  buff_diff[2];
    vector<line>      lines;
    vector<int>       line_buff[2];
    vector<file_diff> f_diff;
} diff_main;

diff_main diff_m;

#include "diff_algorithms/myers_diff.hpp"
#include "diff_algorithms/myers_linear_diff.hpp"
#include "diff_algorithms/patience_diff.hpp"

//Base Yed Plugin Functions
static int  get_or_make_buffers(char *buff_1, char *buff_2);
static void unload(yed_plugin *self);
static  int diff_completion(char *name, struct yed_completion_results_t *comp_res);
static  int diff_completion_multiple(char *name, struct yed_completion_results_t *comp_res);
static void clear_buffers(char* b1, char* b2, char* b3, char* b4);

//Diff Functions
static void diff(int n_args, char **args);
static int  init(void);
static int  truncate_lines(int num_cont_eql_rows, int last_eql_row);
static void align_buffers(yed_buffer_ptr_t buff_1, yed_buffer_ptr_t buff_2);
static void diff_adjacent_line(line &tmp_line, int loc);
static void update_diff_buffer(yed_buffer_ptr_t buff_orig, yed_buffer_ptr_t buff_diff);
static void line_base_draw(yed_event *event);
static void line_char_draw(yed_event *event);
static void frame_post_scroll(yed_event *event);

extern "C" {
    int yed_plugin_boot(yed_plugin *self) {
        YED_PLUG_VERSION_CHECK();
int a;
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

    /*     FRAME_POST_SCROLL */
        yed_event_handler frame_post_scroll_eh;
        frame_post_scroll_eh.kind = EVENT_FRAME_POST_SCROLL;
        frame_post_scroll_eh.fn   = frame_post_scroll;
        yed_plugin_add_event_handler(self, frame_post_scroll_eh);

        yed_plugin_set_unload_fn(self, unload);
        yed_plugin_set_command(self, "diff", diff);
        yed_plugin_set_completion(self, "diff-compl", diff_completion);
        yed_plugin_set_completion(self, "diff-compl-arg-0", diff_completion_multiple);
        yed_plugin_set_completion(self, "diff-compl-arg-1", diff_completion_multiple);

        if (yed_get_var("diff-multi-line-compare-algorithm") == NULL) {
            yed_set_var("diff-multi-line-compare-algorithm", "patience");
        }

        if (yed_get_var("diff-line-compare-algorithm") == NULL) {
            yed_set_var("diff-line-compare-algorithm", "myers");
        }

        if (yed_get_var("diff-min-lines-big-file") == NULL) {
            yed_set_var("diff-min-lines-big-file", XSTR(5000));
        }

        if (yed_get_var("diff-multi-line-compare-algorithm-big-file") == NULL) {
            yed_set_var("diff-multi-line-compare-algorithm-big-file", "myers_linear");
        }

        if (yed_get_var("diff-insert-color") == NULL) {
            yed_set_var("diff-insert-color", "&active &blue swap");
        }

        if (yed_get_var("diff-insert-dashes-color") == NULL) {
            yed_set_var("diff-insert-dashes-color", "&active &cyan swap");
        }

        if (yed_get_var("diff-delete-color") == NULL) {
            yed_set_var("diff-delete-color", "&active &blue swap");
        }

        if (yed_get_var("diff-delete-dashes-color") == NULL) {
            yed_set_var("diff-delete-dashes-color", "&active &cyan swap");
        }

        if (yed_get_var("diff-inner-compare-color") == NULL) {
            yed_set_var("diff-inner-compare-color", "&active &magenta swap");
        }

        if (yed_get_var("diff-inner-compare-char-color") == NULL) {
            yed_set_var("diff-inner-compare-char-color", "&active &attention swap");
        }

        if (yed_get_var("diff-trunc-color") == NULL) {
            yed_set_var("diff-trunc-color", "&active &cyan");
        }

        if (yed_get_var("diff-truncate-lines") == NULL) {
            yed_set_var("diff-truncate-lines", "yes");
        }

        if (yed_get_var("diff-truncate-lines-min-num") == NULL) {
            yed_set_var("diff-truncate-lines-min-num", XSTR(12));
        }

        return 0;
    }
}

static int get_or_make_buffers(char *buff_1, char *buff_2) {
    yed_buffer *buffer;
    char        tmp_buff[512];

    buffer = yed_get_buffer_by_path(buff_1);
    if (buffer == NULL) {
        YEXE("buffer-hidden", buff_1);
        buffer = yed_get_buffer_by_path(buff_1);
        if(buffer == NULL) {
            return 1;
        }
    }
    diff_m.buff_orig[LEFT] = buffer;

    buffer = yed_get_buffer_by_path(buff_2);
    if (buffer == NULL) {
        YEXE("buffer-hidden", buff_2);
        buffer = yed_get_buffer_by_path(buff_2);
        if(buffer == NULL) {
            return 1;
        }
    }
    diff_m.buff_orig[RIGHT] = buffer;

    snprintf(tmp_buff, 512, "*diff:%s", diff_m.buff_orig[LEFT]->name);
    buffer = yed_get_buffer(tmp_buff);
    if (buffer == NULL) {
        buffer = yed_create_buffer(tmp_buff);
        buffer->flags |= BUFF_SPECIAL;
    }
    diff_m.buff_diff[LEFT] = buffer;
    update_diff_buffer(diff_m.buff_orig[LEFT], diff_m.buff_diff[LEFT]);

    snprintf(tmp_buff, 512, "*diff:%s", diff_m.buff_orig[RIGHT]->name);
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
    yed_buffer_ptr_t buffer;

    buffer = yed_get_buffer(diff_m.buff_diff[LEFT]->name);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }

    buffer = yed_get_buffer(diff_m.buff_diff[RIGHT]->name);
    if (buffer != NULL) {
        yed_free_buffer(buffer);
    }

    diff_m.lines.clear();
    diff_m.line_buff[LEFT].clear();
    diff_m.line_buff[RIGHT].clear();
    diff_m.f_diff.clear();
}

static int diff_completion(char *name, struct yed_completion_results_t *comp_res) {
    int                                          ret;
    array_t                                      list;
    tree_it(yed_buffer_name_t, yed_buffer_ptr_t) buffers_it;

    ret  = 0;
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
    char *buff_1;
    char *buff_2;
    char *diff_multi_line_var;
    char  tmp_buff_1[512];
    char  tmp_buff_2[512];
    int   min_lines;

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

    yed_get_var_as_int("diff-min-lines-big-file", &min_lines);
    if (yed_buff_n_lines(diff_m.buff_diff[LEFT]) > min_lines || yed_buff_n_lines(diff_m.buff_diff[RIGHT]) > min_lines) {
        diff_multi_line_var = yed_get_var("diff-multi-line-compare-algorithm-big-file");
    } else {
        diff_multi_line_var = yed_get_var("diff-multi-line-compare-algorithm");
    }

    if (strcmp(diff_multi_line_var, "myers") == 0) {
        DBG("myers");
        Myers<vector<int>> myers(diff_m.line_buff[LEFT], diff_m.line_buff[LEFT].size(),
                                    diff_m.line_buff[RIGHT], diff_m.line_buff[RIGHT].size());
        diff_m.f_diff = myers.diff();
    } else if (strcmp(diff_multi_line_var, "myers_linear") == 0) {
        DBG("myers_linear");
        Myers_linear<vector<int>> myers_linear(diff_m.line_buff[LEFT], diff_m.line_buff[LEFT].size(),
                                    diff_m.line_buff[RIGHT], diff_m.line_buff[RIGHT].size());
        diff_m.f_diff = myers_linear.diff();
    } else if (strcmp(diff_multi_line_var, "patience") == 0) {
        DBG("patience");
        Patience<vector<int>> patience(diff_m.line_buff[LEFT], diff_m.line_buff[LEFT].size(),
                                    diff_m.line_buff[RIGHT], diff_m.line_buff[RIGHT].size());
        Slice slice = Slice(0, patience.len_a, 0, patience.len_b);
        diff_m.f_diff = patience.diff(slice);
    }

    if (diff_m.f_diff.size() == 0) {
        yed_cerr("Files were the same!");
        return;
    }

    YEXE("frame-new");
    YEXE("buffer", diff_m.buff_diff[LEFT]->name);
    YEXE("frame-vsplit");
    YEXE("buffer", diff_m.buff_diff[RIGHT]->name);

    align_buffers(diff_m.buff_diff[LEFT], diff_m.buff_diff[RIGHT]);

    diff_m.f_diff.clear();

    LOG_FN_ENTER();
    yed_log("%s, %s", buff_1, buff_2);
    LOG_EXIT();
}

static int init(void) {
    yed_line                              *line;
    unordered_map <string, int>            hashmap;
    unordered_map <string, int>::iterator  hashmap_iter;
    int                                    unique_id;
    string                                 A;
    string                                 B;

    if (diff_m.buff_orig[LEFT] == NULL) {
        yed_cerr("Couldn't find the first diff buffer.");
        return 1;
    }else if (diff_m.buff_orig[RIGHT] == NULL) {
        yed_cerr("Couldn't find the second diff buffer.");
        return 1;
    }

    diff_m.line_buff[LEFT].clear();
    diff_m.line_buff[RIGHT].clear();

    unique_id = 0;

    bucket_array_traverse(diff_m.buff_orig[LEFT]->lines, line) {
        array_zero_term(line->chars);
        A.clear();
        A.append((char *)(line->chars.data));
        A.append("\n");

        hashmap_iter = hashmap.find(A);
        if (hashmap_iter != hashmap.end()) {
            diff_m.line_buff[LEFT].push_back(hashmap_iter->second);
        } else {
            diff_m.line_buff[LEFT].push_back(unique_id);
            hashmap.insert({A, unique_id});
            unique_id++;
        }
    }

    bucket_array_traverse(diff_m.buff_orig[RIGHT]->lines, line) {
        array_zero_term(line->chars);
        B.clear();
        B.append((char *)(line->chars.data));
        B.append("\n");

        hashmap_iter = hashmap.find(B);
        if (hashmap_iter != hashmap.end()) {
            diff_m.line_buff[RIGHT].push_back(hashmap_iter->second);
        } else {
            diff_m.line_buff[RIGHT].push_back(unique_id);
            hashmap.insert({B, unique_id});
            unique_id++;
        }
    }

    return 0;
}

static int expand_truncated_lines(int row) {
    if (diff_m.lines[row].line_type == TRUNC && diff_m.lines[row].is_expanded == 0) {

    }
}

static int truncate_lines(int tot_num_rows, int last_row) {
    int min_rows;
    int first_row;
    int first_remove_row;
    int last_remove_row;
    int tot_num_truncate_rows;

    if (yed_var_is_truthy("diff-truncate-lines")) {
        yed_get_var_as_int("diff-truncate-lines-min-num", &min_rows);
        if (min_rows % 2 != 0) {
            min_rows++;
        }

        if (tot_num_rows > min_rows + 1) {

            first_row             = last_row - tot_num_rows + 1;
            first_remove_row      = last_row - tot_num_rows + (min_rows / 2) + 1;
            tot_num_truncate_rows = tot_num_rows - min_rows;
            last_remove_row       = last_row - (min_rows / 2);

            for (int i = last_remove_row; i >= first_remove_row; i--) {
                yed_buff_delete_line_no_undo(diff_m.buff_diff[LEFT], i);
                yed_buff_delete_line_no_undo(diff_m.buff_diff[RIGHT], i);
            }

            char trunc_str[25];
            snprintf(trunc_str, 25, "------ %d lines: char");
            yed_buff_insert_line_no_undo(diff_m.buff_diff[LEFT], first_remove_row);
            yed_buff_insert_string(diff_m.buff_diff[LEFT], trunc_str, first_remove_row, 1);
            yed_buff_insert_line_no_undo(diff_m.buff_diff[RIGHT], first_remove_row);
            yed_buff_insert_string(diff_m.buff_diff[RIGHT], trunc_str, first_remove_row, 1);

            for (int i = 0; i < min_rows / 2; i++) {
                line tmp_line;
                tmp_line.line_type = EQL;
                diff_m.lines.push_back(tmp_line);
            }

            line tmp_line;
            tmp_line.line_type   = TRUNC;
            tmp_line.trunc_rows  = tot_num_truncate_rows / 2;
            tmp_line.is_expanded = 0;
            diff_m.lines.push_back(tmp_line);

            for (int i = 0; i < min_rows / 2; i++) {
                line tmp_line;
                tmp_line.line_type = EQL;
                diff_m.lines.push_back(tmp_line);
            }

            return tot_num_truncate_rows - 1;
        }
    }

    for (int i = 0; i < tot_num_rows; i++) {
        line tmp_line;
        tmp_line.line_type = EQL;
        diff_m.lines.push_back(std::move(tmp_line));
    }

    return 0;
}

static void align_buffers(yed_buffer_ptr_t buff_1, yed_buffer_ptr_t buff_2) {
    int  save_i;
    int  num_tmp_ins;
    int  num_tmp_del;
    int  left_added_row;
    int  right_added_row;
    int  num_cont_eql_rows;
    int  num_truncated_rows;

    line init_line;
    init_line.line_type = INIT;

//     char dashes[7];
//     memset(dashes, '-', 6);

    diff_m.lines.clear();
    diff_m.lines.push_back(std::move(init_line));

    save_i             = 0;
    num_tmp_ins        = 0;
    num_tmp_del        = 0;
    left_added_row     = 0;
    right_added_row    = 0;
    num_cont_eql_rows  = 0;
    num_truncated_rows = 0;
    for (int i = 0; i < diff_m.f_diff.size(); i++) {

        if (diff_m.f_diff[i].type[LEFT] == INIT) {
            continue;
        }

        save_i = i;
        if (diff_m.f_diff[i].type[RIGHT] == INS) {
            num_tmp_ins++;

            if (num_cont_eql_rows > 0) {
                num_truncated_rows += truncate_lines(num_cont_eql_rows, diff_m.f_diff[i].row_num[RIGHT] + right_added_row - 1 - num_truncated_rows);
                num_cont_eql_rows   = 0;
            }
        } else if (diff_m.f_diff[i].type[LEFT] == DEL) {
            num_tmp_del++;
            if (num_cont_eql_rows > 0) {
                num_truncated_rows += truncate_lines(num_cont_eql_rows, diff_m.f_diff[i].row_num[LEFT] + left_added_row - 1 - num_truncated_rows);
                num_cont_eql_rows   = 0;
            }
        } else {

            if (num_tmp_ins < num_tmp_del) {
                for (int j = num_tmp_ins; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[RIGHT] + right_added_row - j - num_truncated_rows);
                    diff_m.lines.push_back(std::move(tmp_line));
                }

                for (int j = num_tmp_ins; j < num_tmp_del; j++) {
                    line tmp_line;
                    yed_buff_insert_line_no_undo(diff_m.buff_diff[RIGHT],
                        diff_m.f_diff[i].row_num[RIGHT] + right_added_row - num_truncated_rows);
//                     yed_buff_insert_string(diff_m.buff_diff[RIGHT],
//                         dashes, diff_m.f_diff[i].row_num[RIGHT] + right_added_row - num_truncated_rows, 1);
                    right_added_row++;
                    tmp_line.line_type = DEL;
                    diff_m.lines.push_back(std::move(tmp_line));
                }
            } else if (num_tmp_ins > num_tmp_del) {
                for (int j = num_tmp_del; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[LEFT] + left_added_row - j - num_truncated_rows);
                    diff_m.lines.push_back(std::move(tmp_line));
                }

                for (int j = num_tmp_del; j < num_tmp_ins; j++) {
                    line tmp_line;
                    yed_buff_insert_line_no_undo(diff_m.buff_diff[LEFT],
                        diff_m.f_diff[i].row_num[LEFT] + left_added_row - num_truncated_rows);
//                     yed_buff_insert_string(diff_m.buff_diff[LEFT],
//                         dashes, diff_m.f_diff[i].row_num[LEFT] + left_added_row - num_truncated_rows, 1);
                    left_added_row++;
                    tmp_line.line_type = INS;
                    diff_m.lines.push_back(std::move(tmp_line));
                }
            } else {
                for (int j = num_tmp_ins; j > 0; j--) {
                    line tmp_line;
                    tmp_line.line_type = CHG;
                    diff_adjacent_line(tmp_line, diff_m.f_diff[i].row_num[LEFT] + left_added_row - j - num_truncated_rows);
                    diff_m.lines.push_back(std::move(tmp_line));
                }
            }

            num_cont_eql_rows++;

            num_tmp_ins = 0;
            num_tmp_del = 0;
        }
    }

    if (num_tmp_ins < num_tmp_del) {
        for (int j = num_tmp_ins; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row - j - num_truncated_rows);
            diff_m.lines.push_back(std::move(tmp_line));
        }

        for (int j = num_tmp_ins; j < num_tmp_del; j++) {
            line tmp_line;
            yed_buff_insert_line_no_undo(diff_m.buff_diff[RIGHT],
                diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row - num_truncated_rows);
//             yed_buff_insert_string(diff_m.buff_diff[RIGHT],
//                 dashes, diff_m.f_diff[save_i].row_num[RIGHT] + right_added_row - num_truncated_rows, 1);
            right_added_row++;
            tmp_line.line_type = DEL;
            diff_m.lines.push_back(std::move(tmp_line));
        }
    } else if (num_tmp_ins > num_tmp_del) {
        for (int j = num_tmp_del; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j - num_truncated_rows);
            diff_m.lines.push_back(std::move(tmp_line));
        }

        for (int j = num_tmp_del; j < num_tmp_ins; j++) {
            line tmp_line;
            yed_buff_insert_line_no_undo(diff_m.buff_diff[LEFT],
                diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - num_truncated_rows);
//             yed_buff_insert_string(diff_m.buff_diff[LEFT],
//                 dashes, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - num_truncated_rows, 1);
            left_added_row++;
            tmp_line.line_type = INS;
            diff_m.lines.push_back(std::move(tmp_line));
        }
    } else {
        for (int j = num_tmp_ins; j > 0; j--) {
            line tmp_line;
            tmp_line.line_type = CHG;
            diff_adjacent_line(tmp_line, diff_m.f_diff[save_i].row_num[LEFT] + left_added_row - j - num_truncated_rows);
            diff_m.lines.push_back(std::move(tmp_line));
        }
    }
}

static void diff_adjacent_line(line &tmp_line, int loc) {
    char *left;
    char *right;
    char *diff_line_var;
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
        left  = yed_get_line_text(diff_m.buff_diff[LEFT], loc);
        left_len = array_len(yed_buff_get_line(diff_m.buff_diff[LEFT], loc)->chars);
        right = yed_get_line_text(diff_m.buff_diff[RIGHT], loc);
        right_len = array_len(yed_buff_get_line(diff_m.buff_diff[RIGHT], loc)->chars);

        vector<file_diff> tmp_file_diff;
        diff_line_var = yed_get_var("diff-line-compare-algorithm");
        if (strcmp(diff_line_var, "myers") == 0) {
            Myers<string> myers_r(left, left_len, right, right_len);
            tmp_file_diff = myers_r.diff();
        } else if (strcmp(diff_line_var, "myers_linear") == 0) {
            vector<int> left_v(left_len);
            for (int i = 0; i < left_len; i++) {
                left_v[i] = left[i] - '0';
            }

            vector<int> right_v(right_len);
            for (int i = 0; i < right_len; i++) {
                right_v[i] = right[i] - '0';
            }

            Myers_linear<vector<int>> myers_r(left_v, left_v.size(), right_v, right_v.size());
            tmp_file_diff = myers_r.diff();
        } else if (strcmp(diff_line_var, "patience") == 0) {
            vector<int> left_v(left_len);
            for (int i = 0; i < left_len; i++) {
                left_v[i] = left[i] - '0';
            }

            vector<int> right_v(right_len);
            for (int i = 0; i < right_len; i++) {
                right_v[i] = right[i] - '0';
            }

            Patience<vector<int>> myers_r(left_v, left_v.size(), right_v, right_v.size());
            Slice slice = Slice(0, myers_r.len_a, 0, myers_r.len_b);
            tmp_file_diff = myers_r.diff(slice);
        }

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
            } else if (tmp_file_diff[c].type[LEFT] == DEL) {
                tmp_line.char_type.push_back(DEL);
                if (tmp_line.col_begin[LEFT] == 0) {
                    tmp_line.col_begin[LEFT] = tmp_file_diff[c].row_num[LEFT];
                }
                last_col_del = tmp_file_diff[c].row_num[LEFT];
                if (last_eql_right > last_col_ins) {
                    last_col_ins = last_eql_right;
                }
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
            }
        }
        tmp_line.col_end[LEFT]  = last_col_del;
        tmp_line.col_end[RIGHT] = last_col_ins;
    }
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
    char      *color_var;
    yed_attrs  tmp_attr;

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL
    ||  event->row           >= array_len(event->frame->buffer->lines)) {
        return;
    }

    if (event->frame->buffer == diff_m.buff_diff[LEFT]) {
        if (diff_m.lines[event->row].line_type == INS) {
            if ((color_var = yed_get_var("diff-insert-dashes-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == DEL) {
            if ((color_var = yed_get_var("diff-delete-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == CHG) {
            if ((color_var = yed_get_var("diff-inner-compare-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == TRUNC) {
            if ((color_var = yed_get_var("diff-trunc-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else {
            return;
        }

        tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
        if (tmp_line == NULL) { return; }

        event->row_base_attr = tmp_attr;

    }else if (event->frame->buffer == diff_m.buff_diff[RIGHT]) {
        if (diff_m.lines[event->row].line_type == INS) {
            if ((color_var = yed_get_var("diff-insert-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == DEL) {
            if ((color_var = yed_get_var("diff-delete-dashes-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == CHG) {
            if ((color_var = yed_get_var("diff-inner-compare-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
        } else if (diff_m.lines[event->row].line_type == TRUNC) {
            if ((color_var = yed_get_var("diff-trunc-color"))) {
                tmp_attr   = yed_parse_attrs(color_var);
            }
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
    char      *color_var;
    yed_attrs  tmp_attr;
    int        top_row;
    int        top_left_col;
    int        bot_row;
    int        bot_right_col;
    int        left_col;
    int        right_col;

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
                if (diff_m.lines[event->row].char_type.size() < col) {
                    return;
                }

                if (col >= diff_m.lines[event->row].col_begin[LEFT]
                && col <= diff_m.lines[event->row].col_end[LEFT]) {
                    if ((color_var = yed_get_var("diff-inner-compare-char-color"))) {
                        tmp_attr   = yed_parse_attrs(color_var);
                    }

                    if (diff_m.buff_diff[LEFT]->has_selection) {
                        if (diff_m.buff_diff[LEFT]->selection.anchor_row < diff_m.buff_diff[LEFT]->selection.cursor_row) {
                            top_row = diff_m.buff_diff[LEFT]->selection.anchor_row;
                            top_left_col = diff_m.buff_diff[LEFT]->selection.anchor_col;
                            bot_row = diff_m.buff_diff[LEFT]->selection.cursor_row;
                            bot_right_col = diff_m.buff_diff[LEFT]->selection.cursor_col;
                        } else {
                            top_row = diff_m.buff_diff[LEFT]->selection.cursor_row;
                            top_left_col = diff_m.buff_diff[LEFT]->selection.cursor_col;
                            bot_row = diff_m.buff_diff[LEFT]->selection.anchor_row;
                            bot_right_col = diff_m.buff_diff[LEFT]->selection.anchor_col;
                        }

                        if (diff_m.buff_diff[LEFT]->selection.anchor_col < diff_m.buff_diff[LEFT]->selection.cursor_col) {
                            left_col = diff_m.buff_diff[LEFT]->selection.anchor_col;
                            right_col = diff_m.buff_diff[LEFT]->selection.cursor_col;
                        } else {
                            left_col = diff_m.buff_diff[LEFT]->selection.cursor_col;
                            right_col = diff_m.buff_diff[LEFT]->selection.anchor_col;
                        }

                        if (top_row <= event->row && event->row <= bot_row) {
                            if (top_row < event->row && event->row < bot_row) {
                                continue;
                            } else if (event->row == top_row && top_row != bot_row && col >= top_left_col) {
                                continue;
                            } else if (event->row == bot_row && top_row != bot_row && col < bot_right_col) {
                                continue;
                            } else if (event->row == top_row && event->row == bot_row && left_col <= col && col < right_col) {
                                continue;
                            }
                        }
                    }
                    yed_eline_combine_col_attrs(event, col, &tmp_attr);
                }
            }
        }

    } else if (event->frame->buffer == diff_m.buff_diff[RIGHT]) {
        if (diff_m.lines[event->row].line_type == CHG) {
            tmp_line = yed_buff_get_line(event->frame->buffer, event->row);
            if (tmp_line == NULL) { return; }

            for (int col = 1; col <= tmp_line->visual_width; col++) {
                if (diff_m.lines[event->row].char_type.size() < col) {
                    return;
                }

                if (col >= diff_m.lines[event->row].col_begin[RIGHT]
                && col <= diff_m.lines[event->row].col_end[RIGHT]) {
                    if ((color_var = yed_get_var("diff-inner-compare-char-color"))) {
                        tmp_attr   = yed_parse_attrs(color_var);
                    }

                    if (diff_m.buff_diff[RIGHT]->has_selection) {
                        if (diff_m.buff_diff[RIGHT]->selection.anchor_row < diff_m.buff_diff[RIGHT]->selection.cursor_row) {
                            top_row = diff_m.buff_diff[RIGHT]->selection.anchor_row;
                            top_left_col = diff_m.buff_diff[RIGHT]->selection.anchor_col;
                            bot_row = diff_m.buff_diff[RIGHT]->selection.cursor_row;
                            bot_right_col = diff_m.buff_diff[RIGHT]->selection.cursor_col;
                        } else {
                            top_row = diff_m.buff_diff[RIGHT]->selection.cursor_row;
                            top_left_col = diff_m.buff_diff[RIGHT]->selection.cursor_col;
                            bot_row = diff_m.buff_diff[RIGHT]->selection.anchor_row;
                            bot_right_col = diff_m.buff_diff[RIGHT]->selection.anchor_col;
                        }

                        if (diff_m.buff_diff[RIGHT]->selection.anchor_col < diff_m.buff_diff[RIGHT]->selection.cursor_col) {
                            left_col = diff_m.buff_diff[RIGHT]->selection.anchor_col;
                            right_col = diff_m.buff_diff[RIGHT]->selection.cursor_col;
                        } else {
                            left_col = diff_m.buff_diff[RIGHT]->selection.cursor_col;
                            right_col = diff_m.buff_diff[RIGHT]->selection.anchor_col;
                        }

                        if (top_row <= event->row && event->row <= bot_row) {
                            if (top_row < event->row && event->row < bot_row) {
                                continue;
                            } else if (event->row == top_row && top_row != bot_row && col >= top_left_col) {
                                continue;
                            } else if (event->row == bot_row && top_row != bot_row && col < bot_right_col) {
                                continue;
                            } else if (event->row == top_row && event->row == bot_row && left_col <= col && col < right_col) {
                                continue;
                            }
                        }
                    }
                    yed_eline_combine_col_attrs(event, col, &tmp_attr);
                }
            }
        }
    }
}

bool scrolling_others = false;

static void frame_post_scroll(yed_event *event) {
    yed_frame_tree *p;
    yed_frame_tree *other_child;
    int             current_row;

    if (scrolling_others) {
        return;
    }

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

    if (event->frame->tree->parent->child_trees[0]->frame->buffer != diff_m.buff_diff[LEFT]
    &&  event->frame->tree->parent->child_trees[1]->frame->buffer != diff_m.buff_diff[RIGHT]) {
        return;
    }

    p           = event->frame->tree->parent;
    other_child = p->child_trees[event->frame->tree == p->child_trees[0]];

    scrolling_others = true;
    yed_frame_scroll_buffer(other_child->frame, event->frame->buffer_y_offset - other_child->frame->buffer_y_offset);
    scrolling_others = false;
}
