#ifndef DiffPostprocess_HPP
#define DiffPostprocess_HPP

// Post-processing heuristics applied to diff output to improve readability.
// Each heuristic is controlled by a yed config variable and can be toggled
// independently. All operate on the finished diff (vector<file_diff>) and
// preserve correctness — they reorder or shift entries but never change
// what lines are marked as equal, inserted, or deleted.

// Reorder adjacent INS/DEL entries so all DELs come before INS within
// each contiguous change block. Makes diffs easier to read since you
// see what was removed before what was added.
static void postprocess_del_before_ins(vector<file_diff> &d) {
    int n = (int)d.size();
    int i = 0;

    while (i < n) {
        if (d[i].type[LEFT] != INS && d[i].type[LEFT] != DEL) {
            i++;
            continue;
        }

        int block_start = i;
        while (i < n && (d[i].type[LEFT] == INS || d[i].type[LEFT] == DEL)) {
            i++;
        }
        int block_end = i;

        // Stable partition: DELs first, then INS, preserving relative order
        // within each group.
        vector<file_diff> dels, inss;
        for (int j = block_start; j < block_end; j++) {
            if (d[j].type[LEFT] == DEL) {
                dels.push_back(d[j]);
            } else {
                inss.push_back(d[j]);
            }
        }

        int pos = block_start;
        for (auto &e : dels) d[pos++] = e;
        for (auto &e : inss) d[pos++] = e;
    }
}

// When an edit (INS or DEL) is adjacent to an EQL of the same line value,
// slide the edit downward. For example, deleting one "}" out of three
// consecutive "}" lines will show the last one as deleted rather than the
// first, which matches human expectation.
//
// Works by scanning for an edit followed by an EQL (or vice versa) where
// the underlying line values are the same, then swapping them so the edit
// moves toward the end of the run.
static void postprocess_slide_down(vector<file_diff> &d,
                                   const vector<int> &a,
                                   const vector<int> &b) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i + 1 < (int)d.size(); i++) {
            int t0 = d[i].type[LEFT];
            int t1 = d[i + 1].type[LEFT];

            // Pattern: DEL followed by EQL where both reference the same A value.
            // Swap so the EQL comes first, pushing the DEL downward.
            if (t0 == DEL && t1 == EQL) {
                int del_a = d[i].row_num[LEFT] - 1;
                int eql_a = d[i + 1].row_num[LEFT] - 1;
                if (del_a >= 0 && del_a < (int)a.size()
                &&  eql_a >= 0 && eql_a < (int)a.size()
                &&  a[del_a] == a[eql_a]) {
                    file_diff tmp = d[i];
                    d[i]     = d[i + 1];
                    d[i + 1] = tmp;
                    changed = true;
                }
            }

            // Pattern: INS followed by EQL where both reference the same B value.
            // Swap so the EQL comes first, pushing the INS downward.
            if (t0 == INS && t1 == EQL) {
                int ins_b = d[i].row_num[RIGHT] - 1;
                int eql_b = d[i + 1].row_num[RIGHT] - 1;
                if (ins_b >= 0 && ins_b < (int)b.size()
                &&  eql_b >= 0 && eql_b < (int)b.size()
                &&  b[ins_b] == b[eql_b]) {
                    file_diff tmp = d[i];
                    d[i]     = d[i + 1];
                    d[i + 1] = tmp;
                    changed = true;
                }
            }
        }
    }
}

