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
// Created by HariharanDevarajan on 2/1/2019.
//

#ifndef HERMES_PROJECT_DISTRIBUTEDPRIORITYQUEUE_H
#define HERMES_PROJECT_DISTRIBUTEDPRIORITYQUEUE_H

/**
 * Include Headers
 */
/** Standard C++ Headers**/
#include <iostream>
#include <functional>
#include <utility>
/** MPI Headers**/
#include <mpi.h>
/** RPC Lib Headers**/
#include "../../../../external/rpclib/include/rpc/server.h"
#include "../../../../external/rpclib/include/rpc/client.h"
/** Boost Headers **/
#include <boost/interprocess/managed_shared_memory.hpp>
#include <queue>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/algorithm/string.hpp>
#include <src/common/constants.h>
#include <src/common/distributed_ds/communication/rpc_lib.h>
#include <src/common/singleton.h>
#include <src/common/macros.h>

/** Namespaces Uses **/
namespace bip=boost::interprocess;

/** Global Typedefs **/
typedef unsigned long long int really_long;

/**
 * This is a Distributed HashMap Class. It uses shared memory + RPC + MPI to achieve the data structure.
 *
 * @tparam MappedType, the value of the HashMap
 */
template<typename MappedType,typename Compare=std::less<MappedType>>
class DistributedPriorityQueue {
private:
    /** Class Typedefs for ease of use **/
    typedef bip::allocator<MappedType, bip::managed_shared_memory::segment_manager> ShmemAllocator;
    typedef std::priority_queue<MappedType, std::vector<MappedType,ShmemAllocator>,Compare> Queue;

    /** Class attributes**/
    int comm_size, my_rank,num_servers;
    uint16_t  my_server;
    std::shared_ptr<RPC> rpc;
    really_long memory_allocated;
    bool is_server;
    boost::interprocess::managed_shared_memory segment;
    std::string name,func_prefix;
    Queue *queue;
    boost::interprocess::interprocess_mutex* mutex;
public:
    /* Constructor to deallocate the shared memory*/
    ~DistributedPriorityQueue(){
        if(is_server) bip::shared_memory_object::remove(name.c_str());
    }

