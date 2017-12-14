#include <string>
#include <vector>
#include <algorithm>


namespace xmrstak
{
	bool strcmp_i(const std::string& str1, const std::string& str2)
	{
		if(str1.size() != str2.size())
			return false;
		else
		return (str1.empty() | str2.empty()) ?
				false :
				std::equal(str1.begin(), str1.end(),str2.begin(),
					[](char c1, char c2)
					{
						return ::tolower(c1) == ::tolower(c2);
					}
				);
	}

	void split(const std::string &s, const char* delim, std::vector<std::string> & v) {
		// to avoid modifying original string
		// first duplicate the original string and return a char pointer then free the memory
		char * dup = strdup(s.c_str());
		char * token = strtok(dup, delim);
		while (token != NULL) {
			v.push_back(std::string(token));
			// the call is treated as a subsequent calls to strtok:
			// the function continues from where it left in previous invocation
			token = strtok(NULL, delim);
		}
		// add empty string if original string ends with delim
		if (s.c_str()[s.length()-1] == delim[0]) {
			v.push_back(std::string(""));
		}
		free(dup);
	}
} // namepsace xmrstak