// Indent heuristic: when a change block can slide (adjacent EQL has the same
// line value as the edit), position the boundary so the EQL line at the edge
// of the hunk has the lowest indentation. Blank lines score best (0), then
// lines by ascending indent level. This makes hunks start/end at function
// boundaries, closing braces, or blank lines rather than mid-block.
//
// line_indent and line_is_blank are indexed by line value ID (the ints in a/b).
static void postprocess_indent_heuristic(vector<file_diff> &d,
                                         const vector<int>  &a,
                                         const vector<int>  &b,
                                         const vector<int>  &line_indent,
                                         const vector<bool> &line_is_blank) {
    // Score a line value: blank = -1 (best), otherwise indent level.
    auto score = [&](int val) -> int {
        if (val < 0 || val >= (int)line_indent.size()) return 9999;
        if (line_is_blank[val]) return -1;
        return line_indent[val];
    };

    // Score the boundary EQL above a change block: lower is better.
    auto boundary_score_above = [&](int block_start) -> int {
        if (block_start <= 0) return 9999;
        if (d[block_start - 1].type[LEFT] != EQL) return 9999;
        int a_pos = d[block_start - 1].row_num[LEFT] - 1;
        if (a_pos < 0 || a_pos >= (int)a.size()) return 9999;
        return score(a[a_pos]);
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i + 1 < (int)d.size(); i++) {
            int t0 = d[i].type[LEFT];
            int t1 = d[i + 1].type[LEFT];

            // EQL followed by DEL with same A value: could slide DEL up.
            // Only do it if it improves the boundary score.
            if (t0 == EQL && t1 == DEL) {
                int eql_a = d[i].row_num[LEFT] - 1;
                int del_a = d[i + 1].row_num[LEFT] - 1;
                if (eql_a >= 0 && eql_a < (int)a.size()
                &&  del_a >= 0 && del_a < (int)a.size()
                &&  a[eql_a] == a[del_a]) {
                    int score_before = boundary_score_above(i + 1);
                    // If we swap, d[i] becomes the DEL and d[i+1] the EQL.
                    // The new boundary-above for the block starting at i
                    // is whatever is at i-1.
                    int score_after;
                    if (i <= 0 || d[i - 1].type[LEFT] != EQL) {
                        score_after = 9999;
                    } else {
                        int prev_a = d[i - 1].row_num[LEFT] - 1;
                        score_after = (prev_a >= 0 && prev_a < (int)a.size())
                                      ? score(a[prev_a]) : 9999;
                    }
                    if (score_after < score_before) {
                        file_diff tmp = d[i];
                        d[i]     = d[i + 1];
                        d[i + 1] = tmp;
                        changed = true;
                    }
                }
            }

            // DEL followed by EQL with same A value: could slide DEL down.
            if (t0 == DEL && t1 == EQL) {
                int del_a = d[i].row_num[LEFT] - 1;
                int eql_a = d[i + 1].row_num[LEFT] - 1;
                if (eql_a >= 0 && eql_a < (int)a.size()
                &&  del_a >= 0 && del_a < (int)a.size()
                &&  a[eql_a] == a[del_a]) {
                    int score_before = score(a[eql_a]);
                    int score_after;
                    if (i + 2 >= (int)d.size() || d[i + 2].type[LEFT] != EQL) {
                        score_after = 9999;
                    } else {
                        int next_a = d[i + 2].row_num[LEFT] - 1;
                        score_after = (next_a >= 0 && next_a < (int)a.size())
                                      ? score(a[next_a]) : 9999;
                    }
                    if (score_after < score_before) {
                        file_diff tmp = d[i];
                        d[i]     = d[i + 1];
                        d[i + 1] = tmp;
                        changed = true;
                    }
                }
            }

            // EQL followed by INS with same B value: could slide INS up.
            if (t0 == EQL && t1 == INS) {
                int eql_b = d[i].row_num[RIGHT] - 1;
                int ins_b = d[i + 1].row_num[RIGHT] - 1;
                if (eql_b >= 0 && eql_b < (int)b.size()
                &&  ins_b >= 0 && ins_b < (int)b.size()
                &&  b[eql_b] == b[ins_b]) {
                    int score_before = boundary_score_above(i + 1);
                    int score_after;
                    if (i <= 0 || d[i - 1].type[LEFT] != EQL) {
                        score_after = 9999;
                    } else {
                        int prev_b = d[i - 1].row_num[RIGHT] - 1;
                        score_after = (prev_b >= 0 && prev_b < (int)b.size())
                                      ? score(b[prev_b]) : 9999;
                    }
                    if (score_after < score_before) {
                        file_diff tmp = d[i];
                        d[i]     = d[i + 1];
                        d[i + 1] = tmp;
                        changed = true;
                    }
                }
            }

            // INS followed by EQL with same B value: could slide INS down.
            if (t0 == INS && t1 == EQL) {
                int ins_b = d[i].row_num[RIGHT] - 1;
                int eql_b = d[i + 1].row_num[RIGHT] - 1;
                if (eql_b >= 0 && eql_b < (int)b.size()
                &&  ins_b >= 0 && ins_b < (int)b.size()
                &&  b[eql_b] == b[ins_b]) {
                    int score_before = score(b[eql_b]);
                    int score_after;
                    if (i + 2 >= (int)d.size() || d[i + 2].type[LEFT] != EQL) {
                        score_after = 9999;
                    } else {
                        int next_b = d[i + 2].row_num[RIGHT] - 1;
                        score_after = (next_b >= 0 && next_b < (int)b.size())
                                      ? score(b[next_b]) : 9999;
                    }
                    if (score_after < score_before) {
                        file_diff tmp = d[i];
                        d[i]     = d[i + 1];
                        d[i + 1] = tmp;
                        changed = true;
                    }
                }
            }
        }
    }
}

