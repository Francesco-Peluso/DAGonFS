#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <cstdlib>

using namespace std;

map<string, string> readConfig(const string& filename) {
    map<string, string> config;
    ifstream file(filename);
    string line;

    if (!file) {
        cerr << "Error: Unable to open configuration file!" << endl;
        exit(1);
    }

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Ignora linee vuote e commenti
        size_t pos = line.find('=');
        if (pos != string::npos) {
            string key = line.substr(0, pos-1);
            string value = line.substr(pos + 1);
            if(value.length() == 1 || value.length() == 0)
              value = "";
            else
              value = value.substr(1);
            config[key] = value;
            cout<<"config["<<key<<"]="<<value<<endl;
        }
    }

    return config;
}

int main() {
    string configFile = "DAGonFS.ini";
    auto config = readConfig(configFile);

    // Recupero parametri
    int num_processes = stoi(config["num_processes"]);
    string machinefile = config["machinefile"];
    string executable;
    if(config["dagonfs_model"] == "client-server"){
      executable = "DAGonFS_CS.exe";}
    else{cout << config["dagonfs_model"]<<"==client-server"<<endl;
      executable = "DAGonFS_P2P.exe";}
    string fuse_mode = config["fuse_mode"];
    string dagonfs_root_dir = config["dagonfs_root_dir"];
    bool enable_logging = (config["enable_logging"] == "true");

    // Costruisco il comando mpirun
    stringstream command;
    command << "mpirun -np " << num_processes;
    if(machinefile.length() != 0)
      command << "--hostfile " << machinefile;
    command << " ./" << executable
            << " -" << fuse_mode
            << " " << dagonfs_root_dir
            << " " << enable_logging;

    // Stampo ed eseguo il comando
    cout << "Eseguendo: " << command.str() << endl;
    int ret = system(command.str().c_str());
    if (ret != 0) {
        cerr << "Error during DAGonFS execution!" << endl;
    }

    return ret;
}

