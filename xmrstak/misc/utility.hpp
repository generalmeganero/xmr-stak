#pragma once

#include <string>
#include <vector>

namespace xmrstak
{
	/** case insensitive string compare
	 *
	 * @return true if both strings are equal, else false
	 */
	bool strcmp_i(const std::string& str1, const std::string& str2);
	void split(const std::string &s, const char* delim, std::vector<std::string> & v);
} // namepsace xmrstak
