#include <stdio.h>
#include <stdlib.h>
/**
 * Reads a line and creates and returns null terminated string.
 * Parameters:
 *       string The array to read a line from.
 *       size Size of he resulting string.
 *
 */
char *read_line_from_array(char *string, int *size, char delimeter)
{
   
         
    char readCharacter = 'a';
    (*size) = 0;
    char *line = malloc(sizeof(char));
    while (readCharacter != '\n' && readCharacter != '\0' &&
           readCharacter != EOF && readCharacter != delimeter && (*size) < 254) {
        (*size)++;
        line = realloc(line, sizeof(char) * ((*size)));
        readCharacter = string[(*size) - 1];
        line[(*size) - 1] = readCharacter;
    }
    (*size)++;
    line[(*size) - 2] = '\0';
    return line;
    
   
}

/**
 * char*read_line_from_file
 *
 * @param  {FILE*} file     :
 * @param  {int*} size      :
 * @param  {char} delimeter :
 */
char *read_line_from_file(FILE * file, int *size, char delimeter)
{
    char readCharacter = 'a';
    (*size) = 0;
    char *line = malloc(sizeof(char));
    while (readCharacter != '\n' && readCharacter != '\0' &&
           readCharacter != EOF && readCharacter != delimeter) {
        (*size)++;
        line = realloc(line, sizeof(char) * ((*size)));
        readCharacter = fgetc(file);
        line[(*size) - 1] = readCharacter;
    }
    (*size)++;
    line[(*size) - 2] = '\0';
    return line;
}
