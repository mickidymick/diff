#ifndef MyersDiff_HPP
#define MyersDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

template <class T>
class Myers {
    public:
        T   a;
        int len_a;
        T   b;
        int len_b;
        int max;

        Myers(T a_t, int len_a_t, T b_t, int len_b_t) {
            a     = a_t;
            len_a = len_a_t;
            b     = b_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        // Member Functions
        int wrap_indx(int size, int indx) {
            if ( indx < 0 ) {
                return (size + indx);
            }

            return indx;
        }

        //Finds the Shortest Edit Path
        vector<vector<int>> shortest_edit(void) {
            int tmp_k;
            int tmp_k_add;
            int tmp_k_min;
            int x;
            int y;
            int size;

            size = 2 * max + 1;
            type_arr.resize(size);

            vector<int> v(2 * max + 1);
            v[1] = 0;
            vector<vector<int>> trace;

            int tot = 0;

            for(int d = 0; d <= max; d++) {
                trace.push_back(v);
                tot++;
                for(int k = -d; k <= d; k+=2) {
                    if (k == -d || (k != d && v[wrap_indx(size, k - 1)] < v[wrap_indx(size, k + 1)])) {
                        x = v[wrap_indx(size, k + 1)];
                    } else {
                        x = v[wrap_indx(size, k - 1)] + 1;
                    }

                    y = x - k;

                    while (x < len_a &&
                           y < len_b &&
                           a[x] == b[y]) {
                               x++;
                               y++;
                    }

                    v[wrap_indx(size, k)] = x;

                    if (x >= len_a &&
                        y >= len_b) {
                        return trace;
                    }
                }
            }
        }

        void backtrack_yield(int prev_x, int prev_y, int x, int y) {
            string a_line;
            string b_line;

            if (prev_x >= 0 && prev_x < len_a) {
                a_line = a[prev_x];
            }
            if (prev_y >= 0 && prev_y < len_b) {
                b_line = b[prev_y];
            }

            if (x == prev_x) {
                Line_diff tmp_line_diff(INS, prev_x + 1, "", prev_y + 1, b_line);
                l_diff.insert(l_diff.begin(), tmp_line_diff);
                type_arr[prev_y + 1].b = INS;
            } else if (y == prev_y) {
                Line_diff tmp_line_diff(DEL, prev_x + 1, a_line, prev_y + 1, "");
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
        int diff(void) {

            vector<vector<int>> se = shortest_edit();

            backtrack(se, len_a, len_b);

            return 0;
        }
};

#endif