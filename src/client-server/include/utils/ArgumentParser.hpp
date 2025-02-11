//
// Created by frank on 1/2/25.
//

#ifndef ARGUMENTPARSER_HPP
#define ARGUMENTPARSER_HPP

class ArgumentParser {
private:
	int argc;
	char **copied_args;

public:
	ArgumentParser(){};
	ArgumentParser(int argc, char **argv);
	~ArgumentParser();
	char **copy_args(int argc, char * argv[]);
	char **getCopiedArgs(){return copied_args;};
	void delete_args(int argc, char **argv);
};

#endif //ARGUMENTPARSER_HPP
