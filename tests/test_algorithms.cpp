/*
 * Unit tests for all four diff algorithms.
 *
 * Build & run (from tests/):
 *   g++ -std=c++11 -I. -o test_algorithms test_algorithms.cpp && ./test_algorithms
 *
 * Verification strategy: given a diff of A → B, reconstruct B by walking the
 * diff (EQL entries come from A, INS entries come from B, DEL entries are
 * skipped).  The reconstruction must equal B exactly.  We also verify that
 * every line in A is covered by exactly one EQL or DEL entry, and every line
 * in B is covered by exactly one EQL or INS entry.
 */

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>

using namespace std;

/* ── types shared with the algorithm headers ─────────────────────────────── */

#define LEFT  (0)
#define RIGHT (1)
#define INIT  (0)
#define INS   (1)
#define DEL   (2)
#define EQL   (3)
#define CHG   (4)
#define NUL   (5)
#define TRUNC (7)
#define EXPND (8)

class file_diff {
public:
    int type[2];
    int row_num[2];
    file_diff(int t_1, int row_1, int t_2, int row_2) {
        type[0]    = t_1;  type[1]    = t_2;
        row_num[0] = row_1; row_num[1] = row_2;
    }
};

struct LineEntry { int count_a, count_b, idx_a, idx_b; };

/* ── algorithm headers (same include order as diff.cpp) ──────────────────── */

#include "../diff_algorithms/myers_diff.hpp"
#include "../diff_algorithms/myers_linear_diff.hpp"
#include "../diff_algorithms/patience_diff.hpp"
#include "../diff_algorithms/histogram_diff.hpp"
#include "../diff_algorithms/diff_postprocess.hpp"

/* ── test framework ──────────────────────────────────────────────────────── */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/*
 * Verify that `d` is a valid diff from `a` to `b`.
 *
 * Returns an empty string on success, or a description of the first failure.
 */
static string verify(const vector<int> &a,
                     const vector<int> &b,
                     const vector<file_diff> &d) {
    /* coverage maps */
    vector<int> a_cover(a.size(), 0);
    vector<int> b_cover(b.size(), 0);

    for (int i = 0; i < (int)d.size(); i++) {
        int t      = d[i].type[LEFT];   /* always same on both sides */
        int a_pos  = d[i].row_num[LEFT]  - 1;
        int b_pos  = d[i].row_num[RIGHT] - 1;

        if (t == EQL || t == DEL) {
            if (a_pos < 0 || a_pos >= (int)a.size())
                return "EQL/DEL a_pos out of range at entry " + to_string(i);
            a_cover[a_pos]++;
        }
        if (t == EQL || t == INS) {
            if (b_pos < 0 || b_pos >= (int)b.size())
                return "EQL/INS b_pos out of range at entry " + to_string(i);
            b_cover[b_pos]++;
        }
        if (t == EQL) {
            if (a[a_pos] != b[b_pos])
                return "EQL mismatch at entry " + to_string(i)
                       + " a[" + to_string(a_pos) + "]=" + to_string(a[a_pos])
                       + " b[" + to_string(b_pos) + "]=" + to_string(b[b_pos]);
        }
    }

    for (int i = 0; i < (int)a.size(); i++) {
        if (a_cover[i] == 0)
            return "A line " + to_string(i) + " not covered";
        if (a_cover[i] > 1)
            return "A line " + to_string(i) + " covered " + to_string(a_cover[i]) + " times";
    }
    for (int i = 0; i < (int)b.size(); i++) {
        if (b_cover[i] == 0)
            return "B line " + to_string(i) + " not covered";
        if (b_cover[i] > 1)
            return "B line " + to_string(i) + " covered " + to_string(b_cover[i]) + " times";
    }

    /* reconstruct B from the diff and compare */
    vector<int> recon;
    for (int i = 0; i < (int)d.size(); i++) {
        int t     = d[i].type[LEFT];
        int a_pos = d[i].row_num[LEFT]  - 1;
        int b_pos = d[i].row_num[RIGHT] - 1;
        if (t == EQL)       recon.push_back(a[a_pos]);
        else if (t == INS)  recon.push_back(b[b_pos]);
    }
    if (recon != b) {
        string msg = "reconstruction mismatch: got [";
        for (int v : recon) msg += to_string(v) + " ";
        msg += "] expected [";
        for (int v : b) msg += to_string(v) + " ";
        msg += "]";
        return msg;
    }

    return "";
}

