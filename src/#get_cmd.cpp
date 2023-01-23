/* 
 * get_cmd.cpp
 */
/* $begin get_cmd.cpp */
#include <string>
#include <cctype>

bool cmd_is_empty(const std::string & cmdline) {
	std::size_t sz = cmdline.size();
	for (std::size_t i = 0; i < sz; ++ i) {
		if (!std::isspace(cmdline[i]))
			return false;
	}
	
	return true;
}

/* $end get_cmd.cpp */
