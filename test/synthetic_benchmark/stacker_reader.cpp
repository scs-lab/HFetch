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
#include "../util.h"

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

typedef enum ReaderType{
    READ_ENTIRE_EACH_TS=0,
    READ_ENTIRE_SAME_TS=1,
    READ_ENTIRE_EVERY_STRIDE_TS=2,
    READ_ENTIRE_RANDOM_HALF_TS=3,
} ReaderType;
typedef unsigned long long really_long;
struct ReaderInput{
    ReaderType type;
    size_t compute_sec;
};

inline ReaderInput ParseArgs(int argc,char* argv[]){
    ReaderInput args;
    args.type=ReaderType::READ_ENTIRE_EACH_TS;
    args.compute_sec=0;
    int opt;
    /* a:c:d:f:i:l:m:n:p:r:s:w: */
    optind=1;
    while ((opt = getopt (argc, argv, "a:c:d:f:i:l:m:n:p:r:s:w:b:e:")) != -1)
    {
        switch (opt)
        {
            case 'b':{
                args.type= (ReaderType) atoi(optarg);
                break;
            }
            case 'e':{
                args.compute_sec= atoi(optarg);
                break;
            }
            default: {}
        }
    }
    return args;
}

int main(int argc, char*argv[]){
    addSignals();
    InputArgs args = hfetch::MPI_Init(&argc,&argv);
    int my_rank,comm_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    ReaderInput input = ParseArgs(argc,argv);
    if(args.is_logging){
        char complete_log[256];
        sprintf(complete_log, "%s/run_%d.log", args.log_path,my_rank);
        freopen(complete_log,"w+",stdout);
        freopen(complete_log,"a",stderr);
    }
    size_t file_size = args.io_size_mb*MB/2;
    char *pfs_path = getenv("RUN_DIR");
    if(my_rank==0){
        char filename_1[256];
        sprintf(filename_1, "%s/pfs/test_0.bat", pfs_path);
        if(!exists(filename_1)){
            char command[256];
            sprintf(command,"dd if=/dev/urandom of=%s bs=%d count=%d > /dev/null 2>&1",filename_1,MB,file_size/MB);
            run_command(command);
        }
        char filename_2[256];
        sprintf(filename_2, "%s/pfs/test_1.bat", pfs_path);
        if(!exists(filename_2)){
            char command[256];
            sprintf(command,"dd if=/dev/urandom of=%s bs=%d count=%d > /dev/null 2>&1",filename_2,MB,file_size/MB);
            run_command(command);
        }
        printf("Data is prepared for the test of size %d MB\n",file_size*2/MB);
    }
    run_command("sudo fm");
    MPI_Barrier(MPI_COMM_WORLD);
    if (my_rank == 0) {
        printf("Press any key to start program\n");
        getchar();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    Timer t;
    t.startTime();
    char filename[256];
    sprintf(filename, "%s/pfs/test_%d.bat", pfs_path,my_rank%2);
    FILE* fh = hfetch::fopen(filename,"r");
    size_t timesteps=args.iteration_;

    switch(input.type){
        case ReaderType::READ_ENTIRE_EACH_TS:{
            size_t read_size = 1*1024*1024;
            size_t read_iterations=file_size/read_size/32;
            read_iterations=read_iterations==0?1:read_iterations;
            void* buf = malloc(read_size);
            for(size_t i=0;i<timesteps;++i){
                for(int j=0;j<read_iterations;++j){
                    hfetch::fread(buf,read_size,1,fh);
                    if(input.compute_sec!=0) sleep(input.compute_sec);
                }
                hfetch::fseek(fh,0L,SEEK_SET);
            }
            free(buf);
            break;
        }
        case ReaderType::READ_ENTIRE_EVERY_STRIDE_TS:{
            ssize_t read_size = 1*1024*1024;
            size_t read_iterations=file_size/read_size/32;
            void* buf = malloc(read_size);
            for(size_t i=0;i<timesteps;++i){
                for(int j=0;j<read_iterations;++j){
                    hfetch::fseek(fh,(j+my_rank%2)*read_size,SEEK_SET);
                    hfetch::fread(buf,read_size,1,fh);
                    if(input.compute_sec!=0) sleep(input.compute_sec);
                }
                hfetch::fseek(fh,0L,SEEK_SET);

            }
            free(buf);
            break;
        }
        case ReaderType::READ_ENTIRE_SAME_TS:{
            size_t read_size = 1*1024*1024;
            size_t read_iterations=file_size/read_size/32;
            void* buf = malloc(read_size);
            for(size_t i=0;i<timesteps;++i){
                for(int j=0;j<read_iterations;++j){
                    hfetch::fseek(fh,(my_rank%(read_iterations-1))*read_size,SEEK_SET);
                    hfetch::fread(buf,read_size,1,fh);
                    if(input.compute_sec!=0) sleep(input.compute_sec);
                }
                hfetch::fseek(fh,0L,SEEK_SET);

            }
            free(buf);
            break;
        }
        case ReaderType::READ_ENTIRE_RANDOM_HALF_TS:{

            srand(200);

            size_t read_size = 1*1024*1024;
            size_t read_iterations=file_size/read_size/32;
            void* buf = malloc(read_size);
            for(size_t i=0;i<timesteps;++i){
                for(int j=0;j<read_iterations;++j){
                    int random = rand();
                    hfetch::fseek(fh,(random%(read_iterations-1))*read_size,SEEK_SET);
                    hfetch::fread(buf,read_size,1,fh);
                    if(input.compute_sec!=0) sleep(input.compute_sec);
                }
                hfetch::fseek(fh,0L,SEEK_SET);

            }
            free(buf);
            break;
        }
    }
    hfetch::fclose(fh);
    double v = t.endTime();
    MPI_Barrier(MPI_COMM_WORLD);
    double sum_time,min_time,max_time;
    MPI_Allreduce(&v, &sum_time, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&v, &min_time, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&v, &max_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    double hit_ratio=CONF->hit/(CONF->total*1.0);
    double sum_hit_ratio,min_hit_ratio,max_hit_ratio;
    MPI_Allreduce(&hit_ratio, &sum_hit_ratio, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&hit_ratio, &min_hit_ratio, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&hit_ratio, &max_hit_ratio, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

    double mean_time = sum_time / comm_size;
    double mean_hit_ratio = sum_hit_ratio / comm_size;
    if(my_rank == 0) {
        printf("mean_time,min_time,max_time,mean_hr,min_hr,max_hr\n");
        printf("%f,%f,%f,%f,%f,%f\n",mean_time/1000,min_time/1000,max_time/1000,mean_hit_ratio,min_hit_ratio,max_hit_ratio);
    }

    hfetch::MPI_Finalize();
    //clean_env(args);
    return 0;
}
