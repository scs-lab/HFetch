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
// Created by hariharan on 3/20/19.
//

#ifndef HFETCH_DATA_MANAGER_H
#define HFETCH_DATA_MANAGER_H

#include <src/common/enumerations.h>
#include <src/common/data_structure.h>

class DataManager{
private:
    std::shared_ptr<IOClientFactory> ioFactory;
    std::shared_ptr<FileSegmentAuditor> fileSegmentAuditor;
    GlobalSequence file_id_seq;
public:
    DataManager():file_id_seq("FILE_NUM_SEQ",CONF->is_server,CONF->my_server,CONF->num_servers){
        AutoTrace trace = AutoTrace("DataManager");
        ioFactory = Singleton<IOClientFactory>::GetInstance();
        fileSegmentAuditor = Singleton<FileSegmentAuditor>::GetInstance();
    }
    ServerStatus Move(PosixFile source, PosixFile destination, bool deleteSource = true) {
        AutoTrace trace = AutoTrace("DataManager::Move",source,destination,deleteSource);
        ServerStatus status;
        if(source.GetSize()>0){
            destination.data.reserve(source.GetSize());
            status = ioFactory->GetClient(source.layer.io_client_type)->Read(source,destination);
            if(status != SERVER_SUCCESS) return status;
            status = ioFactory->GetClient(destination.layer.io_client_type)->Write(destination,destination);
            if(status != SERVER_SUCCESS) return status;
            destination.data.erase();
            if(deleteSource) status = ioFactory->GetClient(source.layer.io_client_type)->Delete(source);
        }
        return status;
    }

    ServerStatus Prefetch(PosixFile source, PosixFile destination) {
        AutoTrace trace = AutoTrace("DataManager::Prefetch",source,destination);
        return Move(source,destination,source.layer!=*Layer::LAST);
    }

    bool HasCapacity(long amount, Layer layer){
        AutoTrace trace = AutoTrace("DataManager::HasCapacity",amount,layer);
        double remaining_capacity = layer.capacity_mb_*MB-ioFactory->GetClient(layer.io_client_type)->GetCurrentUsage(layer);
        remaining_capacity = remaining_capacity<0?0:remaining_capacity;
        return remaining_capacity >= amount;
    }

    ServerStatus MakeCapacity(long amount, double score, Layer layer){
        AutoTrace trace = AutoTrace("DataManager::MakeCapacity",amount,score,layer);
        /* if layer can fit it not return error */
        if(!CanFit(amount,layer)) return ServerStatus::SERVER_FAILED;
        /* Last layer always has space */
        if(layer == *Layer::LAST) return ServerStatus::SERVER_SUCCESS;
        /* fetch layer scores */
        auto layerScore = fileSegmentAuditor->FetchLayerScores();
        auto layerScoreMap = layerScore.find(layer.id_)->second;
        /* Check what data to be moved so that space can be made */
        double remaining_capacity = layer.capacity_mb_*MB-ioFactory->GetClient(layer.io_client_type)->GetCurrentUsage(layer);
        remaining_capacity = remaining_capacity<0?0:remaining_capacity;
        long required_space = remaining_capacity;
        double this_layer_score=0;
        auto iter = layerScoreMap.end();
        while(iter!=layerScoreMap.begin() && iter->first < score && required_space <amount){
            required_space += iter->second.second.GetSize();
            this_layer_score = iter->first;
            iter--;
        }
        iter++;

        /* if layer doesnt have capacity move data to next layer to make capacity*/
        if(!HasCapacity(amount,*layer.next)){
            MakeCapacity(amount, this_layer_score, *layer.next);
        }
        /* At this point we have ensured we have space in next layer
         * Move this layer data to next layer and update MDM */
        while(iter!=layerScoreMap.end()){
            if(iter->second.second.GetSize() > required_space){
                auto orig_pieces = Split(iter->second.first,required_space);
                auto buf_pieces = Split(iter->second.second,required_space);
                PosixFile source = buf_pieces[1];
                PosixFile destination = source;
                destination.layer = *layer.next;
                destination.segment.start = 0;
                destination.segment.end = source.GetSize() - 1;
                if(source.layer != *Layer::LAST) destination.filename = GenerateBufferFilename();
                if(destination.layer == *Layer::LAST){
                    destination = iter->second.first;
                }
                Move(source,destination,source.layer != *Layer::LAST);
                fileSegmentAuditor->UpdateOnMove(orig_pieces[1],destination);
                required_space-=buf_pieces[1].GetSize();
            }else{
                PosixFile source = iter->second.second;
                PosixFile destination = iter->second.second;
                destination.layer = *layer.next;
                if(destination.layer == *Layer::LAST){
                    destination = iter->second.first;
                }
                Move(source,destination,source.layer != *Layer::LAST);
                fileSegmentAuditor->UpdateOnMove(iter->second.first,destination);
                required_space-=source.GetSize();
            }
            iter++;
        }
        return ServerStatus::SERVER_SUCCESS;
    }
    std::vector<PosixFile> Split(PosixFile file, long remaining_capacity) {
        AutoTrace trace = AutoTrace("DataManager::Split",file,remaining_capacity);
        std::vector<PosixFile> pieces=std::vector<PosixFile>();
        PosixFile p1=file;
        p1.segment.end = p1.segment.start + remaining_capacity - 1;
        PosixFile p2=file;
        p2.segment.start = p1.segment.start + remaining_capacity;
        pieces.push_back(p1);
        pieces.push_back(p2);
        return pieces;
    }
    ServerStatus CanFit(long amount, Layer layer){
        AutoTrace trace = AutoTrace("DataManager::CanFit",amount,layer);
        /* Last layer can always fit and for others check capacity*/
        return layer == *Layer::LAST || layer.capacity_mb_*MB >= amount?ServerStatus::SERVER_SUCCESS:ServerStatus::SERVER_FAILED;
    }
    CharStruct GenerateBufferFilename() {
        AutoTrace trace = AutoTrace("DataManager::GenerateBufferFilename");
        return CharStruct(std::to_string(file_id_seq.GetNextSequenceServer(0)) + ".hfetch");
    }
};
#endif //HFETCH_DATA_MANAGER_H
