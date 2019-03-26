//
// Created by hariharan on 3/18/19.
//

#include <test/util.h>
#include "server.h"
#include <boost/stacktrace.hpp>

int main(int argc, char*argv[]){
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);
    std::string name="hfetch_server";
    pthread_setname_np(pthread_self(), name.c_str());
    MPI_Init(&argc,&argv);
    int my_rank,comm_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    /*if (my_rank == 0) {
        printf("Press any key to start server\n");
        getchar();
    }
    MPI_Barrier(MPI_COMM_WORLD);*/
    InputArgs args = Server::InitializeServer(argc,argv);
    if(args.is_logging){
        char complete_log[256];
        sprintf(complete_log, "%s/server_%d.log", args.log_path,my_rank);
        freopen("test.txt","w+",stdout);
        freopen("test.txt","a",stderr);
    }
    setup_env(args);
    Singleton<Server>::GetInstance()->async_run(args.num_workers);
    if (CONF->my_rank_world == 0) {
        printf("Press any key to exit server\n");
    }
    getchar();
    Singleton<Server>::GetInstance()->stop();
    MPI_Finalize();
    return 0;
}