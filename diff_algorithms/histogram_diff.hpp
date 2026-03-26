#ifndef HistogramDiff_HPP
#define HistogramDiff_HPP

extern "C" {
    #include <yed/plugin.h>
}

// Histogram diff: a generalization of patience diff that uses the lowest-frequency
// common lines as anchors rather than requiring uniqueness. When all common lines
// are unique (min_count == 1), behavior is identical to patience. When repeated
// lines are the only common anchors, histogram finds them where patience would fall
// back directly to Myers. This produces better diffs for code with repeated
// structural lines (braces, blank lines, short keywords, etc.).
//
// Shares Match, Slice, and LineEntry with patience_diff.hpp / diff.cpp.

template <class T>
class Histogram {
    public:
        T                 a;
        T                 b;
        int               len_a;
        int               len_b;
        int               max;

        Histogram(T &a_t, int len_a_t, T &b_t, int len_b_t) {
            a     = a_t;
            b     = b_t;
            len_a = len_a_t;
            len_b = len_b_t;
            max   = len_a + len_b;
        }

        vector<file_diff> fallback(Slice slice) {
            vector<int>       tmp_a;
            vector<int>       tmp_b;
            vector<file_diff> f_diff;

            for (int i = slice.a_low; i < slice.a_high; i++) {
                tmp_a.push_back(a[i]);
            }
            for (int i = slice.b_low; i < slice.b_high; i++) {
                tmp_b.push_back(b[i]);
            }

            Myers_linear<vector<int>> myers_linear(tmp_a, tmp_a.size(), tmp_b, tmp_b.size());
            f_diff = myers_linear.diff();

            for (int i = 0; i < (int)f_diff.size(); i++) {
                f_diff[i].row_num[LEFT]  += slice.a_low;
                f_diff[i].row_num[RIGHT] += slice.b_low;
            }

            return f_diff;
        }

        int binary_search(vector<Match *> &stacks, Match *match) {
            int low  = -1;
            int high = (int)stacks.size();

            while (low + 1 < high) {
                int mid = (low + high) / 2;
                if (stacks[mid]->b_line < match->b_line) {
                    low = mid;
                } else {
                    high = mid;
                }
            }

            return low;
        }

        Match *patience_sort(Match *matches) {
            Match          *match;
            vector<Match *> stacks;
            int              i;

            for (match = matches; match != NULL; match = match->next) {
                i = binary_search(stacks, match);

                if (i >= 0) {
                    match->prev = stacks[i];
                }

                if (i + 1 >= (int)stacks.size()) {
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
                return NULL;
            }

            match->next = NULL;
            while (match->prev != NULL) {
                match->prev->next = match;
                match             = match->prev;
            }

            return match;
        }

        vector<Match *> match_ptrs;

        Match *lowest_frequency_matching_lines(Slice slice) {
            Match                                   *match;
            Match                                   *last_match;
            Match                                   *tmp_match;
            unordered_map<int, LineEntry>            hashmap;
            unordered_map<int, LineEntry>::iterator  hashmap_iter;
            vector<int>                              order;
            int                                      min_count;

            for (int i = slice.a_low; i < slice.a_high; i++) {
                hashmap_iter = hashmap.find(a[i]);
                if (hashmap_iter == hashmap.end()) {
                    hashmap.insert({a[i], {1, 0, i, -1}});
                    order.push_back(a[i]);
                } else {
                    hashmap_iter->second.count_a += 1;
                }
            }

            for (int i = slice.b_low; i < slice.b_high; i++) {
                hashmap_iter = hashmap.find(b[i]);
                if (hashmap_iter == hashmap.end()) {
                    hashmap.insert({b[i], {0, 1, -1, i}});
                    order.push_back(b[i]);
                } else {
                    hashmap_iter->second.count_b += 1;
                    if (hashmap_iter->second.idx_b == -1) {
                        hashmap_iter->second.idx_b = i;
                    }
                }
            }

            // Find the minimum A-frequency among lines that appear in both files.
            // This is the key difference from patience: rather than requiring count == 1,
            // we find the rarest common line and use it (and any equally rare lines) as
            // anchors. Falls back to Myers if no lines are common at all.
            min_count = max + 1;
            for (int i = 0; i < (int)order.size(); i++) {
                const LineEntry &e = hashmap.at(order[i]);
                if (e.count_a > 0 && e.count_b > 0 && e.count_a < min_count) {
                    min_count = e.count_a;
                }
            }

            if (min_count == max + 1) {
                return NULL;
            }

            match = NULL;
            for (int i = 0; i < (int)order.size(); i++) {
                const LineEntry &e = hashmap.at(order[i]);
                if (e.count_a == min_count && e.count_b > 0) {
                    if (match == NULL) {
                        tmp_match  = new Match(e.idx_a, e.idx_b, NULL, NULL);
                        match_ptrs.push_back(tmp_match);
                        match      = tmp_match;
                    } else {
                        tmp_match        = new Match(e.idx_a, e.idx_b, last_match, NULL);
                        match_ptrs.push_back(tmp_match);
                        last_match->next = tmp_match;
                    }
                    last_match = tmp_match;
                }
            }

            return match;
        }

        vector<file_diff> diff(Slice &slice) {
            int               start_index;
            Match            *match;
            int               a_line;
            int               b_line;
            int               a_next;
            int               b_next;
            vector<file_diff> f_diff;
            vector<file_diff> tmp_f_diff;

            start_index = (int)match_ptrs.size();
            match = patience_sort(lowest_frequency_matching_lines(slice));

            if (match == NULL) {
                return fallback(slice);
            }

            a_line = slice.a_low;
            b_line = slice.b_low;

            while (1) {
                a_next = match != NULL ? match->a_line : slice.a_high;
                b_next = match != NULL ? match->b_line : slice.b_high;

                Slice subslice = Slice(a_line, a_next, b_line, b_next);

                tmp_f_diff = diff(subslice);
                for (int i = 0; i < (int)tmp_f_diff.size(); i++) {
                    f_diff.push_back(tmp_f_diff[i]);
                }

                if (match == NULL) {
                    for (int i = start_index; i < (int)match_ptrs.size(); i++) {
                        delete match_ptrs[i];
                    }
                    match_ptrs.resize(start_index);
                    return f_diff;
                }

                f_diff.emplace_back(EQL, match->a_line + 1, EQL, match->b_line + 1);

                a_line = match->a_line + 1;
                b_line = match->b_line + 1;

                match  = match->next;
            }
        }
};

#endif
