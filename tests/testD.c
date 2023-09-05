
static void align_buffers(Myers<vector<string>> myers, diff_buff buff_1, diff_buff buff_2) {
    char          tmp_buff[512];

    yed_buffer_t* buff;
    int           a = 0;
    int           add = 0;
    int           sub = 0;
    color_diff.clear();
    line.line_ type = 0;
    color_diff.push_back(line);

    for (int i = 0; i < l_diff.size(); i++) {
        line_color line1;

        if (l_diff[i].type == INS) {
            snprintf(tmp_buff, 512, "*diff:%s", buff_1.buff->name);
            buff = yed_get_buffer(tmp_buff);
            if (buff != NULL) {
//                 yed_buff_insert_line_no_undo(buff, l_diff[i].a_num + a);
//                 a++;
//                 line1.line_type = INS;
//                 color_diff.push_back(line1);
                DBG("INS\n");
                add++;
l;aksjdf
asd;lfkj
//                 vector<string> tmp_1;
//                 for (int c = 0; c < buffer_a.lines[i].size(); c++) {
//                     tmp_1.push_back(string(1, buffer_a.lines[i][c]));
//                 }
//                 vector<string> tmp_2;
//                 for (int c = 0; c < buffer_b.li
