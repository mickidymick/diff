#ifndef MyersPatienceDiff_HPP
#define MyersPatienceDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

#define not_empty ((slice.a_low < slice.a_high) && (slice.b_low < slice.b_high))

class Match {
    public:
        Match *prev;
        Match *next;
        int    a_line;
        int    b_line;

        Match(int a, int b, Match *p, Match *n) {
            prev   = p;
            next   = n;
            a_line = a;
            b_line = b;
        }
};

class Slice {
    public:
        int a_low;
        int a_high;
        int b_low;
        int b_high;

        Slice(int a_l, int a_h, int b_l, int b_h) {
            a_low  = a_l;
            a_high = a_h;
            b_low  = b_l;
            b_high = b_h;
        }
};

template <class T>
class Patience {
    public:
        T                 a;
        T                 b;
        int               len_a;
        int               len_b;
        int               max;
        vector<file_diff> f_diff;

        Patience(T &a_t, int len_a_t, T &b_t, int len_b_t) {
            a     = a_t;
            b     = b_t;
            len_a = len_a_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        vector<file_diff> fallback(Slice slice) {
            vector<int> tmp_a;
            vector<int> tmp_b;
            vector<file_diff> f_diff;

            for(int i = slice.a_low; i < slice.a_high; i++) {
                tmp_a.push_back(a[i]);
            }

            for(int i = slice.b_low; i < slice.b_high; i++) {
                tmp_b.push_back(b[i]);
            }

            Myers_linear<vector<int>> myers_linear(tmp_a, tmp_a.size(), tmp_b, tmp_b.size());
            f_diff = myers_linear.diff();

            for (int i = 0; i < f_diff.size(); i++) {
                f_diff[i].row_num[LEFT]  += slice.a_low;
                f_diff[i].row_num[RIGHT] += slice.b_low;
            }

            return f_diff;
        }

        int binary_search(vector<Match *> &stacks, Match *match) {
            int low;
            int mid;
            int high;

            low   = -1;
            high = stacks.size();

            while(low + 1 < high) {
                mid = (low + high) / 2;

                if (stacks[mid]->b_line < match->b_line) {
                    low = mid;
                } else {
                    high = mid;
                }
            }

            return low;
        }

        Match *patience_sort(Match *matches) {
            Match           *match;
            vector<Match *>  stacks;
            int              i;

            for(match = matches; match != NULL; match = match->next) {
                i = binary_search(stacks, match);

                if (i >= 0) {
                    match->prev = stacks[i];
                }

                if (i + 1 >= stacks.size()) {
                    stacks.insert(stacks.begin() + i + 1, match);
                } else {
                    stacks[i + 1] = match;
                }
            }

            if (stacks.size() == 0) {
                return NULL;
            }

            match = stacks.back();

            if (match == NULL) {
                match->next = NULL;
                return NULL;
            }

            match->next = NULL;
            while(match->prev != NULL) {
                match->prev->next = match;
                match             = match->prev;
            }

            return match;
        }

        vector<Match *> match_ptrs;

        Match *unique_matching_lines(Slice slice) {
            Match                                              *match;
            Match                                              *last_match;
            Match                                              *tmp_match;
            int                                                 counts;
            unordered_map <int, vector<int> *>                    hashmap;
            unordered_map <int, vector<int> *>::iterator          hashmap_iter;
            vector<vector<int> *> order;

            for(int i = slice.a_low; i < slice.a_high; i++) {
                hashmap_iter = hashmap.find(a[i]);
                if (hashmap_iter == hashmap.end()) {
                    vector<int> *tmp = new vector<int>{1, 0, i, -1};
                    hashmap.insert({a[i], tmp});
                    order.push_back(tmp);
                } else {
                    hashmap_iter->second->at(0) += 1;
                    if (hashmap_iter->second->at(2) == -1) {
                        hashmap_iter->second->at(2) = i;
                    }
                }
            }

            for(int i = slice.b_low; i < slice.b_high; i++) {
                hashmap_iter = hashmap.find(b[i]);
                if (hashmap_iter == hashmap.end()) {
                    vector<int> *tmp = new vector<int>{0, 1, -1, i};
                    hashmap.insert({b[i], tmp});
                    order.push_back(tmp);
                } else {
                    hashmap_iter->second->at(1) += 1;
                    if (hashmap_iter->second->at(3) == -1) {
                        hashmap_iter->second->at(3) = i;
                    }
                }
            }

            match = NULL;
            for(int i = 0; i < order.size(); i++) {
                if (order[i]->at(0) == 1 && order[i]->at(1) == 1) {
                    if (match == NULL) {
                        tmp_match = new Match(order[i]->at(2), order[i]->at(3), NULL, NULL);
                        match_ptrs.push_back(tmp_match);
                        match = tmp_match;
                    } else {
                        tmp_match = new Match(order[i]->at(2), order[i]->at(3), last_match, NULL);
                        match_ptrs.push_back(tmp_match);
                        last_match->next = tmp_match;
                    }
                    last_match = tmp_match;
                }
            }

            for(hashmap_iter = hashmap.begin(); hashmap_iter != hashmap.end(); hashmap_iter++) {
                delete hashmap_iter->second;
            }

            return match;
        }

        vector<file_diff> match_head(Slice &slice) {
            vector<file_diff> f_diff;

            while (((slice.a_low < slice.a_high) && (slice.b_low < slice.b_high))
                    && a[slice.a_low] == b[slice.b_low]) {
                f_diff.emplace_back(EQL, slice.a_low + 1, EQL, slice.b_low + 1);
                slice.a_low++;
                slice.b_low++;
            }

            return f_diff;
        }

        vector<file_diff> match_tail(Slice &slice) {
            vector<file_diff> f_diff;

            while (((slice.a_low < slice.a_high) && (slice.b_low < slice.b_high))
                    && a[slice.a_high - 1] == b[slice.b_high - 1]) {
                slice.a_high--;
                slice.b_high--;
                f_diff.emplace_back(EQL, slice.a_high + 1, EQL, slice.b_high + 1);
            }

            return f_diff;
        }

        vector<file_diff> diff(Slice &slice) {
            Match             *match;
            int                a_line;
            int                b_line;
            int                a_next;
            int                b_next;
            vector<file_diff>  f_diff;
            vector<file_diff>  tmp_f_diff;

            match = patience_sort(unique_matching_lines(slice));

            if (match == NULL) {
                return fallback(slice);
            }

            int i = 0;
            for(Match *m = match; m != NULL; m = m->next) {
                i++;
            }

            a_line = slice.a_low;
            b_line = slice.b_low;

            while(1) {
                a_next = match != NULL ? match->a_line : slice.a_high;
                b_next = match != NULL ? match->b_line : slice.b_high;

                Slice subslice = Slice(a_line, a_next, b_line, b_next);

//                 tmp_f_diff = match_head(subslice);
//                 for(int i = 0; i < tmp_f_diff.size(); i++) {
//                     f_diff.push_back(tmp_f_diff[i]);
//                 }

                tmp_f_diff = diff(subslice);
                for(int i = 0; i < tmp_f_diff.size(); i++) {
                    f_diff.push_back(tmp_f_diff[i]);
                }

//                 tmp_f_diff = match_tail(subslice);
//                 for(int i = 0; i < tmp_f_diff.size(); i++) {
//                     f_diff.push_back(tmp_f_diff[i]);
//                 }

                if (match == NULL) {
                    return f_diff;
                }

                f_diff.emplace_back(EQL, match->a_line + 1, EQL, match->b_line + 1);

                a_line = match->a_line + 1;
                b_line = match->b_line + 1;

                match  = match->next;
            }

            for(int i = 0; i < match_ptrs.size(); i++) {
                delete match_ptrs[i];
            }

            return f_diff;
        }
};

#endif