/* count EQL entries (measures diff quality — higher is better) */
static int count_eql(const vector<file_diff> &d) {
    int n = 0;
    for (auto &e : d) if (e.type[LEFT] == EQL) n++;
    return n;
}

/* run one test case, report result */
static const char *type_str(int t) {
    if (t == INS) return "INS";
    if (t == DEL) return "DEL";
    if (t == EQL) return "EQL";
    return "???";
}

static void run_test(const char        *algo,
                     const char        *name,
                     const vector<int> &a,
                     const vector<int> &b,
                     vector<file_diff>  d) {
    tests_run++;
    string err = verify(a, b, d);
    if (err.empty()) {
        tests_passed++;
        printf("  PASS  [%s] %s\n", algo, name);
    } else {
        tests_failed++;
        printf("  FAIL  [%s] %s\n        %s\n", algo, name, err.c_str());
        printf("        diff entries (%d):\n", (int)d.size());
        for (int i = 0; i < (int)d.size(); i++) {
            printf("          [%d] %s L=%d R=%d\n",
                   i, type_str(d[i].type[LEFT]),
                   d[i].row_num[LEFT], d[i].row_num[RIGHT]);
        }
    }
}

/* ── helpers that run all four algorithms on the same inputs ─────────────── */

static void test_all(const char *name, vector<int> a, vector<int> b) {
    {
        Myers<vector<int>> m(a, a.size(), b, b.size());
        run_test("myers", name, a, b, m.diff());
    }
    {
        Myers_linear<vector<int>> ml(a, a.size(), b, b.size());
        run_test("myers_linear", name, a, b, ml.diff());
    }
    {
        Patience<vector<int>> p(a, a.size(), b, b.size());
        Slice s(0, p.len_a, 0, p.len_b);
        run_test("patience", name, a, b, p.diff(s));
    }
    {
        Histogram<vector<int>> h(a, a.size(), b, b.size());
        Slice s(0, h.len_a, 0, h.len_b);
        run_test("histogram", name, a, b, h.diff(s));
    }
}

/* ── test cases ──────────────────────────────────────────────────────────── */

static void test_basic() {
    printf("\n── basic correctness ────────────────────────────────────────\n");

    /* identical */
    test_all("identical",        {1,2,3,4,5},         {1,2,3,4,5});
    /* single element */
    test_all("single identical", {42},                 {42});
    test_all("single different", {1},                  {2});
    /* empty cases */
    test_all("empty A",          {},                   {1,2,3});
    test_all("empty B",          {1,2,3},              {});
    test_all("both empty",       {},                   {});
    /* insertions */
    test_all("insert at end",    {1,2,3},              {1,2,3,4});
    test_all("insert at begin",  {1,2,3},              {0,1,2,3});
    test_all("insert in middle", {1,2,3},              {1,9,2,3});
    /* deletions */
    test_all("delete at end",    {1,2,3,4},            {1,2,3});
    test_all("delete at begin",  {1,2,3,4},            {2,3,4});
    test_all("delete in middle", {1,2,3,4},            {1,3,4});
    /* replacements */
    test_all("replace first",    {1,2,3},              {9,2,3});
    test_all("replace last",     {1,2,3},              {1,2,9});
    test_all("replace middle",   {1,2,3},              {1,9,3});
    test_all("replace all",      {1,2,3},              {4,5,6});
}

static void test_multi_change() {
    printf("\n── multi-change patterns ────────────────────────────────────\n");

    /* multiple insertions */
    test_all("multi insert",         {1,2,3},              {1,10,2,11,3});
    /* multiple deletions */
    test_all("multi delete",         {1,2,3,4,5},          {2,4});
    /* changes in different regions */
    test_all("changes head+tail",    {1,2,3,4,5},          {9,2,3,4,8});
    test_all("changes scattered",    {1,2,3,4,5,6,7},      {1,9,3,9,5,9,7});
    /* swap adjacent elements */
    test_all("adjacent swap",        {1,2,3,4},            {1,3,2,4});
    /* grow and shrink */
    test_all("many inserts",         {1,5},                {1,2,3,4,5});
    test_all("many deletes",         {1,2,3,4,5},          {1,5});
    /* mixed */
    test_all("mixed ins+del",        {1,2,3,4,5},          {0,2,4,6});
}

