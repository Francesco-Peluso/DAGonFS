//
// Created by frank on 1/15/25.
//

#include "RequestSender.hpp"

#include <cstring>
#include <iostream>
#include <mpi.h>

#include "DistributedCode.hpp"
#include "mpi_data.hpp"

using namespace std;

void RequestSender::sendWriteRequest(int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = WRITE;
	for (int i=0; i<mpiWorldSize; i++) {
		if (i != sourceRank) {
			MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		}
	}
}

void RequestSender::sendReadRequest(int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = READ;
	for (int i=0; i<mpiWorldSize; i++) {
		if (i != sourceRank) {
			MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		}
	}
}

void RequestSender::sendCreateFileRequest(string name, int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = CREATE_FILE;
	FileCreationRequest fileCreateRequest;
	memcpy(fileCreateRequest.name, name.c_str(), name.size());
	for (int i=0 ; i<mpiWorldSize ; i++) {
		MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		if (i != sourceRank) {
			MPI_Send(&fileCreateRequest, sizeof(FileCreationRequest), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		}
	}
}

void RequestSender::sendDeleteFileRequest(string name, int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = DELETE_FILE;
	FileDeletionRequest fileDeleteRequest;
	memcpy(fileDeleteRequest.name, name.c_str(), name.size());
	for (int i=0 ; i<mpiWorldSize ; i++) {
		MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		if (i != sourceRank) {
			MPI_Send(&fileDeleteRequest, sizeof(FileDeletionRequest), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		}
	}
}

void RequestSender::sendCreateDirectoryRequest(string dirAbsPath, int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = CREATE_DIR;
	DirectoryCreationRequest dirCreateRequest;
	memcpy(dirCreateRequest.absolutePath, dirAbsPath.c_str(), dirAbsPath.size());
	for (int i=0; i< mpiWorldSize; i++) {
		MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		if (i != sourceRank)
			MPI_Send(&dirCreateRequest, sizeof(DirectoryCreationRequest), MPI_BYTE, i, 0, MPI_COMM_WORLD);
	}
}

void RequestSender::sendDeleteDirectoryRequest(string dirAbsPath, int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = DELETE_DIR;
	DirectoryDeletionRequest dirDeleteRequest;
	memcpy(dirDeleteRequest.absolutePath, dirAbsPath.c_str(), dirAbsPath.size());
	for (int i=0; i< mpiWorldSize; i++) {
		MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		if (i != sourceRank)
			MPI_Send(&dirDeleteRequest, sizeof(DirectoryDeletionRequest), MPI_BYTE, i, 0, MPI_COMM_WORLD);
	}
}

void RequestSender::sendRenameRequest(std::string oldName, std::string newName, int sourceRank, int mpiWorldSize) {
	RequestPacket loopRequest;
	loopRequest.type = RENAME;
	RenameRequest renameRequest;
	memcpy(renameRequest.oldName, oldName.c_str(), oldName.size());
	memcpy(renameRequest.newName, newName.c_str(), newName.size());
	for (int i=0; i< mpiWorldSize; i++) {
		MPI_Send(&loopRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
		if (i != sourceRank)
			MPI_Send(&renameRequest, sizeof(RenameRequest), MPI_BYTE, i, 0, MPI_COMM_WORLD);
	}
}


void RequestSender::sendTerminationRequest(int sourceRank, int mpiWorldSize) {
	cout << "Process " << sourceRank << " - Sending termination request" << endl;
	RequestPacket termationRequest;
	termationRequest.type = TERMINATE;
	for (int i=0; i<mpiWorldSize; i++) {
          MPI_Send(&termationRequest, sizeof(RequestPacket), MPI_BYTE, i, 0, MPI_COMM_WORLD);
	}

}