// Blank-line gravity: when a change block can slide, prefer the position
// where the boundary EQL falls on a blank line. Simpler and less aggressive
// than the full indent heuristic — only slides when a blank-line boundary
// is reachable.
//
// Slides edits toward blank lines: if an EQL adjacent to an edit is the
// same value AND the line on the other side of the slide is blank, swap.
static void postprocess_blank_line_gravity(vector<file_diff> &d,
                                           const vector<int>  &a,
                                           const vector<int>  &b,
                                           const vector<bool> &line_is_blank) {
    auto is_blank = [&](int val) -> bool {
        if (val < 0 || val >= (int)line_is_blank.size()) return false;
        return line_is_blank[val];
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i + 1 < (int)d.size(); i++) {
            int t0 = d[i].type[LEFT];
            int t1 = d[i + 1].type[LEFT];

            // DEL followed by EQL with same A value: slide down if the
            // EQL after the swap target is blank.
            if (t0 == DEL && t1 == EQL) {
                int del_a = d[i].row_num[LEFT] - 1;
                int eql_a = d[i + 1].row_num[LEFT] - 1;
                if (eql_a >= 0 && eql_a < (int)a.size()
                &&  del_a >= 0 && del_a < (int)a.size()
                &&  a[eql_a] == a[del_a]
                &&  !is_blank(a[eql_a])) {
                    // Check if the line after the swap position is blank.
                    if (i + 2 < (int)d.size() && d[i + 2].type[LEFT] == EQL) {
                        int next_a = d[i + 2].row_num[LEFT] - 1;
                        if (next_a >= 0 && next_a < (int)a.size() && is_blank(a[next_a])) {
                            file_diff tmp = d[i];
                            d[i]     = d[i + 1];
                            d[i + 1] = tmp;
                            changed = true;
                        }
                    }
                }
            }

            // INS followed by EQL with same B value: slide down toward blank.
            if (t0 == INS && t1 == EQL) {
                int ins_b = d[i].row_num[RIGHT] - 1;
                int eql_b = d[i + 1].row_num[RIGHT] - 1;
                if (eql_b >= 0 && eql_b < (int)b.size()
                &&  ins_b >= 0 && ins_b < (int)b.size()
                &&  b[eql_b] == b[ins_b]
                &&  !is_blank(b[eql_b])) {
                    if (i + 2 < (int)d.size() && d[i + 2].type[LEFT] == EQL) {
                        int next_b = d[i + 2].row_num[RIGHT] - 1;
                        if (next_b >= 0 && next_b < (int)b.size() && is_blank(b[next_b])) {
                            file_diff tmp = d[i];
                            d[i]     = d[i + 1];
                            d[i + 1] = tmp;
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

// Hunk coalescence: when two change blocks are separated by a small number
// of EQL lines (gap_threshold or fewer), merge them into a single change
// block by converting the intervening EQL lines into DEL+INS pairs.
// This reduces visual noise from many tiny scattered hunks.
static void postprocess_hunk_coalescence(vector<file_diff> &d, int gap_threshold = 3) {
    if (d.size() == 0) return;

    // Find change blocks and the gaps between them.
    int n = (int)d.size();
    int i = 0;

    while (i < n) {
        // Skip non-change entries.
        if (d[i].type[LEFT] != INS && d[i].type[LEFT] != DEL) {
            i++;
            continue;
        }

        // Skip past this change block.
        while (i < n && (d[i].type[LEFT] == INS || d[i].type[LEFT] == DEL)) {
            i++;
        }

        // Count EQL gap.
        int gap_start = i;
        while (i < n && d[i].type[LEFT] == EQL) {
            i++;
        }
        int gap_end = i;
        int gap_len = gap_end - gap_start;

        // Check if there's another change block after the gap.
        if (i >= n || (d[i].type[LEFT] != INS && d[i].type[LEFT] != DEL)) {
            continue;
        }

        // Skip past the second change block so we know its extent.
        while (i < n && (d[i].type[LEFT] == INS || d[i].type[LEFT] == DEL)) {
            i++;
        }
        int block2_end = i;

        // If gap is small enough, convert EQL entries to DEL+INS pairs.
        if (gap_len > 0 && gap_len <= gap_threshold) {
            vector<file_diff> replacements;
            for (int j = gap_start; j < gap_end; j++) {
                replacements.emplace_back(DEL, d[j].row_num[LEFT], DEL, d[j].row_num[RIGHT]);
                replacements.emplace_back(INS, d[j].row_num[LEFT], INS, d[j].row_num[RIGHT]);
            }

            d.erase(d.begin() + gap_start, d.begin() + gap_end);
            d.insert(d.begin() + gap_start, replacements.begin(), replacements.end());

            // Adjust n and skip past the entire merged region (block1 + converted
            // gap + block2) so the merged block doesn't cascade into the next gap.
            n = (int)d.size();
            i = block2_end + gap_len;
        }
    }
}

// Semantic cleanup: when the EQL gap between two change blocks is shorter
// than the smaller of the two adjacent change blocks, merge them. This
// trades diff minimality for readability — instead of many small scattered
// edits with tiny equal regions between them, you get fewer, larger change
// regions that are easier to read as a whole.
static void postprocess_semantic_cleanup(vector<file_diff> &d) {
    if (d.size() == 0) return;

    int n = (int)d.size();
    int i = 0;

    while (i < n) {
        if (d[i].type[LEFT] != INS && d[i].type[LEFT] != DEL) {
            i++;
            continue;
        }

        // Measure first change block.
        int block1_start = i;
        while (i < n && (d[i].type[LEFT] == INS || d[i].type[LEFT] == DEL)) {
            i++;
        }
        int block1_len = i - block1_start;

        // Measure EQL gap.
        int gap_start = i;
        while (i < n && d[i].type[LEFT] == EQL) {
            i++;
        }
        int gap_end = i;
        int gap_len = gap_end - gap_start;

        // Check if there's another change block.
        if (i >= n || (d[i].type[LEFT] != INS && d[i].type[LEFT] != DEL)) {
            continue;
        }

        // Measure second change block.
        int block2_start = i;
        while (i < n && (d[i].type[LEFT] == INS || d[i].type[LEFT] == DEL)) {
            i++;
        }
        int block2_len = i - block2_start;
        int block2_end = i;

        // Merge if gap is shorter than the smaller block.
        int min_block = block1_len < block2_len ? block1_len : block2_len;
        if (gap_len > 0 && gap_len < min_block) {
            vector<file_diff> replacements;
            for (int j = gap_start; j < gap_end; j++) {
                replacements.emplace_back(DEL, d[j].row_num[LEFT], DEL, d[j].row_num[RIGHT]);
                replacements.emplace_back(INS, d[j].row_num[LEFT], INS, d[j].row_num[RIGHT]);
            }

            d.erase(d.begin() + gap_start, d.begin() + gap_end);
            d.insert(d.begin() + gap_start, replacements.begin(), replacements.end());

            // Adjust n and skip past the entire merged region (block1 + converted
            // gap + block2) so the merged block doesn't cascade into the next gap.
            n = (int)d.size();
            i = block2_end + gap_len;
        }
    }
}

#endif