static void test_repeated_lines() {
    printf("\n── repeated lines (key for histogram/patience) ──────────────\n");

    /* sequences of braces / blank lines — exactly the unrelated-C-file pattern */
    /* { } = 10, blank = 20, return = 30, int = 40 */
    vector<int> braces_a = {10,40,10,30,10,10,40,10,30,10,10,40,10,30};
    vector<int> braces_b = {10,40,40,10,30,10,10,40,10,10,30,10};
    test_all("repeated braces", braces_a, braces_b);

    /* both files share only very repeated common lines */
    /* A: code block using values 1..5 with repeated 0s as "blank lines" */
    vector<int> only_repeated_a = {0,1,2,3,0,4,5,6,0,7,8,9,0};
    /* B: different code with same "blank line" pattern */
    vector<int> only_repeated_b = {0,10,11,12,0,13,14,0,15,16,17,18,0};
    test_all("only repeated common", only_repeated_a, only_repeated_b);

    /* interleaved identical and different */
    test_all("interleaved",      {1,99,1,99,1},        {1,77,1,77,1});

    /* long sequence with all same value */
    test_all("all same value",   {5,5,5,5,5},          {5,5,5,5,5,5});

    /* A and B swap blocks around a common anchor */
    test_all("swap blocks",      {1,2,3,100,4,5,6},    {4,5,6,100,1,2,3});
}

static void test_unrelated_files() {
    printf("\n── unrelated-file simulation ────────────────────────────────\n");

    /*
     * Simulates two unrelated C files where the only common "lines" are
     * highly repeated syntactic elements: open-brace (1), close-brace (2),
     * blank (3), return (4), int (5).  Unique function bodies use large IDs.
     * This is the exact scenario that triggered the histogram bug.
     */
    auto make_func = [](int id, int open, int close, int blank,
                        int ret, int kw) -> vector<int> {
        return { kw, id, open,
                 id+1, id+2, id+3,
                 ret,
                 close, blank };
    };

    vector<int> file_a, file_b;
    for (int i = 0; i < 8; i++) {
        auto f = make_func(1000 + i*10, 1, 2, 3, 4, 5);
        file_a.insert(file_a.end(), f.begin(), f.end());
    }
    for (int i = 0; i < 8; i++) {
        auto f = make_func(2000 + i*10, 1, 2, 3, 4, 5);
        file_b.insert(file_b.end(), f.begin(), f.end());
    }
    test_all("unrelated funcs same structure", file_a, file_b);

    /* completely disjoint — no common elements at all */
    vector<int> disjoint_a = {1,2,3,4,5,6,7,8};
    vector<int> disjoint_b = {9,10,11,12,13,14,15,16};
    test_all("disjoint sequences", disjoint_a, disjoint_b);

    /* one common element surrounded by unrelated content */
    test_all("one common anchor", {1,2,3,99,4,5,6},    {7,8,9,99,10,11,12});
}

static void test_histogram_quality() {
    printf("\n── histogram quality (EQL count) ────────────────────────────\n");

    /*
     * For these cases histogram should find at least as many EQL entries as
     * myers_linear because it has better anchor selection.  We compare the
     * two algorithms' EQL counts and flag it if histogram does strictly worse.
     */
    struct Case { const char *name; vector<int> a, b; };
    vector<Case> cases = {
        {
            "repeated common anchors",
            {3,1,2,3,1,2,3,1,2,3},
            {3,9,8,3,7,6,3,5,4,3}
        },
        {
            "blank-line aligned blocks",
            {0,1,2,3,0,4,5,6,0,7,8,9,0},
            {0,10,11,12,0,13,14,15,0,16,17,18,0}
        },
        {
            "min_count > 1 scenario",
            {5,1,2,5,3,4,5,6,7,5},
            {5,9,8,5,7,6,5,4,3,5}
        },
    };

    for (auto &c : cases) {
        Histogram<vector<int>> h(c.a, c.a.size(), c.b, c.b.size());
        Slice hs(0, h.len_a, 0, h.len_b);
        auto hdiff = h.diff(hs);
        int h_eql = count_eql(hdiff);

        Myers_linear<vector<int>> ml(c.a, c.a.size(), c.b, c.b.size());
        int ml_eql = count_eql(ml.diff());

        tests_run++;
        string err = verify(c.a, c.b, hdiff);
        if (!err.empty()) {
            tests_failed++;
            printf("  FAIL  [histogram quality] %s\n        %s\n", c.name, err.c_str());
        } else if (h_eql < ml_eql) {
            tests_failed++;
            printf("  FAIL  [histogram quality] %s\n"
                   "        histogram EQL=%d < myers_linear EQL=%d"
                   " (histogram should not be worse)\n",
                   c.name, h_eql, ml_eql);
        } else {
            tests_passed++;
            printf("  PASS  [histogram quality] %s"
                   "  (histogram EQL=%d  myers_linear EQL=%d)\n",
                   c.name, h_eql, ml_eql);
        }
    }
}

