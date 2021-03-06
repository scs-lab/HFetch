/*
 * Copyright (C) 2019  SCS Lab <scs-help@cs.iit.edu>, Hariharan
 * Devarajan <hdevarajan@hawk.iit.edu>, Xian-He Sun <sun@iit.edu>
 *
 * This file is part of HFetch
 * 
 * HFetch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
//
// Created by hariharan on 3/18/19.
//

#include <hfetch.h>
#include <mpi.h>
#include <src/common/macros.h>
#include <src/common/configuration_manager.h>
#include <src/common/singleton.h>
#include <sys/stat.h>
#include "util.h"

char* GenerateData(long size){
    char* data= static_cast<char *>(malloc(size));
    size_t num_elements=size;
    size_t offset=0;
    srand(200);
    for(int i=0;i<num_elements;++i) {
        int random = rand();
        char c = static_cast<char>((random % 26) + 'a');
        memcpy(data + offset, &c, sizeof(char));
        offset += sizeof(char);
    }
    return data;
}


inline bool exists(char* name) {
    struct stat buffer;
    return (stat(name, &buffer) == 0);
}

int main(int argc, char*argv[]){
    addSignals();
    InputArgs args = hfetch::MPI_Init(&argc,&argv);

    //setup_env(args);
    int my_rank,comm_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    if(args.is_logging){
        char complete_log[256];
        sprintf(complete_log, "%s/run_%d.log", args.log_path,my_rank);
        freopen(complete_log,"w+",stdout);
        freopen(complete_log,"a",stderr);
    }
    size_t my_rank_size = args.io_size_mb/comm_size;
    void* buf = malloc(my_rank_size);
    char *homepath = getenv("RUN_DIR");
    //printf("rank:%d, my_server:%d\n",my_rank,CONF->my_server);
    char filename[256];
    sprintf(filename, "%s/pfs/test_%d.bat", homepath,my_rank);
    if(!exists(filename)){
        char command[256];
        sprintf(command,"dd if=/dev/urandom of=%s bs=%d count=%d",filename,MB,my_rank_size/MB);
        run_command(command);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (my_rank == 0) {
        printf("Press any key to start program\n");
        getchar();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    /* Actual APP */
    FILE* fh = hfetch::fopen(filename,"r");
    int iterations = 2;
    size_t small_io_size = my_rank_size/iterations;
    for(int i=0;i<iterations;i++){
        hfetch::fread(buf,small_io_size,1,fh);
        usleep(100000);
    }
    hfetch::fclose(fh);
    free(buf);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank:%d hit ratio %f\n",my_rank,CONF->hit/(CONF->total*1.0));
    hfetch::MPI_Finalize();
    //clean_env(args);
    return 0;
}
