#ifndef _unix_helper_hpp_
#define _unix_helper_hpp_

#include <vector>

// execute shell command and return shell output
std::string getCmdOutput(char* cmd);

// convert unix time to format HH:MM
std::string getReadableTime();

// replaces all substrings inside a string
void strReplace(std::string& str, const std::string& oldStr, const std::string& newStr);

// converts an integer into a string
std::string IntToStr(int value);

// checks if a file exists
bool FileExists(const std::string& filename);


std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

#endif