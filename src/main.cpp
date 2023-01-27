/* 
 * main.cpp
 */
/* $begin main.cpp */
#include <iostream>
#include <string>
#include <cstdlib>
#include "myshell.h"

int main (int argc, char * argv[]) {
	using std::cin;
	using std::cout;
	using std::endl;
	
	std::string cmdline;
	const char * prompt = DFL_PROMPT;
	
	init_shell();
	
	cout << prompt;
	while (std::getline(cin, cmdline).good()) {
		if(!cmd_is_empty(cmdline) && eval_cmd(cmdline) &&
			builtin_cmd(current_job) == 0)
		{
			launch_job(current_job, foreground);
		}
		
		do_job_notification();
		cout << prompt;
	}
	
	cout << "logout" << endl;
	std::exit(EXIT_SUCCESS);
}

/* $end main.cpp */
