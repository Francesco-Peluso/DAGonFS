//
// Created by frank on 11/29/24.
//

#include "ArgumentParser.hpp"
#include <cstring>

using namespace std;

ArgumentParser::ArgumentParser(int argc, char** argv) {
	this->copy_args(argc, argv);
}

/* Destructor. If the copy of the arguments has not been deleted, the destructor will free the allocated memory
 */
ArgumentParser::~ArgumentParser() {
	/*
	if (this->copied_args != nullptr)
		delete_args(this->argc,this->copied_args);
	*/
}

/* Function that make a copy of argv[] parameter of the main function
 * @param argc Number of agrument
 * @param argv The arguments pointer
 * @return The Copy of the argumetens
 */
char ** ArgumentParser::copy_args(int argc, char * argv[]) {
	this->copied_args = new char*[argc];
	for (int i = 0; i < argc; ++i) {
		int len = (int) strlen(argv[i]) + 1;
		copied_args[i] = new char[len];
		strncpy(copied_args[i], argv[i], len);
	}
	return copied_args;
}

/* Procedure that free the memory previously allocated for the copy of arguments
 * @param argc Number of arguments
 * @param argv The actual data to free
 * */
void ArgumentParser::delete_args(int argc, char** argv){
	for (int i = 0; i < argc; i++) {
		delete this->copied_args[i];
	}
	delete this->copied_args;
}