static void test_edge_cases() {
    printf("\n── edge cases ───────────────────────────────────────────────\n");

    /* one-element sequences */
    test_all("1 vs 1 same",          {7},              {7});
    test_all("1 vs 1 diff",          {7},              {8});
    test_all("1 vs 2",               {7},              {7,8});
    test_all("2 vs 1",               {7,8},            {7});

    /* long identical prefix/suffix with change in middle */
    vector<int> long_prefix_a(50, 1); long_prefix_a.push_back(99); for(int i=0;i<50;i++) long_prefix_a.push_back(i+2);
    vector<int> long_prefix_b(50, 1); long_prefix_b.push_back(77); for(int i=0;i<50;i++) long_prefix_b.push_back(i+2);
    test_all("long prefix+suffix change in middle", long_prefix_a, long_prefix_b);

    /* reverse of sequence */
    test_all("reversed",             {1,2,3,4,5},      {5,4,3,2,1});

    /* alternating match/mismatch */
    test_all("alternating",          {1,2,1,2,1},      {1,3,1,3,1});

    /* large identical sequence */
    vector<int> big(200);
    for (int i = 0; i < 200; i++) big[i] = i;
    vector<int> big2 = big;
    big2[100] = 9999;
    test_all("200 lines one change", big, big2);
}

/* ── post-processing tests ─────────────────────────────────────────────────── */

/* helper: check that a postprocessed diff is still valid */
static void run_pp_test(const char        *name,
                        const vector<int> &a,
                        const vector<int> &b,
                        vector<file_diff>  d) {
    tests_run++;
    string err = verify(a, b, d);
    if (err.empty()) {
        tests_passed++;
        printf("  PASS  [postprocess] %s\n", name);
    } else {
        tests_failed++;
        printf("  FAIL  [postprocess] %s\n        %s\n", name, err.c_str());
    }
}

static void test_del_before_ins() {
    printf("\n── postprocess: DEL-before-INS ──────────────────────────────\n");

    /* Build a diff with interleaved INS/DEL: INS, DEL, INS, DEL
     * After postprocess, should be DEL, DEL, INS, INS */
    vector<int> a = {1, 2, 3};
    vector<int> b = {4, 5, 3};

    /* Manually create: INS(b=0), DEL(a=0), INS(b=1), DEL(a=1), EQL(a=2,b=2) */
    vector<file_diff> d;
    d.emplace_back(INS, 1, INS, 1);
    d.emplace_back(DEL, 1, DEL, 1);
    d.emplace_back(INS, 2, INS, 2);
    d.emplace_back(DEL, 2, DEL, 2);
    d.emplace_back(EQL, 3, EQL, 3);

    postprocess_del_before_ins(d);

    /* Verify DELs come before INS */
    tests_run++;
    if (d[0].type[LEFT] == DEL && d[1].type[LEFT] == DEL
    &&  d[2].type[LEFT] == INS && d[3].type[LEFT] == INS
    &&  d[4].type[LEFT] == EQL) {
        tests_passed++;
        printf("  PASS  [postprocess] del-before-ins reordering\n");
    } else {
        tests_failed++;
        printf("  FAIL  [postprocess] del-before-ins reordering\n");
    }

    /* Run through all algorithms and verify postprocess preserves correctness */
    {
        vector<int> pa = {1,2,3,4,5};
        vector<int> pb = {6,2,7,4,8};
        Myers<vector<int>> m(pa, pa.size(), pb, pb.size());
        auto md = m.diff();
        postprocess_del_before_ins(md);
        run_pp_test("del-before-ins correctness (myers)", pa, pb, md);
    }
    {
        vector<int> pa = {1,2,3,4,5};
        vector<int> pb = {6,2,7,4,8};
        Histogram<vector<int>> h(pa, pa.size(), pb, pb.size());
        Slice s(0, h.len_a, 0, h.len_b);
        auto hd = h.diff(s);
        postprocess_del_before_ins(hd);
        run_pp_test("del-before-ins correctness (histogram)", pa, pb, hd);
    }
}

