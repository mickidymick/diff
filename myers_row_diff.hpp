#ifndef MyersRowDiff_HPP
#define MyersRowDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

class Myers_row {
    public:
        int               len[2];
        int               max;
        vector<file_diff> f_diff;

        Myers_row(int len_a, int len_b) {
            len[LEFT]  = len_a;
            len[RIGHT] = len_b;
            max        = len_a + len_b;
        }

        // Member Functions
        int wrap_indx(int size, int indx) {
            if ( indx < 0 ) {
                return (size + indx);
            }

            return indx;
        }

        char *yed_get_line_text_tmp(yed_buffer *buffer, int row) {
            yed_line *line;

            line = yed_buff_get_line(buffer, row);

            if (line == NULL) { return NULL; }

            array_zero_term(line->chars);

            return line->chars.data;
        }

        int yed_get_line_text_len(yed_buffer *buffer, int row) {
            yed_line *line;

            line = yed_buff_get_line(buffer, row);

            if (line == NULL) { return NULL; }

            return array_len(line->chars);
        }

        //Finds the Shortest Edit Path
        vector<vector<int>> shortest_edit(void) {
            int                 x;
            int                 y;
            int                 size;
            int                 tot = 0;
            vector<vector<int>> trace;

            size = 2 * max + 1;

            vector<int> v(2 * max + 1);
            v[1] = 0;

            for(int d = 0; d <= max; d++) {
                trace.push_back(v);
                for(int k = -d; k <= d; k+=2) {
                    if (k == -d || (k != d
                        && v[wrap_indx(size, k - 1)] < v[wrap_indx(size, k + 1)])) {
                        x = v[wrap_indx(size, k + 1)];
                    } else {
                        x = v[wrap_indx(size, k - 1)] + 1;
                    }

                    y = x - k;

                    while ((x + 1) < len[LEFT] && (y + 1) < len[RIGHT]
                    && yed_get_line_text_len(diff_m.buff_orig[LEFT], x + 1) == yed_get_line_text_len(diff_m.buff_orig[RIGHT], y + 1)
                    && (strcmp(yed_get_line_text_tmp(diff_m.buff_orig[LEFT], x + 1),
                               yed_get_line_text_tmp(diff_m.buff_orig[RIGHT], y + 1)) == 0)) {
                               x++;
                               y++;
                               tot++;
                    }

                    v[wrap_indx(size, k)] = x;

                    if ((x + 1) >= len[LEFT]
                        && (y + 1) >= len[RIGHT]) {
                        DBG("total: %d", tot);
                        return trace;
                    }
                }
            }
        }

        void backtrack_yield(int prev_x, int prev_y, int x, int y) {
            if (x == prev_x) {
                file_diff tmp_line_diff(INS, prev_x + 1, INS, prev_y + 1);
                f_diff.insert(f_diff.begin(), tmp_line_diff);
            } else if (y == prev_y) {
                file_diff tmp_line_diff(DEL, prev_x + 1, DEL, prev_y + 1);
                f_diff.insert(f_diff.begin(), tmp_line_diff);
            } else {
                file_diff tmp_line_diff(EQL, prev_x + 1, EQL, prev_y + 1);
                f_diff.insert(f_diff.begin(), tmp_line_diff);
            }
        }

        //Backtracks the shortest edit path finiding the path
        void backtrack(vector<vector<int>> trace, int x, int y) {
            int         size;
            int         k;
            int         prev_k;
            int         prev_x;
            int         prev_y;
            vector<int> v;

            size = 2 * max + 1;

            for (int d = trace.size() - 1; d >= 0; d--) {

                v = trace[d];
                k = x - y;

                if ( k == -d || (k != d
                    && v[wrap_indx(size, k - 1)] < v[wrap_indx(size, k + 1)])) {
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
        vector<file_diff> diff(void) {

            vector<vector<int>> se = shortest_edit();

            backtrack(se, len[LEFT], len[RIGHT]);

            return f_diff;
        }
};

#endif