    explicit DistributedPriorityQueue(std::string name_,
                                      bool is_server_,
                                      uint16_t my_server_,
                                      int num_servers_)
            : is_server(is_server_), my_server(my_server_), num_servers(num_servers_),
              comm_size(1), my_rank(0), memory_allocated(1024ULL * 1024ULL * 128ULL), name(name_), segment(),
              queue(),func_prefix(name_){
        AutoTrace trace = AutoTrace("DistributedPriorityQueue",name_,is_server_,my_server_,num_servers_);
        /* Initialize MPI rank and size of world */
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
        /* create per server name for shared memory. Needed if multiple servers are spawned on one node*/
        this->name += "_" + std::to_string(my_server);
        /* if current rank is a server */
        rpc=Singleton<RPC>::GetInstance("RPC_SERVER_LIST",is_server_,my_server_,num_servers_);
        if (is_server) {
            /* Delete existing instance of shared memory space*/
            bip::shared_memory_object::remove(name.c_str());
            /* allocate new shared memory space */
            segment=bip::managed_shared_memory(bip::create_only,name.c_str(),memory_allocated);
            ShmemAllocator alloc_inst (segment.get_segment_manager());
            /* Construct Hashmap in the shared memory space. */
            queue = segment.construct<Queue>("Queue")(Compare(),alloc_inst);
            mutex = segment.construct<bip::interprocess_mutex>("mtx")();
            /* Create a RPC server and map the methods to it. */
            rpc=Singleton<RPC>::GetInstance();
            std::function<bool(MappedType,uint16_t)> pushFunc(std::bind(&DistributedPriorityQueue<MappedType,Compare>::Push, this, std::placeholders::_1, std::placeholders::_2));
            std::function<std::pair<bool,MappedType>(uint16_t)> popFunc(std::bind(&DistributedPriorityQueue<MappedType,Compare>::Pop, this, std::placeholders::_1));
            std::function<std::pair<bool,MappedType>(uint16_t)> topFunc(std::bind(&DistributedPriorityQueue<MappedType,Compare>::Top, this, std::placeholders::_1));
            std::function<size_t(uint16_t)> sizeFunc(std::bind(&DistributedPriorityQueue<MappedType,Compare>::Size, this, std::placeholders::_1));
            rpc->bind(func_prefix+"_Push", pushFunc);
            rpc->bind(func_prefix+"_Pop", popFunc);
            rpc->bind(func_prefix+"_Top", topFunc);
            rpc->bind(func_prefix+"_Size", sizeFunc);
        }
        /* Make clients wait untill all servers reach here*/
        MPI_Barrier(MPI_COMM_WORLD);
        /* Map the clients to their respective memory pools */
        if(!is_server){
            segment=bip::managed_shared_memory(bip::open_only,name.c_str());
            std::pair<Queue*, bip::managed_shared_memory::size_type> res;
            res = segment.find<Queue> ("Queue");
            queue = res.first;
            std::pair<bip::interprocess_mutex *, bip::managed_shared_memory::size_type> res2;
            res2 = segment.find<bip::interprocess_mutex>("mtx");
            mutex = res2.first;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    /**
     * Push the data into the queue. Uses key to decide the server to hash it to,
     * @param key, the key for put
     * @param data, the value for put
     * @return bool, true if Put was successful else false.
     */
    bool Push(MappedType data, uint16_t key_int){
        if(key_int == my_server){
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Push(local)",data,key_int);
            bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
            queue->push(data);
            return true;
        }else{
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Push(remote)",data,key_int);
            return rpc->call(key_int,func_prefix+"_Push", data).template as<bool>();
        }
    }
    /**
     * Get the data from the queue. Uses key_int to decide the server to hash it to,
     * @param key_int, key_int to know which server
     * @return return a pair of bool and Value. If bool is true then data was found
     *          and is present in value part else bool is set to false
     */
    std::pair<bool,MappedType> Pop(uint16_t key_int) {
        if (key_int == my_server) {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Pop(local)",key_int);
            bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
            if(queue->size()>0){
                MappedType value= queue->top();
                queue->pop();
                return std::pair<bool,MappedType>(true,value);
            }
            return std::pair<bool,MappedType>(false,MappedType());;
        } else {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Pop(remote)",key_int);
            return rpc->call(key_int,func_prefix+"_Pop").template as<std::pair<bool, MappedType>>();
        }
    }

    /**
     * Get the data from the queue. Uses key_int to decide the server to hash it to,
     * @param key_int, key_int to know which server
     * @return return a pair of bool and Value. If bool is true then data was found
     *          and is present in value part else bool is set to false
     */
    std::pair<bool,MappedType> Top(uint16_t key_int) {
        if (key_int == my_server) {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Top(local)",key_int);
            bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
            if(queue->size()>0){
                MappedType value= queue->top();
                return std::pair<bool,MappedType>(true,value);
            }
            return std::pair<bool,MappedType>(false,MappedType());;
        } else {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Top(remote)",key_int);
            return rpc->call(key_int,func_prefix+"_Pop").template as<std::pair<bool, MappedType>>();
        }
    }

    /**
     * Get the size of the queue. Uses key_int to decide the server to hash it to,
     * @param key_int, key_int to know which server
     * @return return a size of the queue
     */
    size_t Size(uint16_t key_int) {
        if (key_int == my_server) {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Size(local)",key_int);
            bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
            size_t value= queue->size();
            return value;
        } else {
            AutoTrace trace = AutoTrace("DistributedPriorityQueue::Top(remote)",key_int);
            return rpc->call(key_int,func_prefix+"_Size").template as<size_t>();;
        }
    }

};

#endif //HERMES_PROJECT_DISTRIBUTEDPRIORITYQUEUE_H