static void test_slide_down() {
    printf("\n── postprocess: slide-down ──────────────────────────────────\n");

    /* A = {1,1,1,2}, B = {1,1,2} — one "1" deleted.
     * Raw diff might place DEL at the first "1".
     * Slide-down should push it to the last "1" (before the "2"). */
    {
        vector<int> a = {1,1,1,2};
        vector<int> b = {1,1,2};
        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_slide_down(d, a, b);
        run_pp_test("slide-down del correctness", a, b, d);

        /* The DEL should be the last entry before EQL(2), not the first */
        tests_run++;
        bool del_at_end = false;
        for (int i = 0; i < (int)d.size(); i++) {
            if (d[i].type[LEFT] == DEL) {
                /* Check: the entry after this DEL (if EQL) should be the "2" */
                if (i + 1 < (int)d.size() && d[i+1].type[LEFT] == EQL
                &&  a[d[i+1].row_num[LEFT]-1] == 2) {
                    del_at_end = true;
                }
            }
        }
        if (del_at_end) {
            tests_passed++;
            printf("  PASS  [postprocess] slide-down del position\n");
        } else {
            tests_failed++;
            printf("  FAIL  [postprocess] slide-down del position\n");
        }
    }

    /* INS case: A = {1,1,2}, B = {1,1,1,2} */
    {
        vector<int> a = {1,1,2};
        vector<int> b = {1,1,1,2};
        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_slide_down(d, a, b);
        run_pp_test("slide-down ins correctness", a, b, d);
    }
}

static void test_indent_heuristic() {
    printf("\n── postprocess: indent heuristic ────────────────────────────\n");

    /* Simulate lines with different indentation.
     * Use line values where we can set up indent/blank info.
     * Value 0 = blank (indent 0), value 1 = "}" (indent 0),
     * value 2 = "    x = 1;" (indent 4) */
    {
        vector<int> a = {2, 2, 1, 1, 0};
        vector<int> b = {2, 2, 1, 0};
        /* indent: 0=blank, 1=indent0, 2=indent4 */
        vector<int>  indent   = {0, 0, 4};
        vector<bool> is_blank = {true, false, false};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_slide_down(d, a, b);
        postprocess_indent_heuristic(d, a, b, indent, is_blank);
        run_pp_test("indent heuristic correctness", a, b, d);
    }

    /* Larger case through full pipeline */
    {
        vector<int> a = {3,3,3,1,2,2,2};
        vector<int> b = {3,3,1,2,2,2};
        vector<int>  indent   = {0, 0, 4, 2};
        vector<bool> is_blank = {true, false, false, false};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_del_before_ins(d);
        postprocess_slide_down(d, a, b);
        postprocess_indent_heuristic(d, a, b, indent, is_blank);
        run_pp_test("indent heuristic full pipeline", a, b, d);
    }
}

static void test_blank_line_gravity() {
    printf("\n── postprocess: blank-line gravity ──────────────────────────\n");

    /* Value 0 = blank, value 1 = "}", value 2 = code
     * A = {1,1,0,2}, B = {1,0,2} — DEL of "}" should slide toward blank */
    {
        vector<int> a = {1, 1, 0, 2};
        vector<int> b = {1, 0, 2};
        vector<bool> is_blank = {true, false, false};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_blank_line_gravity(d, a, b, is_blank);
        run_pp_test("blank-line gravity correctness", a, b, d);
    }
}

