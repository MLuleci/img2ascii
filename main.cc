#include <iostream>
#include <locale>
#include "image.h"

using namespace std;

int main(int argc, char **argv)
{
    // Set locale to default (UTF-8)
	setlocale(LC_ALL, "");

    // Check args
	if (argc < 3) {
		cerr << "Usage: " << argv[0] 
             << " <input> <output>" << endl;
		return 1;
	}

    // Check output file
    FILE *out;
    if (!(out = fopen(argv[2], "w+"))) {
        cerr << "Cannot open file \"" 
             << argv[2] << '"' << endl;
        return 1;
    }

    try {
        // Load image
        Image im (argv[1]);

        // Process image
        string s (ascii(im));
        size_t sz = s.size();

        // Output result
        if (sz != fwrite(s.c_str(), 1, sz, out)) {
            cerr << "Error while writing output" << endl;
            return 1;
        }
    } catch (const exception& ex) {
        fclose(out);
        cerr << ex.what() << endl;
        return 1;
    }

    fclose(out);
    cout << "Done." << endl;
    return 0;
}
