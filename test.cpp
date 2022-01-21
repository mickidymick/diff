
/* File:
 * Author: Zach McMichael
 * Description:
 */
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <sstream>
#include <utility>
#include <functional>
#include <cstring>
#include "myers-diff/dmp_diff.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    string A{"Woo nice this is great!"};
    string B{"Woo naice thais isgreat!"};

    MyersDiff<string> diffs{A, B};

    for (int i=0; i<diffs.diffs().size(); i++) {
        if (diffs.diffs()[i].operation == INSERT) {
            cout << diffs.diffs()[i].str() << "  " << diffs.diffs()[i].text.from - B.begin() << "B" << endl;
        }else if (diffs.diffs()[i].operation == DELETE) {
            cout << diffs.diffs()[i].str() << "  " << diffs.diffs()[i].text.from - A.begin() << "A" << endl;
        }else {
            cout << diffs.diffs()[i].str() << "  " << diffs.diffs()[i].text.from - A.begin() << "A" << endl;
        }
    }

    return 0;
}
