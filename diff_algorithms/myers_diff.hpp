#ifndef MyersDiff_HPP
#define MyersDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

#define wrap_indx(size, indx) (indx < 0 ? size + indx : indx)

template <class T>
class Myers {
    public:
        T                 a;
        T                 b;
        int               len_a;
        int               len_b;
        int               max;
        vector<file_diff> f_diff;

        Myers(T a_t, int len_a_t, T b_t, int len_b_t) {
            a     = a_t;
            b     = b_t;
            len_a = len_a_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        //Finds the Shortest Edit Path
        vector<vector<int>> shortest_edit(void) {
            int                 x;
            int                 y;
            int                 size;
            vector<vector<int>> trace;

            size = 2 * max + 1;

            vector<int> v(size);
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

                    while (x < len_a && y < len_b && a[x] == b[y]) {
                        x++;
                        y++;
                    }

                    v[wrap_indx(size, k)] = x;

                    if (x >= len_a
                        && y >= len_b) {
                        return trace;
                    }
                }
            }
        }

        void backtrack_yield(int prev_x, int prev_y, int x, int y) {
            if (x == prev_x) {
//                 file_diff tmp_line_diff(INS, prev_x + 1, INS, prev_y + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.emplace(f_diff.begin(), INS, prev_x + 1, INS, prev_y + 1);
            } else if (y == prev_y) {
//                 file_diff tmp_line_diff(DEL, prev_x + 1, DEL, prev_y + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.emplace(f_diff.begin(), DEL, prev_x + 1, DEL, prev_y + 1);
            } else {
//                 file_diff tmp_line_diff(EQL, prev_x + 1, EQL, prev_y + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.emplace(f_diff.begin(), EQL, prev_x + 1, EQL, prev_y + 1);
            }
        }

        //Backtracks the shortest edit path finiding the path
        void backtrack(vector<vector<int>> &trace, int x, int y) {
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

            backtrack(se, len_a, len_b);

            return f_diff;
        }
};

#endif
