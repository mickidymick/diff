#ifndef MyersLinearDiff_HPP
#define MyersLinearDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

#include <cmath>

class Box {
    public:
        int left;
        int right;
        int top;
        int bottom;

        Box(int left_t, int top_t, int right_t, int bottom_t) {
            left   = left_t;
            right  = right_t;
            top    = top_t;
            bottom = bottom_t;
        }

        int width(void) {
            return right - left;
        }

        int height(void) {
            return bottom - top;
        }

        int size(void) {
            return width() + height();
        }

        int delta(void) {
            return width() - height();
        }
};

typedef vector<vector<int>> Snake;
typedef vector<vector<int>> Block;

template <class T>
class Myers_linear {
    public:
        T                 a;
        T                 b;
        int               len_a;
        int               len_b;
        int               max;
        vector<file_diff> f_diff;

        Myers_linear(T a_t, int len_a_t, T b_t, int len_b_t) {
            a     = a_t;
            b     = b_t;
            len_a = len_a_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        int wrap_indx(int size, int indx) {
            if ( indx < 0 ) {
                return (size + indx);
            }

            return indx;
        }

        Snake forwards(Box &box, vector<int> &vf, vector<int> &vb, int &d) {
            int   c;
            int   x;
            int   y;
            int   px;
            int   py;
            Snake snake;

            for (int k = d; k >= -d; k-=2) {
                c = k - box.delta();

                if (k == -d || (k != d
                    && vf[wrap_indx(vf.size(), k - 1)] < vf[wrap_indx(vf.size(), k + 1)])) {
                    x  = vf[wrap_indx(vf.size(), k + 1)];
                    px = x;
                } else {
                    px = vf[wrap_indx(vf.size(), k - 1)];
                    x  = px + 1;
                }

                y  = box.top + (x - box.left) - k;
                py = (d == 0 || x != px) ? y : y - 1;

                while (x < box.right
                    && y < box.bottom
                    && a[wrap_indx(len_a, x)] == b[wrap_indx(len_b, y)]) {
                    x++;
                    y++;
                }

                vf[wrap_indx(vf.size(), k)] = x;

                if (abs((box.delta() % 2)) == 1
                    && (-(d - 1) <= c && c <= d - 1)
                    && y >= vb[wrap_indx(vb.size(), c)]) {
                    snake = Snake {vector<int> {px, py}, vector<int> {x, y}};
                    return snake;
                }
            }

            return snake;
        }

        Snake backwards(Box &box, vector<int> &vf, vector<int> &vb, int &d) {
            int   k;
            int   x;
            int   y;
            int   px;
            int   py;
            Snake snake;

            for (int c = d; c >= -d; c-=2) {
                k = c + box.delta();

                if (c == -d || (c != d
                    && vb[wrap_indx(vb.size(), c - 1)] > vb[wrap_indx(vb.size(), c + 1)])) {
                    y  = vb[wrap_indx(vb.size(), c + 1)];
                    py = y;
                } else {
                    py = vb[wrap_indx(vb.size(), c - 1)];
                    y  = py - 1;
                }

                x  = box.left + (y - box.top) + k;
                px = (d == 0 || y != py) ? x : x + 1;

                while (x > box.left
                    && y > box.top
                    && a[wrap_indx(len_a, x - 1)] == b[wrap_indx(len_b, y - 1)]) {
                    x--;
                    y--;
                }

                vb[wrap_indx(vb.size(), c)] = y;

                if ((box.delta() % 2) == 0
                    && (-d <= k && k <= d)
                    && x <= vf[wrap_indx(vf.size(), k)]) {
                    snake = Snake {vector<int> {x, y}, vector<int> {px, py}};
                    return snake;
                }
            }

            return snake;
        }

        Snake midpoint(Box box) {
            int   tmp_max;
            Snake snake;
            Snake tmp_snake;

            if (box.size() == 0) {
                return snake;
            }

            tmp_max = ceil(box.size() / 2.0);

            vector<int> vf(2 * tmp_max + 1);
            vf[1] = box.left;
            vector<int> vb(2 * tmp_max + 1);
            vb[1] = box.bottom;

            for (int d = 0; d <= tmp_max; d++) {
                tmp_snake = forwards(box, vf, vb, d);
                if (tmp_snake.size() > 0) {
                    return tmp_snake;
                }

                tmp_snake = backwards(box, vf, vb, d);
                if (tmp_snake.size() > 0) {
                    return tmp_snake;
                }
            }

            return snake;
        }

        vector<vector<int>> find_path(int left, int top, int right, int bottom) {
            Snake       snake;
            Snake       ret_snake;
            Snake       head;
            Snake       tail;
            vector<int> start;
            vector<int> finish;

            Box box = Box(left, top, right, bottom);
            snake   = midpoint(box);

            if (snake.size() == 0) {
                return snake;
            }


            start  = snake[0];
            finish = snake[1];

            head = find_path(box.left, box.top, start[0], start[1]);
            tail = find_path(finish[0], finish[1], box.right, box.bottom);

            if (head.size() > 0) {
                for (int i = 0; i < head.size(); i++) {
                    ret_snake.push_back(head[i]);
                }
            } else {
                ret_snake.push_back(start);
            }

            if (tail.size() > 0) {
                for (int i = 0; i < tail.size(); i++) {
                    ret_snake.push_back(tail[i]);
                }
            } else {
                ret_snake.push_back(finish);
            }

            return ret_snake;
        }

        void add_to_diff(int x1, int y1, int x2, int y2, vector<file_diff> &f_diff) {
            if (x1 >= len_a && y1 >= len_b) {
                return;
            }

            if (x1 == x2) {
                file_diff tmp_line_diff(INS, x1 + 1, INS, y1 + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.push_back(tmp_line_diff);
                DBG("INS %d INS %d", x1 + 1, y1 + 1);
            } else if (y1 == y2) {
                file_diff tmp_line_diff(DEL, x1 + 1, DEL, y1 + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.push_back(tmp_line_diff);
                DBG("DEL %d DEL %d", x1 + 1, y1 + 1);
            } else {
                file_diff tmp_line_diff(EQL, x1 + 1, EQL, y1 + 1);
//                 f_diff.insert(f_diff.begin(), tmp_line_diff);
                f_diff.push_back(tmp_line_diff);
                DBG("EQL %d EQL %d", x1 + 1, y1 + 1);
            }
        }

        vector<int> walk_diagonal(int &x1, int &y1, int &x2, int &y2, vector<file_diff> &f_diff) {
            vector<int> tmp_arr(2);

            while (x1 < x2 && y1 < y2 && a[wrap_indx(len_a, x1)] == b[wrap_indx(len_b, y1)]) {
                add_to_diff(x1, y1, x1 + 1, y1 + 1, f_diff);
                x1++;
                y1++;
            }

            tmp_arr[0] = x1;
            tmp_arr[1] = y1;
            return tmp_arr;
        }

        vector<file_diff> diff(void) {
            int               x1;
            int               y1;
            int               x2;
            int               y2;
            Snake             path;
            vector<int>       tmp_arr(2);
            vector<file_diff> f_diff;

            path = find_path(0, 0, len_a, len_b);

            if (path.size() == 0) {
                return f_diff;
            }

            for (int i = 0; i < path.size(); i++) {
                yed_log("[%d, %d]\n", path[i][0], path[i][1]);
            }

            for (int i = 0; i < path.size(); i+=2) {
                x1 = path[i][0];
                y1 = path[i][1];
                x2 = path[i+1][0];
                y2 = path[i+1][1];

                tmp_arr = walk_diagonal(x1, y1, x2, y2, f_diff);
                x1 = tmp_arr[0];
                y1 = tmp_arr[1];

                if ((x2 - x1) < (y2 - y1)) {
                    add_to_diff(x1, y1, x1, y1 + 1, f_diff);
                    y1++;
                } else if ((x2 - x1) > (y2 - y1)) {
                    add_to_diff(x1, y1, x1 + 1, y1, f_diff);
                    x1++;
                }

                walk_diagonal(x1, y1, x2, y2, f_diff);
            }

//             f_diff.clear();
            return f_diff;
        }
};

#endif
