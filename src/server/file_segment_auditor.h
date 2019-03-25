//
// Created by hariharan on 3/19/19.
//

#ifndef HFETCH_FILESEGMENTAUDITOR_H
#define HFETCH_FILESEGMENTAUDITOR_H


#include <src/common/distributed_ds/hashmap/DistributedHashMap.h>
#include <src/common/distributed_ds/map/DistributedMap.h>
#include <src/common/distributed_ds/multimap/DistributedMultiMap.h>
#include <src/common/distributed_ds/sequencer/global_sequence.h>
#include <src/common/configuration_manager.h>
#include <src/common/singleton.h>
#include <src/common/macros.h>
#include <src/common/data_structure.h>
#include <src/common/io_clients/io_client_factory.h>

class FileSegmentAuditor {
    /* filename to offset_map pointer*/
    typedef DistributedMap<Segment,std::pair<PosixFile,SegmentScore>> SegmentMap;
    std::unordered_map<CharStruct,std::shared_ptr<SegmentMap>> file_segment_map;
    DistributedHashMap<CharStruct, uint64_t> valid_buffered_dataset;
    std::shared_ptr<SegmentMap>* offsetMaps;
    DistributedHashMap<CharStruct,uint32_t> file_active_status;
    DistributedHashMap<uint8_t,std::multimap<double,std::pair<PosixFile,PosixFile>,std::greater<double>>> layer_score_map;
    std::shared_ptr<RPC> rpc;
    std::shared_ptr<IOClientFactory> ioFactory;
    GlobalSequence file_seq;
    const std::string FILE_SEGMENT_AUDITOR="FILE_SEGMENT_AUDITOR";

    ServerStatus CreateOffsetMap(Event event);
    ServerStatus MarkFileSegmentsActive(Event event);
    ServerStatus MarkFileSegmentsInactive(Event event);
    ServerStatus IncreaseFileSegmentFrequency(Event event);

public:
    FileSegmentAuditor():file_segment_map(),file_active_status("FILE_ACTIVE_STATUS",CONF->is_server,CONF->my_server,CONF->num_servers),
                         layer_score_map("LAYER_SCORE_MAP",CONF->is_server,CONF->my_server,CONF->num_servers),
                         file_seq("FILE_INDEX",CONF->is_server,CONF->my_server,CONF->num_servers),
                         valid_buffered_dataset("VALID_SEQ",CONF->is_server,CONF->my_server,CONF->num_servers){
        rpc=Singleton<RPC>::GetInstance("RPC_SERVER_LIST",CONF->is_server,CONF->my_server,CONF->num_servers);
        ioFactory = Singleton<IOClientFactory>::GetInstance();
        offsetMaps=new std::shared_ptr<SegmentMap>[CONF->max_num_files];
        for (int i = 0; i < CONF->max_num_files; ++i) {
            offsetMaps[i] = std::make_shared<SegmentMap>(std::to_string(i) + "_OFFSET",CONF->is_server,CONF->my_server,CONF->num_servers);
        }
        if(CONF->is_server){
            std::function<std::vector<std::pair<PosixFile,PosixFile>>(PosixFile)> getDataLocationFunc(std::bind(&FileSegmentAuditor::GetDataLocation, this, std::placeholders::_1));
            rpc->bind(FILE_SEGMENT_AUDITOR+"_GetDataLocation", getDataLocationFunc);
            MPI_Barrier(MPI_COMM_WORLD);
            if(CONF->my_rank_server == 0){
                Layer* current=Layer::FIRST;
                while(current != nullptr){
                    layer_score_map.Put(current->id_, std::multimap<double,std::pair<PosixFile,PosixFile>,std::greater<double>>());
                    current = current->next;
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    ServerStatus Update(std::vector<Event> events);

    ServerStatus UpdateOnMove(PosixFile source, PosixFile destination);

    std::vector<std::tuple<Segment,SegmentScore, PosixFile>> FetchHeatMap(PosixFile file);

    std::map<uint8_t, std::multimap<double,std::pair<PosixFile,PosixFile>,std::greater<double>>> FetchLayerScores();

    std::vector<std::pair<PosixFile,PosixFile>> GetDataLocation(PosixFile file);

    std::vector<std::pair<PosixFile,PosixFile>> GetDataLocationServer(PosixFile file,uint16_t server);

    bool CheckIfFileActive(PosixFile file);

    ServerStatus CallCreateOffsetRPC(uint16_t server, Event event);
};


#endif //HFETCH_FILESEGMENTAUDITOR_H
