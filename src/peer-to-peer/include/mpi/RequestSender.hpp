//
// Created by frank on 1/15/25.
//

#ifndef REQUESTSENDER_HPP
#define REQUESTSENDER_HPP

#include "../utils/fuse_headers.hpp"
#include <string>

class RequestSender {
public:
	static void sendWriteRequest(int sourceRank, int mpiWorldSize);
	static void sendReadRequest(int sourceRank, int mpiWorldSize);
	static void sendCreateFileRequest(std::string name, int sourceRank, int mpiWorldSize);
	static void sendDeleteFileRequest(std::string name, int sourceRank, int mpiWorldSize);
	static void sendCreateDirectoryRequest(std::string dirAbsPath,  int sourceRank, int mpiWorldSize);
	static void sendDeleteDirectoryRequest(std::string dirAbsPath,  int sourceRank, int mpiWorldSize);
	static void sendRenameRequest(std::string oldName, std::string newName,  int sourceRank, int mpiWorldSize);
	static void sendTerminationRequest(int sourceRank, int mpiWorldSize);
};



#endif //REQUESTSENDER_HPP
