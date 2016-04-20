#include <stdio.h>

// characters = anything, including new line
// words - non-zero-length sequence of characters delimited by whitespace
// lines - new lines

int main(int argc, char *argv[]) {
    
    int numOfLines = 0;
    int numOfWords = 0;	
    int numOfChars = 0;
    FILE *filename;

    int firstIteration = 1;
    int prevChar = -1;
    int currChar;


    // determine if there is a file input or using stdin

    if (argc == 2) {
    	filename = fopen(argv[1], "r");
    } else if (argc == 1) {
        filename = stdin;
    }

    currChar = fgetc(filename);
	while (feof(filename) == 0) {
		numOfChars++; //incrementing characters

		if (firstIteration == 0) { //incrementing words
			if (isspace(prevChar) == 0 && isspace(currChar) != 0) {
				numOfWords++;
			}
		} if (currChar == '\n') { //incrementing new line
			numOfLines++;
		}
        if (firstIteration == 1) {
            firstIteration = 0;
        }
		prevChar = currChar;
        currChar = fgetc(filename);
	}
	fclose(filename);
    if (isspace(prevChar) == 0) {
        numOfWords++;
    }
	printf(" %d %d %d \n", numOfLines, numOfWords, numOfChars);


    return 0;
}