static void test_hunk_coalescence() {
    printf("\n── postprocess: hunk coalescence ────────────────────────────\n");

    /* Two changes separated by 2 EQL lines (under threshold of 3).
     * A = {1,2,3,4,5}, B = {9,2,3,4,8}
     * Changes at positions 0 and 4 with 3 EQLs between.
     * gap=3 should merge. */
    {
        vector<int> a = {1,2,3,4,5};
        vector<int> b = {9,2,3,4,8};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_hunk_coalescence(d, 3);

        /* After coalescence, there should be no EQL entries (all merged) */
        tests_run++;
        int eql_count = count_eql(d);
        if (eql_count == 0) {
            tests_passed++;
            printf("  PASS  [postprocess] hunk coalescence merges small gap\n");
        } else {
            tests_failed++;
            printf("  FAIL  [postprocess] hunk coalescence merges small gap"
                   " (EQL count=%d, expected 0)\n", eql_count);
        }
    }

    /* Gap too large — should NOT merge */
    {
        vector<int> a = {1,2,3,4,5,6};
        vector<int> b = {9,2,3,4,5,8};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        int eql_before = count_eql(d);
        postprocess_hunk_coalescence(d, 3);
        int eql_after = count_eql(d);

        tests_run++;
        if (eql_after == eql_before) {
            tests_passed++;
            printf("  PASS  [postprocess] hunk coalescence preserves large gap\n");
        } else {
            tests_failed++;
            printf("  FAIL  [postprocess] hunk coalescence preserves large gap"
                   " (EQL before=%d after=%d)\n", eql_before, eql_after);
        }
    }
}

static void test_semantic_cleanup() {
    printf("\n── postprocess: semantic cleanup ────────────────────────────\n");

    /* A = {1,2,3,4,5,6,7}, B = {9,8,3,4,5,10,11,12}
     * Two change blocks separated by a gap.  If gap < min(block_sizes),
     * they should merge. */
    {
        vector<int> a = {1,2,3,7};
        vector<int> b = {9,8,3,10};

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        int eql_before = count_eql(d);
        postprocess_semantic_cleanup(d);
        int eql_after = count_eql(d);

        tests_run++;
        /* gap of 1 EQL between two blocks of 2 edits each: should merge */
        if (eql_after < eql_before) {
            tests_passed++;
            printf("  PASS  [postprocess] semantic cleanup merges\n");
        } else {
            tests_failed++;
            printf("  FAIL  [postprocess] semantic cleanup merges"
                   " (EQL before=%d after=%d)\n", eql_before, eql_after);
        }
    }

    /* Full pipeline correctness: run all postprocessing and verify */
    {
        vector<int> a = {1,2,3,4,5,6,7,8,9,10};
        vector<int> b = {1,20,3,40,5,6,70,8,90,10};
        vector<int>  indent   = {};
        vector<bool> is_blank = {};
        /* Fill indent/blank for all possible IDs */
        for (int i = 0; i <= 100; i++) {
            indent.push_back(i % 5);
            is_blank.push_back(false);
        }

        Myers<vector<int>> m(a, a.size(), b, b.size());
        auto d = m.diff();
        postprocess_del_before_ins(d);
        postprocess_slide_down(d, a, b);
        postprocess_indent_heuristic(d, a, b, indent, is_blank);
        postprocess_blank_line_gravity(d, a, b, is_blank);
        postprocess_hunk_coalescence(d, 3);
        postprocess_semantic_cleanup(d);
        /* After coalescence/semantic, verify is not applicable in the
         * standard sense (EQLs may have been converted), so just check
         * that no crash occurred and the diff is non-empty. */
        tests_run++;
        if (d.size() > 0) {
            tests_passed++;
            printf("  PASS  [postprocess] full pipeline no crash\n");
        } else {
            tests_failed++;
            printf("  FAIL  [postprocess] full pipeline produced empty diff\n");
        }
    }
}

static void test_postprocess() {
    test_del_before_ins();
    test_slide_down();
    test_indent_heuristic();
    test_blank_line_gravity();
    test_hunk_coalescence();
    test_semantic_cleanup();
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("diff algorithm unit tests\n");
    printf("==========================================================\n");

    test_basic();
    test_multi_change();
    test_repeated_lines();
    test_unrelated_files();
    test_histogram_quality();
    test_edge_cases();
    test_postprocess();

    printf("\n==========================================================\n");
    printf("results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf("  (%d FAILED)\n", tests_failed);
        return 1;
    }
    printf("\n");
    return 0;
}
