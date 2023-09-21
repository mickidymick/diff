#ifndef MyersLinearDiff_HPP
#define MyersLinearDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

#define wrap_indx(size, indx) (indx < 0 ? size + indx : indx)
#define WIDTH  (box.right - box.left)
#define HEIGHT (box.bottom - box.top)
#define SIZE   (WIDTH + HEIGHT)
#define DELTA  (WIDTH - HEIGHT)

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
};

class Point {
    public:
        int x = 0;
        int y = 0;

        Point(int x_1, int y_1) {
            x = x_1;
            y = y_1;
        }
};

class Snake {
    public:
        Point start;
        Point end;

        Snake(int x_1, int y_1, int x_2, int y_2):
            start(x_1, y_1),
            end(x_2, y_2)
        {
        }
};

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

        Myers_linear(T &a_t, int len_a_t, T &b_t, int len_b_t) {
            a     = a_t;
            b     = b_t;
            len_a = len_a_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        Snake *forwards(Box &box, vector<int> &vf, vector<int> &vb, int &d, int &len) {
            int c;
            int x;
            int y;
            int px;
            int py;
            int k_p;
            int k_m;

            for (int k = d; k >= -d; k-=2) {
                c = k - DELTA;

                k_p = wrap_indx(len, k + 1);
                k_m = wrap_indx(len, k - 1);

                if (k == -d || (k != d
                    && vf[k_m] < vf[k_p])) {
                    x  = vf[k_p];
                    px = x;
                } else {
                    px = vf[k_m];
                    x  = px + 1;
                }

                y  = box.top + (x - box.left) - k;
                py = (d == 0 || x != px) ? y : y - 1;

                while (x < box.right
                    && y < box.bottom
                    && a[x] == b[y]) {
                    x++;
                    y++;
                }

                vf[wrap_indx(len, k)] = x;

                if (((DELTA) & 1)
                    && (-(d - 1) <= c && c <= d - 1)
                    && y >= vb[wrap_indx(len, c)]) {
                    return new Snake(px, py, x, y);
                }
            }

            return NULL;
        }

        Snake *backwards(Box &box, vector<int> &vf, vector<int> &vb, int &d, int &len) {
            int k;
            int x;
            int y;
            int px;
            int py;
            int k_p;
            int k_m;

            for (int c = d; c >= -d; c-=2) {
                k = c + DELTA;

                k_p = wrap_indx(len, c + 1);
                k_m = wrap_indx(len, c - 1);

                if (c == -d || (c != d
                    && vb[k_m] > vb[k_p])) {
                    y  = vb[k_p];
                    py = y;
                } else {
                    py = vb[k_m];
                    y  = py - 1;
                }

                x  = box.left + (y - box.top) + k;
                px = (d == 0 || y != py) ? x : x + 1;

                while (x > box.left
                    && y > box.top
                    && a[x - 1] == b[y - 1]) {
                    x--;
                    y--;
                }

                vb[wrap_indx(len, c)] = y;

                if ((!((DELTA) & 1))
                    && (-d <= k && k <= d)
                    && x <= vf[wrap_indx(len, k)]) {
                    return new Snake(x, y, px, py);
                }
            }

            return NULL;
        }

        Snake *midpoint(Box &box) {
            Snake *snake;
            int    tmp_max;
            int    len;

            if (SIZE == 0) {
                return NULL;
            }

            tmp_max = (int)(SIZE / 2.0) + 1;
            len     = 2 * tmp_max + 1;

            vector<int> vf(len);
            vector<int> vb(len);
            vf[1] = box.left;
            vb[1] = box.bottom;

            for (int d = 0; d <= tmp_max; d++) {
                snake = forwards(box, vf, vb, d, len);
                if (snake != NULL) {
                    return snake;
                }

                snake = backwards(box, vf, vb, d, len);
                if (snake != NULL) {
                    return snake;
                }
            }

            return NULL;
        }

        vector<Point> *find_path(int left, int top, int right, int bottom) {
            Snake         *snake;
            vector<Point> *ret_snake;
            vector<Point> *head;
            vector<Point> *tail;

            Box box = Box(left, top, right, bottom);
            snake   = midpoint(box);

            if (snake == NULL) {
                return NULL;
            }

            head = find_path(box.left, box.top, snake->start.x, snake->start.y);
            tail = find_path(snake->end.x, snake->end.y, box.right, box.bottom);

            ret_snake = new vector<Point>;

            if (head != NULL) {
                for (int i = 0; i < head->size(); i++) {
                    ret_snake->emplace_back(head->at(i).x, head->at(i).y);
                }
            } else {
                ret_snake->emplace_back(snake->start.x, snake->start.y);
            }

            if (tail != NULL) {
                for (int i = 0; i < tail->size(); i++) {
                    ret_snake->emplace_back(tail->at(i).x, tail->at(i).y);
                }
            } else {
                ret_snake->emplace_back(snake->end.x, snake->end.y);
            }

            return ret_snake;
        }

        void add_to_diff(int x1, int y1, int x2, int y2, vector<file_diff> &f_diff) {
            if (x1 >= len_a && y1 >= len_b) {
                return;
            }

            if (x1 == x2) {
                f_diff.emplace_back(INS, x1 + 1, INS, y1 + 1);
            } else if (y1 == y2) {
                f_diff.emplace_back(DEL, x1 + 1, DEL, y1 + 1);
            } else {
                f_diff.emplace_back(EQL, x1 + 1, EQL, y1 + 1);
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
            vector<Point>    *path;
            int               x1;
            int               y1;
            int               x2;
            int               y2;
            int               path_len;
            vector<int>       tmp_arr(2);
            vector<file_diff> f_diff;

            path = find_path(0, 0, len_a, len_b);

            if (path == NULL) {
                return f_diff;
            }

            path_len = path->size();

            if (path_len == 0) {
                return f_diff;
            }

            for (int i = 0; i < path_len - 1; i++) {
                x1 = (*path)[i].x;
                y1 = (*path)[i].y;
                x2 = (*path)[i+1].x;
                y2 = (*path)[i+1].y;

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

            return f_diff;
        }
};

#endif
